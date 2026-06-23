ENTRY(_start)

SECTIONS
{
    /* GRUB charge l'OS à partir d'1 Mo en mémoire RAM */
    . = 1M;

    /* En premier, on met l'en-tête Multiboot qui est dans boot.asm */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Ensuite les variables globales en lecture seule */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Ensuite les données initialisées (comme ton IDT et ta Keymap) */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* En dernier, la pile et les données non-initialisées */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        . = ALIGN(4K);
        _kernel_end = .;     /* adresse physique de fin du kernel */
    }
}