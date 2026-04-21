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
    mov rsp, r12    ; RESET stack to our anchor (Fixes the 1430-tick leak)
    ; --------------------------------

    ; Restore Segments
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    popaq
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

[SECTION .rodata]
[global isr_ptr_table]
isr_ptr_table:
%assign i 0
%rep 256
    dq isr%+i       ; Table of pointers for idt_init()
    %assign i i+1
%endrep

