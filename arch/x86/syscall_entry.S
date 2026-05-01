[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:8], rsp     ; Save user stack to scratch
    mov rsp, [gs:0]     ; Load kernel stack
    
    ; Save everything for context switching and ABI compliance
    push qword [gs:8]   ; User RSP
    push r11            ; User RFLAGS
    push rcx            ; User RIP
    push rbp
    push rbx
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdx
    push rsi
    push rdi
    push rax            ; Syscall number

    ; System V C calling convention: rdi, rsi, rdx, rcx, r8, r9
    ; User syscall convention: rax, rdi, rsi, rdx, r10, r8, r9
    
    mov rdi, rax        ; num
    mov rsi, [rsp + 8]  ; arg1 (user rdi)
    mov rdx, [rsp + 16] ; arg2 (user rsi)
    mov rcx, [rsp + 24] ; arg3 (user rdx)
    mov r8,  [rsp + 48] ; arg4 (user r10)
    mov r9,  [rsp + 32] ; arg5 (user r8)

    sub rsp, 8          ; Align stack to 16 bytes
    call syscall_handler
    add rsp, 8

    ; RAX contains return value.
    add rsp, 8          ; Skip saved rax
    pop rdi
    pop rsi
    pop rdx
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbx
    pop rbp
    pop rcx             ; User RIP
    pop r11             ; User RFLAGS
    pop rdi             ; User RSP
    
    mov rsp, rdi
    swapgs
    o64 sysret
