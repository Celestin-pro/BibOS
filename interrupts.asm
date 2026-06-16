bits 32

; On rend la fonction accessible depuis le C
global keyboard_handler_asm
; On indique que la fonction C existe ailleurs
extern keyboard_handler_c

keyboard_handler_asm:
    pusha                    ; 1. Sauvegarde tous les registres généraux (EAX, ECX, etc.)
    push ds                  ; 2. Sauvegarde les segments de données
    push es
    push fs
    push gs

    mov ax, 0x10             ; Charge le segment de données du noyau (sécurité)
    mov ds, ax
    mov es, ax

    call keyboard_handler_c  ; 3. Appelle TON raisonnement écrit en C !

    pop gs                   ; 4. Restaure les segments dans l'ordre inverse
    pop fs
    pop es
    pop ds
    popa                     ; 5. Restaure tous les registres généraux
    
    iret                     ; 6. Instruction spéciale de retour d'interruption !