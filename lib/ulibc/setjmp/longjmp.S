[bits 64]

section .text
    global longjmp

longjmp:
    mov rbx, [rdi + 0*8]
    mov rbp, [rdi + 1*8]
    mov r12, [rdi + 2*8]
    mov r13, [rdi + 3*8]
    mov r14, [rdi + 4*8]
    mov r15, [rdi + 5*8]
    mov rsp, [rdi + 6*8]
    mov rdx, [rdi + 7*8]
    mov eax, esi
    test eax, eax
    jnz .nonzero
    inc eax
.nonzero:
    jmp rdx
