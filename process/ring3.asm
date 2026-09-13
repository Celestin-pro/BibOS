bits 32
section .text

; -----------------------------------------------------------------------
; enter_ring3_asm(unsigned int eip, unsigned int esp_user)
;
;   Charge les registres de segment user, construit le frame iretd et
;   bascule le CPU en Ring 3. Ne retourne jamais.
;
;   Frame iretd (du haut vers le bas, dans l'ordre de dépilage) :
;     EIP    → point d'entrée user
;     CS     → 0x1B  (GDT[3] | RPL=3, user code)
;     EFLAGS → EFLAGS courant | IF=1 (interruptions activées dès Ring 3)
;     ESP    → stack user
;     SS     → 0x23  (GDT[4] | RPL=3, user data)
;   Les 2 derniers ne sont dépilés que lors d'un changement de privilège
;   (Ring 0 → Ring 3), ce qui est précisément notre cas.
;
;   Avant le iretd, DS/ES/FS/GS sont chargés avec le sélecteur user data
;   (0x23) pour que le code user trouve ses segments correctement configurés.
; -----------------------------------------------------------------------
global enter_ring3_asm
enter_ring3_asm:
    mov eax, [esp + 4]      ; eip user
    mov ecx, [esp + 8]      ; esp user

    ; Charger les registres de segment data avec le sélecteur user (DPL=3).
    ; CS sera mis à jour par iretd.
    mov dx, 0x23            ; SEG_USER_DATA = GDT[4] | RPL=3
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ; Construire le frame iretd sur la stack kernel courante.
    push dword 0x23         ; SS   : user data
    push ecx                ; ESP  : stack user
    pushfd                  ; EFLAGS courant (IF déjà à 1 en kernel)
    or dword [esp], 0x200   ; forcer IF=1 (au cas où cli aurait été appelé)
    push dword 0x1B         ; CS   : user code (GDT[3] | RPL=3)
    push eax                ; EIP  : point d'entrée user

    iretd                   ; → Ring 3, ne retourne jamais
