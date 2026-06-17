bits 32

global keyboard_handler_asm
extern keyboard_handler_c

keyboard_handler_asm:
    cli
    pusha                    ; 1. Sauvegarde les registres généraux

    push ds                  ; 2. Sauvegarde les segments actuels
    push es
    push fs
    push gs

    mov ax, 0x10             ; 3. Force le sélecteur de données du Noyau (0x10 est la norme GRUB/Multiboot)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call keyboard_handler_c  ; 4. Appelle ton code C dans driver.c

    pop gs                   ; 5. Restaure les segments
    pop fs
    pop es
    pop ds

    popa                     ; 6. Restaure les registres généraux
    sti                      ; 7. Réactive les interruptions
    iret                     ; 7. Retour d'interruption et réactivation des IRQ