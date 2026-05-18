.intel_syntax noprefix
.global _znu_start

_znu_start:
    xor rbp, rbp
    push 0
    push 0
    push 0
    push 0
    push 0
    xor rax, rax
    jmp _start
