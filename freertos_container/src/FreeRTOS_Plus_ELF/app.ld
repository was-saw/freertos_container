ENTRY(main)

PHDRS
{
    text PT_LOAD FILEHDR PHDRS;
}

SECTIONS
{
    . = SIZEOF_HEADERS;
    
    .text : {
        *(.text.main)
        *(.text)
        *(.text.*)
    } :text
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text
    
    .data : {
        *(.data)
        *(.data.*)
    } :text
    
    .got : {
        *(.got)
    } :text
    
    .got.plt : {
        *(.got.plt)
    } :text
    
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
}
