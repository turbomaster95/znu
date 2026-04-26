[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs

    ; Corrected offset: 0x08 matches cpu_context_t.user_stack_scratch
    mov [gs:0x08], rsp
    mov rsp, [gs:0x00]

    push rax
    push r11
    push rcx
    push rdx
    push rsi
    push rdi

    mov dx, 0xe9
    mov al, '!'
    out dx, al

    ; System V ABI mapping: 
    ; Arg 5 needs to be in R8 (User RFLAGS from R11)
    ; Arg 6 needs to be in R9 (Syscall num from RAX)
    mov r8, r11
    mov r9, rax
    call syscall_handler

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r11
    pop rax

    ; Corrected offset
    mov rsp, [gs:0x08]
    swapgs

    o64 sysret

