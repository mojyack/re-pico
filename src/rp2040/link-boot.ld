ENTRY(boot_stage2);

MEMORY {
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x2003c000, LENGTH = 16k /* top 16k; fw image + comp buffer use the rest */
}

SECTIONS {
    .boot2 : {
        *(.boot2)
        . = ORIGIN(flash) + 252;
        KEEP(*(.crc))
    } > flash

    .text : {
        KEEP(*(.vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > flash

    .bss (NOLOAD) : {
        *(.bss*)
        . = ALIGN(4);
    } > sram

    .data : {
        *(.data*)
        . = ALIGN(4);
    } > sram AT > flash

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
