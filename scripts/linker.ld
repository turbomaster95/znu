/* Tell the linker that we want an x86_64 ELF64 output file */
OUTPUT_FORMAT(elf64-x86-64)

/* We want the symbol _start to be our entry point */
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    /* We want to be placed in the topmost 2GiB of the address space, for limine*/
    . = 0xffffffff80000000;
    _kernel_virtual_start = .;

    /* Define a section to contain the Limine requests and assign it to its own PHDR */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    /* Move to the next memory page for .text */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text : {
        *(.text .text.*)
    } :text

    /* Move to the next memory page for .rodata */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(8);
        PROVIDE(legal_start = .);
        KEEP(*(.legal))
        PROVIDE(legal_end = .);
    } :rodata

    /* Move to the next memory page for .data */
    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        stack_bottom = .;
        . += 0x4000; /* 16 KiB stack */
        stack_top = .;
    } :data

    /* Move completely clear of the .bss tracking frame */
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    
    .ksyms : {
        __ksyms_start = .;
        *(.ksyms)
        QUAD(0); /* Forces the section type to initialized PROGBITS data */
        . = ALIGN(0x1000);
        . += 0x20000;
        __ksyms_end = .;
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .initcalls : ALIGN(8) {
	__initcall_start = .;

	__initcall0_start = .;
        KEEP(*(.initcall0.init))
        __initcall1_start = .;
        KEEP(*(.initcall1.init))
        __initcall2_start = .;
        KEEP(*(.initcall2.init))
        __initcall3_start = .;
        KEEP(*(.initcall3.init))
        __initcall4_start = .;
        KEEP(*(.initcall4.init))
        __initcall5_start = .;
        KEEP(*(.initcall5.init))
        __initcall6_start = .;
        KEEP(*(.initcall6.init))
        __initcall7_start = .;
        KEEP(*(.initcall7.init))

	__initcall_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note.gnu*)
    }

    _kernel_virtual_end = .;
}
