bits 32
section .text

extern syscall_dispatch

; -----------------------------------------------------------------------
; syscall_handler_asm – gestionnaire de int $0x80
;
;   Convention d'appel (côté user, Ring 3) :
;     EAX = numéro de syscall
;     EBX = arg1,  ECX = arg2,  EDX = arg3
;     Retour dans EAX
;
;   Ce handler est déclenché comme trap gate (DPL=3, flags=0xEF) :
;   le CPU bascule en Ring 0, charge ESP depuis TSS.esp0, et empile
;   [SS3, ESP3, EFLAGS, CS3, EIP3] avant de sauter ici.
;
;   Stack après pushad + push ds (BASE = ESP à ce point) :
;     BASE+0  : DS sauvegardé  (= 0x23 si on vient de Ring 3)
;     BASE+4  : EDI
;     BASE+8  : ESI
;     BASE+12 : EBP
;     BASE+16 : ESP original
;     BASE+20 : EBX  ← arg1
;     BASE+24 : EDX  ← arg3
;     BASE+28 : ECX  ← arg2
;     BASE+32 : EAX  ← numéro de syscall
;
;   On pousse les 4 arguments sur la stack (ordre inverse cdecl),
;   en recalculant l'offset à chaque push (ESP change de -4).
;   Après l'appel, la valeur de retour (EAX) est écrite dans le slot
;   EAX sauvegardé (BASE+32) pour que popad la restaure dans EAX.
; -----------------------------------------------------------------------
global syscall_handler_asm
syscall_handler_asm:
    pushad                      ; sauve EAX ECX EDX EBX ESP EBP ESI EDI

    push ds                     ; BASE = ESP maintenant
                                ;   [BASE+32]=EAX [BASE+28]=ECX
                                ;   [BASE+24]=EDX [BASE+20]=EBX

    ; Passer en kernel data (DS peut valoir 0x23 si on vient de Ring 3)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pousser les args pour syscall_dispatch(num, arg1, arg2, arg3)
    ; Après chaque push, ESP recule de 4 → recalcule les offsets.
    push dword [esp + 24]       ; arg3 = EDX  (BASE+24, esp=BASE-4)
    push dword [esp + 32]       ; arg2 = ECX  (BASE+28, esp=BASE-8 → BASE+28=(esp+4)+28=esp+32)
    push dword [esp + 28]       ; arg1 = EBX  (BASE+20, esp=BASE-12 → (esp+8)+20=esp+28)
    push dword [esp + 44]       ; num  = EAX  (BASE+32, esp=BASE-16 → (esp+12)+32=esp+44)

    call syscall_dispatch
    add esp, 16                 ; libère les 4 args → ESP = BASE

    ; Stocker la valeur de retour dans le slot EAX sauvegardé (BASE+32)
    ; pour que popad la place dans EAX au moment de la restauration.
    mov [esp + 32], eax

    ; Restaurer le segment DS d'origine (0x10 kernel ou 0x23 user)
    pop eax                     ; EAX = DS sauvegardé → ESP = BASE+4
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad                       ; restaure EDI…EAX (EAX = valeur de retour)
    iretd                       ; retour Ring 0 → Ring 3 (dépile SS3/ESP3 aussi)
