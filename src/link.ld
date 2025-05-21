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

    .ARM.exidx : {
        *(.ARM.exidx*)
    } > flash

    .stack (NOLOAD) : {
        . = ORIGIN(sram) + LENGTH(sram);
        stack_top = .; 
    } > sram
}
