#include "syscall.h"
#include "../interrupts/idt.h"
#include "../driver/keyboard.h"
#include "../process/process.h"
#include "../memory/vmm.h"

/* Interface assembleur du handler */
extern void syscall_handler_asm(void);

/* -----------------------------------------------------------------------
 * sys_write – écrit len octets du buffer user sur le terminal
 *
 *   Le buffer est une adresse virtuelle userspace. On vérifie qu'il est
 *   entièrement mappé PAGE_USER avant de le déréférencer : sinon un
 *   process Ring 3 pourrait passer une adresse kernel et le kernel la
 *   lirait avec ses propres privilèges (fuite d'info).
 *   Retourne SYS_EFAULT si le buffer déborde de l'espace user.
 * ----------------------------------------------------------------------- */
static unsigned int sys_write(unsigned int buf_virt, unsigned int len) {
    const char *buf = (const char *)buf_virt;
    unsigned int i;

    if (!buf_virt || len == 0) return 0;
    if (!vmm_check_user_range(buf_virt, len)) return SYS_EFAULT;

    for (i = 0; i < len; i++)
        kputchar(buf[i]);

    return i;
}

/* -----------------------------------------------------------------------
 * sys_read – lit jusqu'à len octets depuis le clavier dans le buffer user
 *
 *   Bloquant : attend chaque touche via kgetchar().
 *   S'arrête sur '\n' (inclus dans le buffer) ou si len atteint.
 *   Le buffer est validé (PAGE_USER) avant toute écriture : un process
 *   Ring 3 ne doit pas pouvoir faire écrire le kernel dans sa propre
 *   mémoire en passant une adresse kernel comme buffer.
 * ----------------------------------------------------------------------- */
static unsigned int sys_read(unsigned int buf_virt, unsigned int len) {
    char *buf = (char *)buf_virt;
    unsigned int i;
    char c;

    if (!buf_virt || len == 0) return 0;
    if (!vmm_check_user_range(buf_virt, len)) return SYS_EFAULT;

    for (i = 0; i < len - 1; i++) {
        c = kgetchar();
        kputchar(c);           /* écho local */
        buf[i] = c;
        if (c == '\n') { i++; break; }
    }
    buf[i] = '\0';
    return i;
}

/* -----------------------------------------------------------------------
 * sys_yield – cède volontairement le CPU
 *
 *   On déclenche int $0x20 (= vecteur IRQ0) depuis Ring 0 pour passer
 *   par le chemin irq0_handler_asm complet :
 *     – schedule() change de contexte si nécessaire
 *     – EOI est envoyé au PIC avant le iretd
 *   Quand notre contexte est réélu, on revient ici et on retourne 0.
 * ----------------------------------------------------------------------- */
static unsigned int sys_yield(void) {
    __asm__ volatile("int $0x20");
    return 0;
}

/* -----------------------------------------------------------------------
 * sys_exit – termine le processus courant
 *
 *   Marque le processus PROC_UNUSED et provoque un context switch.
 *   Ne retourne jamais (le processus est abandonné).
 * ----------------------------------------------------------------------- */
static unsigned int sys_exit(unsigned int code) {
    (void)code;

    /* Marquer le processus comme terminé puis basculer sur un autre. */
    if (current_proc)
        current_proc->state = PROC_UNUSED;

    /* Passer par IRQ0 pour le switch propre (EOI inclus). */
    __asm__ volatile("int $0x20");

    /* Jamais atteint : le processus ne sera plus réélu. */
    while (1) {}
    return 0;
}

/* -----------------------------------------------------------------------
 * syscall_dispatch – table de dispatch centrale
 *
 *   Appelé depuis syscall_handler_asm avec :
 *     num  = EAX (numéro de syscall)
 *     arg1 = EBX, arg2 = ECX, arg3 = EDX
 *   Retourne la valeur placée dans EAX au retour en Ring 3.
 * ----------------------------------------------------------------------- */
unsigned int syscall_dispatch(unsigned int num,
                              unsigned int arg1,
                              unsigned int arg2,
                              unsigned int arg3) {
    (void)arg3;   /* inutilisé pour l'instant */

    switch (num) {
        case SYS_WRITE: return sys_write(arg1, arg2);
        case SYS_EXIT:  return sys_exit(arg1);
        case SYS_YIELD: return sys_yield();
        case SYS_READ:  return sys_read(arg1, arg2);
        default:        return SYS_ENOSYS;
    }
}

/* -----------------------------------------------------------------------
 * syscall_init – enregistre la gate IDT 0x80
 *
 *   flags = 0xEF = 1110 1111
 *     P=1  : présent
 *     DPL=3: accessible depuis Ring 3 (int $0x80 en user mode)
 *     S=0  : descripteur système
 *     Type=0xF : trap gate 32 bits (IF n'est PAS effacé → interruptions
 *                restent actives pendant le syscall)
 * ----------------------------------------------------------------------- */
void syscall_init(void) {
    idt_set_gate(0x80,
                 (unsigned int)syscall_handler_asm,
                 0x08,    /* CS kernel code */
                 0xEF);   /* trap gate 32b, DPL=3 */
}
