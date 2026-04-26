[BITS 64]
section .text
global _start

_start:
    ; Syscall 1: Print 'Z'
    mov rax, 1          
    mov rdi, 'Z'        
    syscall             ; After this, rax should be 0x42

    ; Syscall 2: Print the return value we just got
    mov rdi, rax        ; Move return value (0x42) into rdi
    mov rax, 2          ; Call our new "print hex" syscall
    syscall

.loop:
    pause
    jmp .loop

