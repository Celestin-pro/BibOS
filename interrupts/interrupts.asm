bits 32
global default_handler_asm
global keyboard_handler_asm
extern keyboard_handler_c
extern default_handler_c


keyboard_handler_asm:
    pushad
    
    ; Sauvegarde et alignement des segments de données
    mov ax, ds
    push eax
    
    mov ax, 0x18     ; <--- CORRECTION ICI : Le segment de données de GRUB est 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call keyboard_handler_c
    
    ; Restauration
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popad
    iretd

default_handler_asm:
    pushad                ; Sauvegarde les registres
    call default_handler_c ; Appelle le C pour afficher l'erreur
    popad
    iretd                 ; Retour d'interruption protégé