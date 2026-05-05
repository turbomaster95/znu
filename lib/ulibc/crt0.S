[BITS 64]
default rel
section .text
global _start
extern main
extern environ

_start:
    xor rbp, rbp
    
    ; System V ABI says:
    ; [rsp] = argc
    ; [rsp + 8] = argv[0]
    ; ...
    ; [rsp + 8*argc + 8] = NULL
    ; [rsp + 8*argc + 16] = envp[0]
    
    mov rdi, [rsp]          ; argc
    lea rsi, [rsp + 8]      ; argv
    lea rdx, [rsp + rdi*8 + 16] ; envp
    
    ; Set global environ variable
    mov [environ], rdx
    
    ; Align stack to 16 bytes before calling main
    and rsp, -16
    
    call main
    
    mov rdi, rax
    mov rax, 60
    syscall

.loop:
    hlt
    jmp .loop
