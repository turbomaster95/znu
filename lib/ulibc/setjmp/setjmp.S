[bits 64]

section .text
    global setjmp

setjmp:
    mov [rdi + 0*8], rbx
    mov [rdi + 1*8], rbp
    mov [rdi + 2*8], r12
    mov [rdi + 3*8], r13
    mov [rdi + 4*8], r14
    mov [rdi + 5*8], r15
    lea rax, [rsp + 8]
    mov [rdi + 6*8], rax
    mov rax, [rsp]
    mov [rdi + 7*8], rax
    xor eax, eax
    ret
