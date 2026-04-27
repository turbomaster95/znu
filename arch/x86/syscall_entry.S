[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:8], rsp     ; Save user stack to scratch
    mov rsp, [gs:0]     ; Load kernel stack
    
    mov [gs:16], rdx
    
    push rax
    push r11            ; Saved RFLAGS
    push rcx            ; Saved RIP
    push rbp
    push rbx
    push rdi
    push rsi

    ; Pass arguments to: void syscall_handler(uint64_t num, uint64_t arg1, ...)
    ; In SysV ABI: RDI, RSI, RDX, RCX, R8, R9
    mov rdi, [rsp + 48] ; Syscall Number
    mov rsi, [rsp + 8] ; User RDI ('Z')
    mov rdx, [rsp]  ; User RSI
    mov rcx, [gs:16]

    call syscall_handler

    ; Restore registers
    pop rsi             ; Was pushed 7th
    pop rdi             ; Was pushed 6th
    pop rbx             ; Was pushed 5th
    pop rbp             ; Was pushed 4th
    pop rcx             ; Was pushed 3rd (User RIP - CRITICAL)
    pop r11             ; Was pushed 2nd (User RFLAGS)

    add rsp, 8

    mov rsp, [gs:8]     ; Restore user stack
    swapgs
    o64 sysret
