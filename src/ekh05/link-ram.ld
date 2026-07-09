ENTRY(vector);

MEMORY {
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 768k /* SRAM1+2+3, contiguous */
}

SECTIONS {
    .text : {
        KEEP(*(.vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > sram

    .data : {
        *(.data*)
        . = ALIGN(4);
    } > sram

    .bss (NOLOAD) : {
        *(.bss*)
        . = ALIGN(4);
    } > sram

    heap_start = .;

    .stack (NOLOAD) : {
        . = ORIGIN(sram) + LENGTH(sram);
        stack_top = .;
    } > sram

    bss_start = ADDR(.bss);
    bss_end = bss_start + SIZEOF(.bss);
    data_start = ADDR(.data);
    data_end = data_start + SIZEOF(.data);
    data_load = LOADADDR(.data);

    /DISCARD/ : {
        *(.ARM.exidx*)
    }
}
