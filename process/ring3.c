#include "ring3.h"
#include "../memory/vmm.h"
#include "../memory/pmm.h"

/* Interface assembleur — construit et exécute le frame iretd.             */
extern void enter_ring3_asm(unsigned int eip, unsigned int esp_user);

/* -----------------------------------------------------------------------
 * enter_ring3
 *
 *   L'ESP0 (stack que le CPU charge automatiquement à chaque retour en
 *   Ring 0 depuis Ring 3 — timer, clavier, syscall...) N'EST PLUS géré
 *   ici : il l'a déjà été par schedule() (process.c), qui le fait pointer
 *   vers la stack DÉDIÉE de CE process (p->stack, réutilisée : elle ne
 *   sert plus à rien une fois qu'on est passé en Ring 3) à chaque fois
 *   qu'il devient le process actif.
 *
 *   Avant, un seul buffer ring3_kernel_stack était partagé par TOUS les
 *   process Ring 3 : si deux se chevauchaient dans le temps (l'un pas
 *   encore sorti quand l'autre entrait en Ring 3), ils écrasaient le
 *   contexte d'interruption l'un de l'autre sur la même pile physique —
 *   l'un des deux restait corrompu et ne finissait jamais (fuite de ses
 *   ressources, jamais marqué PROC_UNUSED). Un esp0 par process élimine
 *   ce partage.
 *
 *   Cette fonction ne retourne jamais.
 * ----------------------------------------------------------------------- */
void enter_ring3(unsigned int eip, unsigned int esp) {
    enter_ring3_asm(eip, esp);

    /* Jamais atteint – iretd ne retourne pas */
    __builtin_unreachable();
}

/* -----------------------------------------------------------------------
 * process_enter_ring3
 *   Copie le code de la fonction kernel [entry, entry_end) en mémoire
 *   user (PAGE_USER, R/W, à VMM_USER_CODE_BASE), alloue une stack user,
 *   puis saute en Ring 3.
 *
 *   Contrainte : le code copié doit être Position Independent.
 *   Une fonction qui ne fait que tourner en boucle (while(1){}) ou
 *   manipuler des variables locales satisfait cette contrainte avec -O2.
 *
 *   Cette fonction ne retourne jamais.
 * ----------------------------------------------------------------------- */
void process_enter_ring3(void (*entry)(void), void (*entry_end)(void)) {
    unsigned int   code_size = (unsigned int)entry_end - (unsigned int)entry;
    unsigned char *src       = (unsigned char *)entry;
    unsigned char *dst;
    unsigned int   user_esp;
    unsigned int   i;

    /* Mapper une page user pour accueillir le code (R/W/U).              */
    vmm_map_user_page(VMM_USER_CODE_BASE);
    dst = (unsigned char *)VMM_USER_CODE_BASE;

    /* Copier les octets du code (au plus une page).                      */
    for (i = 0; i < code_size && i < PAGE_SIZE; i++)
        dst[i] = src[i];

    /* Allouer et mapper la stack user (1 page sous VMM_USER_STACK_TOP).  */
    user_esp = vmm_create_user_stack();

    /* Sauter en Ring 3.                                                   */
    enter_ring3(VMM_USER_CODE_BASE, user_esp);
}
