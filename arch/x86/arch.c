_Noreturn void arch_halt(void) {
    __asm__ volatile ("hlt");
}
