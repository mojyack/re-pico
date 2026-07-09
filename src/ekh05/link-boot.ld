ENTRY(vector);

MEMORY {
    flash(rx) : ORIGIN = 0x08000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x200b8000, LENGTH = 32k /* top 32k of SRAM1+2+3 */
}

SECTIONS {
    .text : {
        KEEP(*(.vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > flash

    .data : {
        *(.data*)
        . = ALIGN(4);
    } > sram AT > flash

    .bss (NOLOAD) : {
        *(.bss*)
        . = ALIGN(4);
    } > sram

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
