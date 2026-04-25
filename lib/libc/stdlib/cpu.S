[BITS 64]

global wrmsr
wrmsr:
    ; rdi = msr index
    ; rsi = value low (32-bit)
    ; rdx = value high (32-bit)
    mov rcx, rdi
    mov rax, rsi
    ; rdx is already rdx
    wrmsr
    ret

global rdmsr
rdmsr:
    ; rdi = msr index
    mov rcx, rdi
    rdmsr
    ; low 32 bits in eax, high 32 bits in edx
    shl rdx, 32
    or rax, rdx
    ret

