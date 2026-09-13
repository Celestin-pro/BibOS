bits 32
section .text

; -----------------------------------------------------------------------
; gdt_flush(unsigned int gdt_ptr_addr)
;
;   1. Charge la nouvelle GDT via lgdt
;   2. Recharge ds/es/fs/gs/ss avec le sélecteur kernel data (0x10)
;   3. Fait un far jump pour recharger CS avec le sélecteur kernel code (0x08)
;      (seul moyen de changer CS en mode protégé sans iret)
; -----------------------------------------------------------------------
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]      ; eax = adresse du gdt_ptr_struct
    lgdt [eax]

    ; Recharger les registres de segment data
    mov ax, 0x10            ; sélecteur kernel data
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump pour recharger CS = 0x08 (kernel code)
    jmp 0x08:.flush
.flush:
    ret

; -----------------------------------------------------------------------
; tss_flush(unsigned short tss_sel)
;
;   Charge le Task Register avec le sélecteur TSS fourni.
;   Le TSS doit déjà être décrit dans la GDT avant cet appel.
; -----------------------------------------------------------------------
global tss_flush
tss_flush:
    mov ax, [esp + 4]       ; ax = sélecteur TSS (ex. 0x28)
    ltr ax
    ret
