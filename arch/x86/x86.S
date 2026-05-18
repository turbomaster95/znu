; Put the random funcs i need here who dont require their own .S file :)


global force_context_restore
force_context_restore:
    mov rsp, rdi    ; RSP points to the base of registers_t (regs->es)

    ; 1. Safely pull past the uninitialized es/ds structural variables
    pop rax         ; Clean out regs->es field from stack
    pop rax         ; Clean out regs->ds field from stack

    ; 2. Explicitly load the correct Ring 3 User Data Selector (0x1B)
    mov ax, 0x1B    ; Entry 3 (0x18) | RPL 3 (0x03) = 0x1B
    mov ds, ax
    mov es, ax

    ; 3. Pop general purpose registers in structural order
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
    pop rax         ; Restores the true user space RAX register value

    ; 4. Skip int_no and err_code (16 bytes total)
    add rsp, 16

    ; 5. Bounce straight back into user-space via IRETQ
    iretq
