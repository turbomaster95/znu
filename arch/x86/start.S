[BITS 64]
extern kmain
global _start
global stack_top
global stack_bottom

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .text
_start:
    mov rsp, stack_top  ; Set the stack pointer
    mov rbp, rsp
    call kmain
    hlt

