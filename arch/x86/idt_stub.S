[BITS 64]
[SECTION .text]

extern k_exception_handler

%define KERNEL_DATA_SEG 0x10

; --- Register Save/Restore ---
; Exact Size: 15 registers * 8 bytes = 120 bytes
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

    ; --- CRITICAL CORRECTION: USER-MODE CHECK ---
    ; Layout on stack at this point:
    ; [rsp + 0]   to [rsp + 112] -> Saved 15 Registers from pushaq (120 bytes)
    ; [rsp + 120]                -> int_no (8 bytes)
    ; [rsp + 128]                -> err_code (8 bytes)
    ; [rsp + 136]                -> RIP (8 bytes)
    ; [rsp + 144]                -> CS Selector (8 bytes) <-- EXACT TARGET
    
    test qword [rsp + 144], 3
    jz .no_swapgs
    swapgs
.no_swapgs:

    ; Save Segment Registers (64-bit push to maintain 16-byte stack alignment)
    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax

    ; Enforce clean Kernel Data Segments
    mov ax, KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax

    ; --- STACK ALIGNMENT & ANCHOR ---
    mov r12, rsp    ; Store the current structural stack top in R12
    and rsp, -16    ; Align stack pointer to 16-bytes for System V ABI compliance
    mov rdi, r12    ; Pass pointer to our structural state table to C
    cld             ; Clear direction flag for compiler safety
    call k_exception_handler
    mov rsp, rax    ; Safely transition to the stack pointer context returned by C
    ; --------------------------------

    ; Restore Segments
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    popaq

    ; --- CRITICAL CORRECTION: RETURN USER-MODE CHECK ---
    ; Layout on stack here:
    ; [rsp + 0]  -> int_no (8 bytes)
    ; [rsp + 8]  -> err_code (8 bytes)
    ; [rsp + 16] -> RIP (8 bytes)
    ; [rsp + 24] -> CS Selector (8 bytes) <-- EXACT TARGET
    
    test qword [rsp + 24], 3
    jz .no_swapgs_back
    swapgs
.no_swapgs_back:

    add rsp, 16     ; Clean off the dummy/real error code and interrupt number
    iretq

; --- ISR Wrapper Macro ---
%macro isr_wrapper 1
global isr%1
isr%1:
    ; Check if CPU already pushed an error code
    ; Real hardware error codes: 8, 10-14, 17, 21
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

[SECTION .rodata]
[global isr_ptr_table]
isr_ptr_table:
%assign i 0
%rep 256
    dq isr%+i       ; Array block of pointers mapped natively by idt_init()
    %assign i i+1
%endrep
