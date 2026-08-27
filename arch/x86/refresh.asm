[bits 64]
global _start

_start:
    mov rax, 60     ; sys_exit
    xor rdi, rdi    ; exit code 0
    syscall

section .note.GNU-stack noexec alloc nobits
