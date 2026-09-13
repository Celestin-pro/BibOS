#include "process.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../gdt/gdt.h"

/* Table des processus et processus courant */
static process_t procs[MAX_PROCS];
process_t       *current_proc = 0;
static unsigned int next_pid  = 0;

/* Déclarés dans switch.asm */
extern void switch_to(unsigned int *old_esp_ptr, unsigned int new_esp);
extern void proc_entry_trampoline(void);

/* --------------------------------------------------------------------------
 * Nettoyage différé des process terminés (sys_exit, ou une exception CPU
 * dans un process isolé — voir exception_handler_c dans kernel.c)
 *
 *   On ne peut PAS libérer p->stack au moment où un process est marqué
 *   UNUSED : on tourne encore dessus (directement, ou via l'esp0 qui
 *   pointe dedans pour un process ring3 — voir schedule() plus bas). Il
 *   faut attendre d'être garanti de tourner sur une AUTRE stack.
 *
 *   Chaque process a son propre drapeau `pending_free` (process.h) : un
 *   simple pointeur global unique ne suffit pas dès qu'on veut pouvoir
 *   perdre la trace d'AU PLUS UN process à la fois entre deux passages —
 *   avec deux points d'écriture possibles (schedule() pour sys_exit,
 *   exception_handler_c pour un crash Ring 3), rien ne garantit qu'un
 *   seul process soit "en attente" au même instant. `any_pending` reste
 *   un simple raccourci pour éviter le scan complet quand il n'y a
 *   vraiment rien à faire (immense majorité des ticks timer).
 *
 *   process_reap_zombie() est réclamée au tout début du prochain
 *   schedule() (on vient de switcher ailleurs, donc c'est sûr) et au tout
 *   début de process_spawn() (pour ne jamais réutiliser un slot dont les
 *   ressources n'ont pas encore été rendues à la PMM). Elle est appelée
 *   depuis DEUX contextes différents :
 *     - schedule() : toujours avec IF=0 (gate d'interruption 0x8E), donc
 *       naturellement atomique vis-à-vis d'un autre appel.
 *     - process_spawn() : appelée directement depuis le shell (ex.
 *       "ring3", "leaktest"), avec IF=1, SANS protection.
 *   On sauvegarde/restaure EFLAGS (pushfl/popfl) plutôt que de faire un
 *   cli/sti inconditionnel : un sti forcé en sortie casserait schedule()
 *   (qui doit rester avec IF=0 jusqu'à la fin de switch_to()).           */
static int any_pending = 0;

static void process_reap_zombie(void) {
    unsigned int eflags;
    int          i;

    if (!any_pending) return;

    __asm__ __volatile__(
        "pushfl\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(eflags)
        :
        : "memory", "cc"
    );

    for (i = 0; i < MAX_PROCS; i++) {
        process_t *p = &procs[i];
        if (!p->pending_free) continue;

        if (p->stack)
            pmm_free_page((unsigned int)p->stack);

        if (p->page_directory &&
            p->page_directory != vmm_get_kernel_directory())
            vmm_destroy_address_space(p->page_directory);

        p->stack          = 0;
        p->page_directory = 0;
        p->pending_free   = 0;
    }
    any_pending = 0;

    __asm__ __volatile__(
        "push %0\n\t"
        "popfl"
        :
        : "r"(eflags)
        : "memory", "cc"
    );
}

/* --------------------------------------------------------------------------
 * process_init
 *   Enregistre le thread kernel courant comme processus 0.
 *   Doit être appelé avant tout process_create() et avant d'activer le timer.
 * -------------------------------------------------------------------------- */
void process_init(void) {
    int i;
    for (i = 0; i < MAX_PROCS; i++)
        procs[i].state = PROC_UNUSED;

    procs[0].pid            = next_pid++;
    procs[0].esp            = 0;   /* sera écrasé par switch_to au premier switch  */
    procs[0].stack          = 0;   /* stack du kernel main, pas gérée par le PMM   */
    procs[0].page_directory = vmm_get_kernel_directory();
    procs[0].state          = PROC_RUNNING;
    procs[0].pending_free   = 0;
    current_proc = &procs[0];
}

/* --------------------------------------------------------------------------
 * process_spawn – helper commun à process_create/process_create_isolated
 *
 *   Alloue une stack, y empile le frame initial, et marque le processus READY
 *   dans l'espace d'adressage pd fourni par l'appelant.
 *
 *   Frame initial (du haut de la stack vers le bas, i.e. ordre de dépilage) :
 *     [entry]                  ← ret de proc_entry_trampoline → saute à entry
 *     [proc_entry_trampoline]  ← ret de switch_to             → trampoline
 *
 *   Quand switch_to fait ret, on arrive dans proc_entry_trampoline qui :
 *     1. envoie l'EOI au PIC (IRQ0 du switch déclencheur)
 *     2. fait sti (les interruptions étaient désactivées pendant le handler)
 *     3. fait ret → saute à entry()
 * -------------------------------------------------------------------------- */
/* Retourne 1 si le process a bien été créé, 0 sinon (table pleine ou PMM
 * épuisée). NE libère PAS pd en cas d'échec : pd peut être l'espace
 * d'adressage kernel partagé (process_create) qu'il ne faut jamais
 * détruire — c'est à l'appelant qui l'a alloué (process_create_isolated)
 * de le libérer si process_spawn échoue.                                 */
static int process_spawn(void (*entry)(void), unsigned int *pd) {
    int            i;
    process_t     *p = 0;
    unsigned int  *sp;
    unsigned char *stack;

    /* Ne jamais réutiliser un slot dont les ressources n'ont pas encore
     * été rendues à la PMM (voir process_reap_zombie ci-dessus).          */
    process_reap_zombie();

    /* Trouver un slot libre */
    for (i = 0; i < MAX_PROCS; i++) {
        if (procs[i].state == PROC_UNUSED) { p = &procs[i]; break; }
    }
    if (!p) return 0;

    /* Allouer une page (4 Ko) comme stack du nouveau processus */
    stack = (unsigned char *)pmm_alloc_page();
    if (!stack) return 0;

    sp = (unsigned int *)(stack + PAGE_SIZE);  /* esp part du haut */

    *--sp = (unsigned int)entry;                   /* cible du ret de trampoline */
    *--sp = (unsigned int)proc_entry_trampoline;   /* cible du ret de switch_to  */

    p->pid            = next_pid++;
    p->esp            = (unsigned int)sp;
    p->stack          = stack;
    p->page_directory = pd;
    p->state          = PROC_READY;
    p->pending_free   = 0;
    return 1;
}

/* Process kernel-only : partage l'espace d'adressage du kernel. */
void process_create(void (*entry)(void)) {
    process_spawn(entry, vmm_get_kernel_directory());
}

/* Process isolé : espace d'adressage dédié (voir vmm_new_address_space).
 * Si process_spawn échoue (table de process pleine ou PMM épuisée), pd a
 * déjà été alloué par vmm_new_address_space() ci-dessous : il faut le
 * détruire ici, sinon c'est une page directory orpheline à chaque échec
 * — exactement le genre de fuite silencieuse que "leaktest" mesure.      */
void process_create_isolated(void (*entry)(void)) {
    unsigned int *pd = vmm_new_address_space();
    if (!pd) return;
    if (!process_spawn(entry, pd))
        vmm_destroy_address_space(pd);
}

/* --------------------------------------------------------------------------
 * schedule
 *   Sélectionne le prochain processus READY (round-robin) et effectue le
 *   context switch. Appelé depuis irq0_handler (interruptions désactivées).
 * -------------------------------------------------------------------------- */
void schedule(void) {
    int i;
    process_t *old;
    process_t *next = 0;
    int start;

    /* On tourne forcément sur une AUTRE stack que celle du zombie marqué
     * lors du switch précédent : point sûr pour le libérer.               */
    process_reap_zombie();

    old   = current_proc;
    start = (int)(old - procs + 1) % MAX_PROCS;

    for (i = 0; i < MAX_PROCS; i++) {
        int idx = (start + i) % MAX_PROCS;
        if (procs[idx].state == PROC_READY ||
            procs[idx].state == PROC_RUNNING) {
            next = &procs[idx];
            break;
        }
    }

    /* Ce marquage doit avoir lieu AVANT le "rien à faire" ci-dessous : si
     * old vient de sortir (PROC_UNUSED) et qu'aucun autre process n'est
     * disponible (next == 0), la fonction retourne sans jamais switcher —
     * mais old ne tournera de toute façon plus jamais (sys_exit ne revient
     * pas), donc ses ressources doivent être mises en attente de reap
     * MÊME dans ce cas, sous peine de fuite permanente et silencieuse. */
    if (old->state == PROC_RUNNING)
        old->state = PROC_READY;
    else if (old->state == PROC_UNUSED) {
        old->pending_free = 1;
        any_pending       = 1;
    }

    if (!next || next == old) return;  /* rien d'autre à faire */

    next->state  = PROC_RUNNING;
    current_proc = next;

    /* Basculer sur l'espace d'adressage du process élu AVANT de changer de
     * stack : le code kernel (schedule, irq0_handler...) reste identiquement
     * mappé dans tous les espaces (PD[0] partagé), donc ce changement de
     * CR3 est transparent pour l'exécution en cours.                        */
    vmm_switch_address_space(next->page_directory);

    /* esp0 : la stack que le CPU charge automatiquement à chaque retour en
     * Ring 0 depuis Ring 3 (timer, syscall...) pour CE process. On réutilise
     * p->stack : elle ne sert plus à rien une fois le process passé en
     * Ring 3 (voir ring3.c). Sans ça, tous les process Ring 3 partageraient
     * la même pile d'interruption et se corromperaient mutuellement dès
     * qu'ils se chevauchent dans le temps.                                  */
    if (next->stack)
        tss_set_esp0((unsigned int)next->stack + PAGE_SIZE);

    switch_to(&old->esp, next->esp);
    /* On revient ici quand ce processus est ré-élu */
}
