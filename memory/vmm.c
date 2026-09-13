#include "vmm.h"
#include "pmm.h"

/*
 * Page directory du kernel (1024 × 4 o = 4 Ko).
 * Chaque entrée couvre 4 Mo de l'espace virtuel.
 * PD[0] (identity-map 0-4 Mo) est PARTAGÉ par tous les espaces d'adressage :
 * chaque process isolé en reçoit une copie de cette entrée, pour que le
 * code kernel et les handlers d'interruption restent accessibles quel que
 * soit le CR3 chargé.
 */
static unsigned int kernel_directory[1024] __attribute__((aligned(4096)));

/*
 * Page table dédiée à l'identity-mapping kernel (0 – 4 Mo).
 * Couvre : NULL-page, VGA (0xB8000), kernel (1 Mo+), bitmap PMM.
 * Pas de PAGE_USER → Ring 3 ne peut pas y accéder.
 */
static unsigned int kernel_page_table[1024] __attribute__((aligned(4096)));

/*
 * Espace d'adressage actif (= celui chargé dans CR3). Toute l'API qui ne
 * prend pas de page directory en paramètre (vmm_map_page, vmm_map_user_page,
 * vmm_check_user_range, ...) opère sur celui-ci. C'est ce qui permet à
 * process_enter_ring3() de mapper du code/une stack "pour le process
 * courant" sans avoir besoin de connaître son page directory explicitement :
 * le scheduler l'a déjà chargé dans CR3 avant de lui rendre la main.
 */
static unsigned int *current_pd = kernel_directory;

/* -----------------------------------------------------------------------
 * Helpers internes
 * ----------------------------------------------------------------------- */

/* Invalide l'entrée TLB pour une adresse virtuelle (évite un reload CR3). */
static void tlb_flush(unsigned int virt) {
    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
}

/* Retourne un pointeur vers la PTE d'une adresse virtuelle, dans l'espace
 * d'adressage actif. Retourne 0 si la page table parente n'existe pas.   */
static unsigned int *vmm_get_pte(unsigned int virt) {
    unsigned int  pd_idx = virt >> 22;
    unsigned int  pt_idx = (virt >> 12) & 0x3FF;
    unsigned int *pt;

    if (!(current_pd[pd_idx] & PAGE_PRESENT))
        return 0;

    pt = (unsigned int *)(current_pd[pd_idx] & ~0xFFFu);
    return &pt[pt_idx];
}

/* -----------------------------------------------------------------------
 * vmm_map_page – cœur du mapping, opère sur l'espace d'adressage actif
 *
 *  Règle x86 : pour qu'un accès Ring 3 réussisse, PDE **et** PTE doivent
 *  avoir PAGE_USER.  On propage donc PAGE_USER de flags vers le PDE quand
 *  on crée (ou met à jour) l'entrée du page directory.
 * ----------------------------------------------------------------------- */
void vmm_map_page(unsigned int phys, unsigned int virt, unsigned int flags) {
    unsigned int  pd_idx  = virt >> 22;
    unsigned int  pt_idx  = (virt >> 12) & 0x3FF;
    unsigned int *pt;
    unsigned int  i;

    /* Flags minimaux pour le PDE : on hérite PAGE_USER si la page le demande.
     * Le PDE est toujours RW pour que le kernel puisse gérer ses tables.    */
    unsigned int pde_flags = PAGE_PRESENT | PAGE_RW;
    if (flags & PAGE_USER)
        pde_flags |= PAGE_USER;

    if (!(current_pd[pd_idx] & PAGE_PRESENT)) {
        /* Créer une nouvelle page table (allouée dans la zone identity-map). */
        unsigned int new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) return;

        current_pd[pd_idx] = new_pt_phys | pde_flags;

        /* Mettre à zéro la nouvelle table (identity-mapped : phys == virt). */
        pt = (unsigned int *)new_pt_phys;
        for (i = 0; i < 1024; i++) pt[i] = 0;
    } else {
        pt = (unsigned int *)(current_pd[pd_idx] & ~0xFFFu);

        /* Si la table existe déjà mais que le PDE n'a pas encore PAGE_USER
         * et qu'on mappe une page user, on l'ajoute maintenant.             */
        if ((flags & PAGE_USER) && !(current_pd[pd_idx] & PAGE_USER))
            current_pd[pd_idx] |= PAGE_USER;
    }

    pt[pt_idx] = (phys & ~0xFFFu) | flags | PAGE_PRESENT;
}

/* -----------------------------------------------------------------------
 * vmm_init – identity-map kernel + activation de la pagination
 * ----------------------------------------------------------------------- */
void vmm_init(void) {
    unsigned int i;

    for (i = 0; i < 1024; i++)
        kernel_directory[i] = 0;

    /* Identity-map 0 – 4 Mo, kernel-only (pas de PAGE_USER).
     * Couvre la page nulle, VGA, kernel, bitmap PMM.                       */
    for (i = 0; i < 1024; i++)
        kernel_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;

    /* PD[0] → 0x000000 – 0x3FFFFF  (pas de PAGE_USER sur le PDE kernel)   */
    kernel_directory[0] = ((unsigned int)kernel_page_table) | PAGE_PRESENT | PAGE_RW;

    /* Charger CR3 et activer la pagination (bit PG de CR0). */
    __asm__ __volatile__(
        "mov %0, %%cr3          \n\t"
        "mov %%cr0, %%eax       \n\t"
        "or  $0x80000000, %%eax \n\t"
        "mov %%eax, %%cr0       \n\t"
        : : "r"(kernel_directory) : "eax"
    );
}

/* -----------------------------------------------------------------------
 * Isolation mémoire par process
 * ----------------------------------------------------------------------- */

/* Retourne l'espace d'adressage kernel (partagé par tous les process qui
 * ne sont pas isolés : le shell, proc_a, proc_b...).                      */
unsigned int *vmm_get_kernel_directory(void) {
    return kernel_directory;
}

/* Alloue un nouveau page directory vierge pour un process isolé.
 * PD[0] est copié depuis kernel_directory (même page table physique que
 * le kernel) : le code kernel et les handlers d'IRQ/syscall restent donc
 * accessibles après un changement de CR3 vers cet espace. Tout le reste
 * (PD[1..1023], où vivent le code/la stack Ring 3) part à zéro : ce
 * process ne voit AUCUNE page d'un autre process isolé.                   */
unsigned int *vmm_new_address_space(void) {
    unsigned int  phys = pmm_alloc_page();
    unsigned int *pd;
    unsigned int  i;

    if (!phys) return 0;

    pd = (unsigned int *)phys;
    for (i = 0; i < 1024; i++) pd[i] = 0;
    pd[0] = kernel_directory[0];

    return pd;
}

/* Change l'espace d'adressage actif (recharge CR3). Ne fait rien si pd est
 * nul ou déjà actif — évite un flush TLB inutile. À appeler par le
 * scheduler avant switch_to(), pour que le process élu tourne dans SON
 * page directory.                                                          */
void vmm_switch_address_space(unsigned int *pd) {
    if (!pd || pd == current_pd) return;
    current_pd = pd;
    __asm__ __volatile__("mov %0, %%cr3" : : "r"((unsigned int)pd) : "memory");
}

/* Libère toutes les pages physiques référencées par pd (PD[1..1023] : code
 * et stack user, plus les page tables elles-mêmes), puis pd lui-même.
 * PD[0] (kernel, partagé entre tous les espaces) n'est jamais libéré ici —
 * le libérer casserait TOUS les autres process, y compris celui en train
 * d'appeler cette fonction.                                                */
void vmm_destroy_address_space(unsigned int *pd) {
    unsigned int  pd_idx, pt_idx;
    unsigned int *pt;

    if (!pd) return;

    for (pd_idx = 1; pd_idx < 1024; pd_idx++) {
        if (!(pd[pd_idx] & PAGE_PRESENT)) continue;

        pt = (unsigned int *)(pd[pd_idx] & ~0xFFFu);
        for (pt_idx = 0; pt_idx < 1024; pt_idx++) {
            if (pt[pt_idx] & PAGE_PRESENT)
                pmm_free_page(pt[pt_idx] & ~0xFFFu);
        }
        pmm_free_page((unsigned int)pt);
    }

    pmm_free_page((unsigned int)pd);
}

/* -----------------------------------------------------------------------
 * API userspace — opère sur l'espace d'adressage actif (current_pd)
 * ----------------------------------------------------------------------- */

/* Alloue une page physique via PMM et la mappe à virt en mode user R/W.    */
void vmm_map_user_page(unsigned int virt) {
    unsigned int phys = pmm_alloc_page();
    if (!phys) return;
    vmm_map_page(phys, virt, PAGE_PRESENT | PAGE_RW | PAGE_USER);
}

/* Démapper une page userspace : récupère la page physique, efface la PTE,
 * invalide le TLB, et libère la page dans la PMM.                          */
void vmm_unmap_user_page(unsigned int virt) {
    unsigned int *pte = vmm_get_pte(virt);
    unsigned int  phys;

    if (!pte || !(*pte & PAGE_PRESENT)) return;

    phys = *pte & ~0xFFFu;
    *pte = 0;
    tlb_flush(virt);
    pmm_free_page(phys);
}

/* Alloue et mappe 1 page pour la stack user juste sous VMM_USER_STACK_TOP.
 * Retourne l'ESP initial (sommet de stack, aligné 4 o).
 * La stack croit vers le bas : la première instruction user fait un push
 * qui écrit à ESP-4, donc toujours dans la page allouée.                   */
unsigned int vmm_create_user_stack(void) {
    vmm_map_user_page(VMM_USER_STACK_TOP - PAGE_SIZE);
    return VMM_USER_STACK_TOP;
}

/* -----------------------------------------------------------------------
 * vmm_check_user_range – valide un buffer fourni par du code Ring 3
 *
 *   À appeler par tout syscall avant de déréférencer un pointeur venu de
 *   l'utilisateur (sys_write, sys_read, ...). Sans ça, un process Ring 3
 *   peut passer n'importe quelle adresse (ex : mémoire kernel) et le
 *   kernel la lit/écrit avec ses propres privilèges → fuite d'info ou
 *   corruption arbitraire depuis du code non privilégié.
 *
 *   Retourne 1 si TOUTES les pages couvrant [virt, virt+len) sont
 *   présentes et marquées PAGE_USER (PDE et PTE) dans l'espace d'adressage
 *   actif (= celui du process appelant), 0 sinon.
 * ----------------------------------------------------------------------- */
int vmm_check_user_range(unsigned int virt, unsigned int len) {
    unsigned int start, end, addr;

    if (len == 0) return 1;
    if (virt + len < virt) return 0;   /* overflow */

    start = virt & ~0xFFFu;
    end   = (virt + len - 1) & ~0xFFFu;

    for (addr = start; ; addr += PAGE_SIZE) {
        unsigned int  pd_idx = addr >> 22;
        unsigned int *pte;

        if (!(current_pd[pd_idx] & PAGE_PRESENT) ||
            !(current_pd[pd_idx] & PAGE_USER))
            return 0;

        pte = vmm_get_pte(addr);
        if (!pte || !(*pte & PAGE_PRESENT) || !(*pte & PAGE_USER))
            return 0;

        if (addr == end) break;
    }
    return 1;
}
