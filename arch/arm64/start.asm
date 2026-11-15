.global _start
.extern earlykmain
.extern stack_top

.text
.type _start, %function
_start:
    adrp  x0, stack_top
    add   x0, x0, :lo12:stack_top
    mov   sp, x0
    mov   x29, sp
    bl    earlykmain
1:
    wfi
    b     1b

.size _start, . - _start
