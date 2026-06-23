#include "vmm.h"
#include "pmm.h"

/*
 * Page directory (1024 entrées × 4 o = 4 Ko).
 * Chaque entrée couvre 4 Mo de l'espace virtuel.
 */
static unsigned int page_directory[1024] __attribute__((aligned(4096)));

/*
 * Page table dédiée à l'identity-mapping du premier 4 Mo.
 * Le kernel démarre à 1 Mo et occupe moins de 3 Mo → une table suffit.
 */
static unsigned int kernel_page_table[1024] __attribute__((aligned(4096)));

/* Mappe une page physique sur une adresse virtuelle */
void vmm_map_page(unsigned int phys, unsigned int virt, unsigned int flags) {
    unsigned int  pd_idx = virt >> 22;              /* bits 31-22 */
    unsigned int  pt_idx = (virt >> 12) & 0x3FF;   /* bits 21-12 */
    unsigned int *pt;
    unsigned int  i;

    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        /* Créer une nouvelle page table */
        unsigned int new_pt_phys = pmm_alloc_page();
        if (!new_pt_phys) return; /* plus de mémoire */
        page_directory[pd_idx] = new_pt_phys | PAGE_PRESENT | PAGE_RW;
        pt = (unsigned int *)new_pt_phys; /* identity-mapped → phys == virt */
        for (i = 0; i < 1024; i++) pt[i] = 0;
    } else {
        pt = (unsigned int *)(page_directory[pd_idx] & ~0xFFFu);
    }

    pt[pt_idx] = (phys & ~0xFFFu) | flags | PAGE_PRESENT;
}

void vmm_init(void) {
    unsigned int i;

    /* Vider le page directory */
    for (i = 0; i < 1024; i++)
        page_directory[i] = 0;

    /* Identity-map des 4 premiers Mo (adresse physique = adresse virtuelle).
     * Couvre : zone basse (0), VGA (0xB8000), kernel (0x100000) + bitmap. */
    for (i = 0; i < 1024; i++)
        kernel_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;

    /* Entrée 0 du PD → couvre 0x000000 – 0x3FFFFF */
    page_directory[0] = ((unsigned int)kernel_page_table) | PAGE_PRESENT | PAGE_RW;

    /* Charger CR3 et activer la pagination (bit PG de CR0) */
    __asm__ __volatile__(
        "mov %0, %%cr3          \n\t"
        "mov %%cr0, %%eax       \n\t"
        "or  $0x80000000, %%eax \n\t"
        "mov %%eax, %%cr0       \n\t"
        : : "r"(page_directory) : "eax"
    );
}
