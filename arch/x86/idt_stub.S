[BITS 64]
[SECTION .text]

extern k_exception_handler

; Limine usually maps data to 0x30, but if you use your own GDT
; as we discussed, this should match your Data Segment (usually 0x10)
%define KERNEL_DATA_SEG 0x10

; --- Register Save/Restore ---
%macro pushaq 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro

[global asm_lidt]
asm_lidt:
    lidt [rdi]
    ret

; --- Uniform ISR Entry Point ---
isr_stub:
    pushaq

    ; Check if we came from user-mode (Ring 3)
    ; At this point, stack has: int_no, err_code, rip, cs, rflags, rsp, ss
    ; CS is at [rsp + 144] (15 regs from pushaq + 2 from wrappers)
    test qword [rsp + 144], 3
    jz .no_swapgs
    swapgs
.no_swapgs:

    ; Save Segment Registers (64-bit push to keep alignment)
    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax

    ; Load Kernel Data Segments
    mov ax, KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax

    ; --- STACK ALIGNMENT & ANCHOR ---
    mov r12, rsp    ; Store the current "clean" stack top in R12
    and rsp, -16    ; Align to 16-bytes for System V ABI
    mov rdi, r12    ; Pass pointer to our registers_t struct
    cld             ; Ensure direction flag is clear for C
    call k_exception_handler
    mov rsp, rax    ; rax contains the new stack pointer returned by k_exception_handler
    ; --------------------------------

    ; Restore Segments
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    popaq

    ; Conditionally swapgs back if we're returning to user-mode
    ; Stack now has: int_no, err_code, rip, cs, ...
    ; CS is at [rsp + 24]
    test qword [rsp + 24], 3
    jz .no_swapgs_back
    swapgs
.no_swapgs_back:

    add rsp, 16     ; Pops the 'int_no' and 'err_code' pushed by wrappers
    iretq

; --- ISR Wrapper Macro ---
%macro isr_wrapper 1
global isr%1
isr%1:
    ; Check if CPU already pushed an error code
    ; Errors: 8, 10-14, 17, 21
    %if !(%1 == 8 || (%1 >= 10 && %1 <= 14) || %1 == 17 || %1 == 21)
        push 0      ; Push dummy error code
    %endif
    push %1         ; Push interrupt number
    jmp isr_stub
%endmacro

; --- Generate all 256 Entry Points ---
%assign i 0
%rep 256
    isr_wrapper i
    %assign i i+1
%endrep

global nmi_panic_handler_stub
extern nmi_panic_handler_c

COM1_BASE   equ 0x3F8
COM1_LSR    equ COM1_BASE + 5   ; Line Status Register (0x3FD)

nmi_panic_handler_stub:
    cli                         ; Explicitly block any maskable interrupts
    cld                         ; Clear direction flag for standard C call safety

    ; ========================================================
    ; HARDWARE SERIAL OUTPUT: TRANSMIT "--- AP CRASHED ---"
    ; ========================================================
    
    ; Print '\n'
    mov al, 10
    call .serial_send_char
    
    ; Print '!'
    mov al, '!'
    call .serial_send_char

    ; Print 'A'
    mov al, 'A'
    call .serial_send_char

    ; Print 'P'
    mov al, 'P'
    call .serial_send_char

    ; Print '-'
    mov al, '-'
    call .serial_send_char

    ; Print 'F'
    mov al, 'F'
    call .serial_send_char

    ; Print 'R'
    mov al, 'R'
    call .serial_send_char

    ; Print 'O'
    mov al, 'O'
    call .serial_send_char

    ; Print 'Z'
    mov al, 'Z'
    call .serial_send_char

    ; Print 'E'
    mov al, 'E'
    call .serial_send_char

    ; Print '\n'
    mov al, 10
    call .serial_send_char

    ; Optional: Jump out to your C code if you want to try printing registers
    call nmi_panic_handler_c

.halt_loop:
    hlt                         ; Safely park the pipeline hardware
    jmp .halt_loop              ; Inescapable fence loop

; --- Internal Assembly Helper Routine ---
.serial_send_char:
    push rbx                    ; Preserve working registers
.wait_clear:
    in al, COM1_LSR             ; Read Line Status Register from 0x3FD
    and al, 0x20                ; Mask out Bit 5 (Transmitter Holding Register Empty)
    jz .wait_clear              ; If bit is 0, buffer is full -> loop and spin
    
    pop rbx                     ; Restore working register
    out COM1_BASE, al           ; Transmit character safely out over COM1
    ret

[SECTION .rodata]
[global isr_ptr_table]
isr_ptr_table:
%assign i 0
%rep 256
    dq isr%+i       ; Table of pointers for idt_init()
    %assign i i+1
%endrep
