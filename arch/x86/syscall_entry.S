[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    push rax
    push rdx
    mov dx, 0xe9
    mov al, '!'
    out dx, al
    pop rdx
    pop rax

    swapgs                  ; Swap User GS with Kernel GS
    mov [gs:0x10], rsp      ; Save User RSP in a scratch area (in TSS or GS)
    mov rsp, [gs:0x00]      ; Load Kernel RSP

    ; Save user state (except RCX and R11 which are handled by sysret)
    push r11 ; User RFLAGS
    push rcx ; User RIP
    push rbp
    push rdi ; Syscall Arg 1
    push rsi ; Syscall Arg 2
    push rdx ; Syscall Arg 3
    ; ... save others if needed ...

    ; Call the C handler
    ; RAX = syscall number, RDI = Arg 1, RSI = Arg 2, RDX = Arg 3
    call syscall_handler

    ; Restore user state
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop rcx
    pop r11

    mov rsp, [gs:0x10]      ; Restore User RSP
    swapgs
    o64 sysret

