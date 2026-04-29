[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:8], rsp     ; Save user stack to scratch
    mov rsp, [gs:0]     ; Load kernel stack
    
    ; Save all registers for full compatibility and stack trace support
    push qword [gs:8]   ; 12 User RSP
    push r11            ; 11 User RFLAGS
    push rcx            ; 10 User RIP
    push r9             ; 9
    push r8             ; 8
    push r10            ; 7
    push rdx            ; 6
    push rsi            ; 5
    push rdi            ; 4
    push rbp            ; 3
    push rbx            ; 2
    push rax            ; 1

    ; Map arguments to C: rdi, rsi, rdx, rcx, r8, r9
    mov rdi, rax        ; num
    mov rsi, [rsp + 24] ; arg1
    mov rdx, [rsp + 32] ; arg2
    mov rcx, [rsp + 40] ; arg3
    mov r8,  [rsp + 48] ; arg4
    mov r9,  [rsp + 56] ; arg5
 
    sub rsp, 8
    call syscall_handler
    add rsp, 8

    ; RAX contains return value. Restore others.
    add rsp, 8          ; Skip saved rax
    pop rbx
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    pop rcx
    pop r11
    pop rdi             ; Restore user RSP into rdi
    
    mov rsp, rdi
    swapgs
    o64 sysret
