[bits 64]
global _start

_start:
    mov rax, 60     ; sys_exit
    xor rdi, rdi    ; exit code 0
    syscall
