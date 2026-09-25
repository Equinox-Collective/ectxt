OUTPUT_FORMAT("binary")
ENTRY(_start)
SECTIONS
{
    . = 0x0100;
    .all : AT(0) {
        *(.entry)
        *(.text*)
        *(.rodata*)
        *(.data*)
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
}