default rel
[BITS 64]
global syscall_entry
extern syscall_handler
extern scheduler

syscall_entry:
    swapgs
    mov [gs:8], rsp
    mov rsp, [gs:0]

    push qword 0x1B         ; ss (User Data Segment)
    push qword [gs:8]       ; rsp (Original User RSP)
    push r11                ; rflags (sysret caches user flags in r11)
    push qword 0x23         ; cs (User Code Segment)
    push rcx                ; rip (sysret caches user instruction pointer in rcx)
    
    push qword 0            ; err_code
    push qword 0            ; int_no
    
    ; Push general purpose registers
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
    
    ; Push data segments
    xor rax, rax
    mov ax, ds
    push rax
    mov ax, es
    push rax

    mov rdi, rsp

    mov rbp, rsp
    and rsp, -16
    
    call syscall_handler
    
    mov rsp, rbp

    cmp rax, 1
    je .block_current_process

    cmp rax, rsp
    jne .switch_to_new_context

.return_to_user:
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

    add rsp, 16             ; Skip int_no and err_code (2 * 8 bytes)
    
    pop rcx                 ; Pop saved RIP into rcx for sysret
    add rsp, 8              ; Skip CS
    pop r11                 ; Pop saved RFLAGS into r11 for sysret
    pop rdi                 ; Pop saved User RSP into rdi temporarily
    add rsp, 8              ; Skip SS
    
    mov rsp, rdi            ; Restore user stack pointer
    
    swapgs                  ; Swap back to user-space GS
    o64 sysret              ; Return to Ring 3

.block_current_process:
    mov rdi, rsp
    call scheduler          ; Returns the registers_t* of the newly elected task
    
.switch_to_new_context:
    mov rsp, rax

    extern current_process
    mov rdi, [current_process] 
    mov rsi, [rdi + 48]        
    mov [gs:0], rsi            

    jmp .return_to_user
