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
    note PT_NOTE;
}

SECTIONS
{
    /* Map picky rump symbols into lower 32-bit address space */
    . = 0x1000000; 

    .rump_low_text : {
        *(.text.*rumpns_* .text.*rump_*)
        /* Updated backup archive file catchers */
        *librumpy.a*(.text .text.*)
        *built-in.a*(.text.*rump*)  /* <-- Updated to .a */
    }

    .rump_low_data : {
        *(.rodata.*rumpns_* .rodata.*rump_*)
        *(.data.*rumpns_* .data.*rump_*)
        *(.bss.*rumpns_* .bss.*rump_*)
        /* Updated backup archive file catchers */
        *librumpy.a*(.rodata .rodata.* .data .data.* .bss .bss.*)
        *built-in.a*(.rodata.*rump* .data.*rump* .bss.*rump*) /* <-- Updated to .a */
    }

    .link_set_rump_components : {
        __start_link_set_rump_components = .;
        *(link_set_rump_components)
        __stop_link_set_rump_components = .;
    }

    /* We want to be placed in the topmost 2GiB of the address space, for optimisations */
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

    .note.znu : {
        KEEP(*(.note.znu))
    } :rodata :note
    
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
    
    /* * FIXED: Explicitly allocate space inside the curly braces 
     * and force SHT_PROGBITS so it takes real space in the file map.
     */
    .ksyms : {
        __ksyms_start = .;
        *(.ksyms)
        QUAD(0); /* Forces the section type to initialized PROGBITS data */
        . = ALIGN(0x1000);
        . += 0x20000; /* Allocate your full symbol table tracking buffer window */
        __ksyms_end = .;
    } :data

    /* Discard .note.* and .eh_frame* safely without stepping on .note.znu */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note.gnu*)
    }

    _kernel_virtual_end = .;
}
