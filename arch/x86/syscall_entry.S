[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:8], rsp
    mov rsp, [gs:0]
    
    push qword 0x1B
    push qword [gs:8]
    push r11
    push qword 0x23
    push rcx
    
    push qword 0
    push qword 0
    
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax

    mov rdi, rsp        ; Pass pointer to registers_t
    sub rsp, 8          ; Align stack
    call syscall_handler
    add rsp, 8

    mov [rsp + 128], rax

    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    add rsp, 16
    
    pop rcx
    add rsp, 8
    pop r11
    pop rdi
    
    mov rsp, rdi
    swapgs
    o64 sysret
