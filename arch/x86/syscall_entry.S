[BITS 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    ; MARKER 1: Entry
    mov dx, 0xe9
    mov al, '1'
    out dx, al

    swapgs

    ; MARKER 2: GS Swapped
    mov al, '2'
    out dx, al

    mov [gs:8], rsp     ; Save user stack to scratch
    mov rsp, [gs:0]     ; Load kernel stack

    ; MARKER 3: Stack Swapped
    mov al, '3'
    out dx, al

    ; --- ABI ALIGNMENT FIX ---
    ; We push 8 registers total (8 * 8 = 64 bytes). 
    ; 64 is a multiple of 16, so the stack remains aligned for the C call.
    push r11            ; Saved RFLAGS
    push rcx            ; Saved RIP
    push rbp
    push rbx
    push rdi
    push rsi
    push rdx
    push rax            ; Syscall number

    ; MARKER 4: Stack Aligned & Ready
    mov al, '4'
    out dx, al

    ; Pass arguments to: void syscall_handler(uint64_t num, uint64_t arg1, ...)
    ; In SysV ABI: RDI, RSI, RDX, RCX, R8, R9
    mov rdi, rax        ; 1st param: syscall number
    
    call syscall_handler

    ; Restore registers
    pop rax
    pop rdx
    pop rsi
    pop rdi
    pop rbx
    pop rbp
    pop rcx
    pop r11

    mov rsp, [gs:8]     ; Restore user stack
    swapgs
    o64 sysret

