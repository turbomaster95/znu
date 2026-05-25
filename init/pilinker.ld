ENTRY(_start)

PHDRS
{
    headers PT_PHDR PHDRS;
    text    PT_LOAD FILEHDR PHDRS;
    dynamic PT_DYNAMIC;
}

SECTIONS
{
    . = 0x1000 + SIZEOF_HEADERS;

    .text : { *(.text) } :text
    .rodata : { *(.rodata) }
    
    .dynamic : { 
        *(.dynamic) 
    } :text :dynamic
     
    .data : { *(.data) } :text
    .bss : { *(.bss) } :text

    .got : { *(.got) *(.got.plt) } :text
    .plt : { *(.plt) } :text
    .rela.dyn : { *(.rela.dyn) } :text
}
