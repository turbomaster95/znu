[BITS 64]

section .text
global jump_to_usermode

jump_to_usermode:
    ; RDI contains the target function address (RIP)
    ; RSI contains the user stack pointer (RSP)
    
    ; 1. Disable interrupts during transition
    cli

    ; 2. Set up the stack frame for IRETQ
    ; The hardware expects: SS, RSP, RFLAGS, CS, RIP (in this order)
    
    ; Push User Data Selector (Selector 0x20 | RPL 3 = 0x23)
    mov ax, 0x23
    push rax            ; SS

    ; Push User Stack Pointer
    push rsi            ; RSP

    ; Push RFLAGS
    ; We want to ensure Bit 9 (Interrupt Flag) is set so 
    ; interrupts are re-enabled when we land in Ring 3.
    pushfq
    pop rax
    or rax, 0x3000
    or rax, 0x200       ; Set IF bit
    push rax            ; RFLAGS

    ; Push User Code Selector (Selector 0x18 | RPL 3 = 0x1B)
    mov rax, 0x1B
    push rax            ; CS

    ; Push the target RIP (from RDI)
    push rdi            ; RIP

    ; 3. Clean up general purpose registers
    ; This prevents the user process from "peeking" at kernel data left in registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor rsi, rsi
    xor rdi, rdi
    xor r8,  rax
    xor r9,  rax
    xor r10, rax
    xor r11, rax
    xor r12, rax
    xor r13, rax
    xor r14, rax
    xor r15, rax

    ; 4. Execute the transition
    iretq

