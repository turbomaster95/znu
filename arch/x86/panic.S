global nmi_panic_handler_stub
extern nmi_panic_handler_c

nmi_panic_handler_stub:
    cli
    cld

    call nmi_panic_handler_c

.halt_loop:
    hlt
    jmp .halt_loop
