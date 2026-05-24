[BITS 64]
extern earlykmain
global _start
global stack_top
global stack_bottom

section .bss
align 16
stack_bottom:
    resb 32768 ; 32 KiB
stack_top:

section .text
_start:
    mov rsp, stack_top  ; Set the stack pointer
    mov rbp, rsp
    call earlykmain
    hlt

