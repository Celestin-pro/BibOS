bits 32
global default_handler_asm
global keyboard_handler_asm
global irq0_handler_asm
extern keyboard_handler_c
extern default_handler_c
extern exception_handler_c
extern schedule

; -----------------------------------------------------------------------
; keyboard_handler_asm — IRQ1 (clavier)
;
;   Sauvegarde DS (peut valoir 0x23 si on vient de Ring 3) puis force
;   DS/ES/FS/GS à 0x10 (kernel data) le temps du handler, et restaure
;   avant l'iretd. iretd se charge de restaurer CS/EIP/EFLAGS et, si
;   changement de privilège, SS/ESP → retour transparent en Ring 3.
; -----------------------------------------------------------------------
keyboard_handler_asm:
    pushad

    mov ax, ds
    push eax                ; sauvegarder DS courant (0x10 ou 0x23)

    mov ax, 0x10            ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call keyboard_handler_c

    pop eax                 ; restaurer DS d'origine
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad
    iretd

; -----------------------------------------------------------------------
; irq0_handler_asm — timer PIT, déclenche le scheduler
;
;   pushad sauvegarde les registres du processus courant sur SA stack.
;   schedule() peut appeler switch_to() et changer esp → on se retrouve
;   sur la stack du NOUVEAU processus (qui a lui aussi ses registres
;   sauvés par son propre pushad passé).
;   L'EOI est envoyé ici pour les switchs "normaux" (processus déjà vus).
;   Pour un premier démarrage, c'est proc_entry_trampoline qui l'envoie.
; -----------------------------------------------------------------------
irq0_handler_asm:
    pushad

    mov ax, ds
    push eax                ; sauvegarder DS courant

    mov ax, 0x10            ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call schedule           ; peut switcher la stack → on revient sur une autre stack

    mov al, 0x20
    out 0x20, al            ; EOI → PIC maître

    pop eax                 ; restaurer DS d'origine
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad
    iretd

; -----------------------------------------------------------------------
; default_handler_asm — handler par défaut (IRQ/vecteurs sans code
; d'erreur : uniquement des vecteurs > 31 non spécifiquement gérés
; ailleurs, donc jamais un des 7 exceptions CPU qui poussent un code
; d'erreur — voir isrN/isr_common_stub ci-dessous pour celles-ci).
; -----------------------------------------------------------------------
default_handler_asm:
    pushad
    call default_handler_c
    popad
    iretd

; -----------------------------------------------------------------------
; isr0..isr31 — un stub par exception CPU (vecteurs 0-31)
;
;   Le x86 pousse AUTOMATIQUEMENT un code d'erreur sur la pile, AVANT le
;   frame iretd [EIP,CS,EFLAGS,(ESP,SS)], pour les exceptions #8 et
;   #10-#14 et #17 (dont #14 Page Fault). Pour toutes les autres, rien
;   n'est poussé. Sans stub dédié, un handler générique qui fait juste
;   `iretd` après une exception AVEC code d'erreur dépile ce code comme
;   s'il s'agissait d'EIP → saut vers une adresse arbitraire au retour.
;
;   Chaque stub uniformise la pile en poussant lui-même :
;     - un code d'erreur factice (0) pour les vecteurs qui n'en ont pas
;     - le numéro de vecteur, dans tous les cas
;   puis saute vers isr_common_stub qui dépile les deux avant l'iretd.
; -----------------------------------------------------------------------
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0        ; code d'erreur factice
    push dword %1        ; numéro de vecteur
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1        ; numéro de vecteur (code d'erreur déjà poussé par le CPU)
    jmp isr_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14              ; #PF Page Fault
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; -----------------------------------------------------------------------
; isr_common_stub
;
;   Pile à l'entrée (empilé par le stub, du plus haut au plus bas) :
;     [esp+0]  vecteur
;     [esp+4]  code d'erreur (réel ou factice)
;     [esp+8]  EIP  \
;     [esp+12] CS    | frame iretd déposé par le CPU
;     [esp+16] EFLAGS/
;
;   Après pushad (32 o, ordre EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI donc EDI en
;   [esp+0]) + push ds (4 o), TOUTE la pile à partir de esp forme la
;   structure regs_t (interrupts/regs.h) dans l'ordre exact de ses champs :
;   ds, edi, esi, ebp, esp_unused, ebx, edx, ecx, eax, vector, error_code,
;   eip, cs, eflags. On passe donc simplement `esp` lui-même (un seul
;   pointeur, cdecl) à exception_handler_c(regs_t *regs) — accès direct à
;   TOUS les registres et au frame CPU, pour un vrai dump de panic.
; -----------------------------------------------------------------------
isr_common_stub:
    pushad

    ; xor avant le mov ax,ds : sinon les 16 bits hauts d'eax gardent la
    ; valeur du code qui a fauté (pushad ne les efface pas), et regs->ds
    ; (unsigned int, affiché en hex sur 32 bits par le dump de panic)
    ; montrerait un DS "corrompu" qui n'est en fait qu'un artefact de
    ; capture — pas une vraie corruption du segment.
    xor eax, eax
    mov ax, ds
    push eax                ; sauvegarder DS courant (32 bits propres)

    mov ax, 0x10             ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                 ; regs_t* : pointe sur la structure décrite ci-dessus
    call exception_handler_c
    add esp, 4                ; libère l'argument

    pop eax                  ; restaurer DS d'origine
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad
    add esp, 8                ; dépiler vecteur + code d'erreur avant l'iretd
    iretd
