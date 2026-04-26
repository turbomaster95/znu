[BITS 64]
section .text
global _start

_start:
    xor rax, rax
    mov rax, 1          ; Clear rax and set to 1
    mov rdi, 90         ; ASCII 'Z'
    syscall
.loop:
    pause               ; Better for the CPU than a raw jmp
    jmp .loop

