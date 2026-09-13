#ifndef REGS_H
#define REGS_H

/* Snapshot complet des registres au moment d'une exception CPU.
 *
 * L'ORDRE DES CHAMPS DOIT CORRESPONDRE EXACTEMENT à l'empilement fait par
 * isr_common_stub (interrupts/interrupts.asm) : pushad (EAX,ECX,EDX,EBX,
 * ESP,EBP,ESI,EDI, dans cet ordre de push -> EDI se retrouve à l'adresse
 * la plus basse), puis push ds, puis le frame [vecteur, code erreur]
 * empilé par le stub isrN, puis le frame iretd déposé par le CPU
 * [EIP, CS, EFLAGS]. isr_common_stub passe un pointeur brut vers le haut
 * de cette pile (regs_t *) à exception_handler_c — aucune copie.
 * esp_unused correspond à l'ESP original sauvé par pushad ; popad l'ignore,
 * on ne l'utilise pas non plus ici (peu fiable en tant que "ESP courant"). */
typedef struct {
    unsigned int ds;
    unsigned int edi, esi, ebp, esp_unused, ebx, edx, ecx, eax;
    unsigned int vector, error_code;
    unsigned int eip, cs, eflags;
} __attribute__((packed)) regs_t;

#endif /* REGS_H */
