[BITS 64]
default rel
section .text
global _start
extern main
extern exit
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
    
    ; This calls the exit handler in which in turn runs the atexit handlers and then exits cleanly
    mov rdi, rax
    call exit

.loop:
    hlt
    jmp .loop
