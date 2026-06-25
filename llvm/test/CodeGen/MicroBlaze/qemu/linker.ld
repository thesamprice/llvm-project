/*
 * Bare-metal linker script for petalogix-s3adsp1800 (MicroBlaze LE).
 *   RAM:  0x90000000, 128 MB
 *   UART: 0x84000000 (uartlite)
 *
 * The .text.start section (from harness.s) is placed first so that
 * _start is at the ELF entry point address 0x90000000.
 */
ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x90000000, LENGTH = 128M
}

SECTIONS {
    . = 0x90000000;

    .text : {
        KEEP(*(.text.start))   /* _start must be first */
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM
}
