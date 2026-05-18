; Put the random funcs i need here who dont require their own .S file :)

global force_context_restore
extern current_process

force_context_restore:
    mov rsp, rdi    

    mov rax, [current_process]
    mov rdx, [rax + 48]       
    mov [gs:0], rdx           

    pop rax         
    pop rax         

    mov ax, 0x1B    ; Entry 3 (0x18) | RPL 3 (0x03) = 0x1B
    mov ds, ax
    mov es, ax

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

    swapgs

    iretq
