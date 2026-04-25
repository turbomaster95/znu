[BITS 64]
section .text
global _start

_start:
    mov rax, 1          ; Syscall #1: print_char
    mov rdi, 'Z'        ; Argument 1: The character to print
    syscall             ; Trigger the kernel!

.loop:
    pause
    jmp .loop

