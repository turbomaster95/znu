.global _start
.global stack_top
.global stack_bottom
.extern earlykmain

.bss
.align 4
stack_bottom:
.space 32768
stack_top:

.text
_start:
    adrp  x0, stack_top
    add   x0, x0, :lo12:stack_top
    mov   sp, x0
    mov   x29, sp
    bl    earlykmain
    wfi

    .size _start, . - _start   
