bits 32
section .text

; -----------------------------------------------------------------------
; switch_to(unsigned int *old_esp_ptr, unsigned int new_esp)
;
;   Sauvegarde esp courant dans *old_esp_ptr, puis charge new_esp.
;   Le `ret` final éjecte depuis la stack du NOUVEAU processus :
;     - si processus déjà tourné : retour dans son schedule(), puis
;       irq0_handler envoie EOI + iretd → reprise normale.
;     - si premier démarrage   : retour dans proc_entry_trampoline.
; -----------------------------------------------------------------------
global switch_to
switch_to:
    mov eax, [esp + 4]    ; eax = old_esp_ptr
    mov ecx, [esp + 8]    ; ecx = new_esp
    mov [eax], esp        ; *old_esp_ptr = esp courant
    mov esp, ecx          ; charger la stack du nouveau processus
    ret                   ; retour sur la stack du nouveau processus

; -----------------------------------------------------------------------
; proc_entry_trampoline
;
;   Cible du premier `ret` de switch_to pour un nouveau processus.
;   Le processus n'est PAS entré via iretd, donc il faut :
;     1. Envoyer l'EOI que irq0_handler n'a pas pu envoyer (il a switché).
;     2. Réactiver les interruptions (IF était 0 pendant le handler IRQ0).
;     3. `ret` pour sauter à l'entry function (qui est sur la stack).
; -----------------------------------------------------------------------
global proc_entry_trampoline
proc_entry_trampoline:
    mov al, 0x20
    out 0x20, al    ; EOI → PIC maître (acquitte l'IRQ0 du switch déclencheur)
    sti             ; réactiver les interruptions
    ret             ; sauter à l'entry function empilée par process_create
