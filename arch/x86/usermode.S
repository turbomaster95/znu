[BITS 64]

section .text
global jump_to_usermode

jump_to_usermode:
    cli

    push 0x1B           ; SS = User Data Selector | RPL 3
    push rsi            ; RSP = user stack top
    pushfq
    pop rax
    or rax, 0x200       ; Ensure Interrupt Flag is set
    push rax            ; RFLAGS
    push 0x23           ; CS = User Code Selector | RPL 3
    push rdi            ; RIP = entry point

    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax

    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor rsi, rsi
    xor rdi, rdi
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    swapgs

    iretq

