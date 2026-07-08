ENTRY(boot_stage2);

MEMORY {
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS {
    .boot2 : {
        *(.boot2)
        . = ORIGIN(flash) + 252;
        *(.crc)
    } > flash

    .text : {
        *(.vector)
        *(.text*)
    } > flash

    .bss : {
        *(.bss*)
    } > sram

    .data : {
        *(.data*)
    } > sram AT > flash

    heap_start = .;

    .stack (NOLOAD) : {
        . = ORIGIN(sram) + LENGTH(sram);
        stack_top = .; 
    } > sram

    bss_start = ADDR(.bss);
    bss_end = data_start + SIZEOF(.bss);
    data_start = ADDR(.data);
    data_end = data_start + SIZEOF(.data);
    data_load = LOADADDR(.data);

    /DISCARD/ : {
        *(.ARM.exidx*)
    }
}
