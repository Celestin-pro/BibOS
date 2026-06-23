; Définition des constantes pour le standard Multiboot
MODULEALIGN equ  1 << 0
MEMINFO     equ  1 << 1
FLAGS       equ  MODULEALIGN | MEMINFO
MAGIC       equ  0x1BADB002
CHECKSUM    equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss         ; <--- BIEN METTRE L'ESPACE ICI !
align 16
stack_bottom:
    resb 16384       ; Réserve 16 Ko pour la pile d'exécution
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top       ; Initialise la pile
    push ebx                 ; arg2 : pointeur multiboot_info_t
    push eax                 ; arg1 : magic Multiboot (0x2BADB002)
    call kernel_main         ; Appelle votre fonction C
    cli
.hang:
    hlt                      ; Arrête le processeur si le C se termine
    jmp .hang

global inb
inb:
    mov dx, [esp + 4]
    in al, dx
    ret

global outb
outb:
    mov dx,  [esp + 4]    ; port  (word)
    mov al,  [esp + 8]    ; data  (byte)
    out dx, al
    ret

global init_idt_asm
init_idt_asm:
    mov eax, [esp + 4]    ; Récupère l'argument (adresse de idt_ptr)
    lidt [eax]            ; Charge l'IDT directement depuis cette adresse
    ret