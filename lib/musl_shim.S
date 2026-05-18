.intel_syntax noprefix
.global _start

.extern main
.extern exit

.section .text

_start:
    xor rbp, rbp

    mov rdi, [rsp]        # 1st Argument (RDI) = argc
    lea rsi, [rsp + 8]    # 2nd Argument (RSI) = argv array pointer

    and rsp, -16

    call main

    mov rdi, rax          # 1st Argument (RDI) = exit status
    call exit

1:  hlt
    jmp 1b
