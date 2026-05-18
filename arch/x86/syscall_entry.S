[BITS 64]
global syscall_entry
extern syscall_handler
extern scheduler

syscall_entry:
    swapgs
    mov [gs:8], rsp         ; Save User RSP to scratch space
    mov rsp, [gs:0]         ; Load Kernel RSP for this thread

    ; -----------------------------------------------------
    ; 1. Build registers_t context frame on kernel stack
    ; -----------------------------------------------------
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

    ; -----------------------------------------------------
    ; 2. Call C Handler
    ; -----------------------------------------------------
    mov rdi, rsp            ; 1st argument: pointer to registers_t

    ; Align the stack to 16-bytes (System V ABI requirement)
    mov rbp, rsp            ; Save our true stack frame pointer
    and rsp, -16            ; Align down to 16 bytes
    
    call syscall_handler    ; Returns: registers_t* OR 1 (block signal)
    
    mov rsp, rbp            ; Restore exact stack pointer so pops align

    ; -----------------------------------------------------
    ; 3. Evaluate Task State (The Architecture Fix)
    ; -----------------------------------------------------
    cmp rax, 1
    je .block_current_process

    cmp rax, rsp
    jne .switch_to_new_context

.return_to_user:
    ; Normal return path. Execution stays in the current process.
    ; (The C code should have placed any return values into regs->rax)

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

; -----------------------------------------------------
; Task Switching Subroutines
; -----------------------------------------------------
.block_current_process:
    ; This process yielded (e.g., waiting for a child).
    ; We pass the current stack frame pointer to the scheduler so it can
    ; save it to current_process->context_ptr and elect a new task.
    mov rdi, rsp
    call scheduler          ; Returns the registers_t* of the newly elected task
    
    ; Fall through to switch context...

.switch_to_new_context:
    ; RAX holds the kernel stack context pointer of the newly scheduled process
    mov rsp, rax            ; Switch hardware stack directly to the new task

    ; NOTE: If your system isolates kernel stacks per process, ensure your C-side 
    ; scheduler() updates the active TSS.rsp0 or GS:0 so the next time this new 
    ; process calls a syscall, it uses its own kernel stack top!

    jmp .return_to_user
