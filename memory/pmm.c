#include "pmm.h"

/*
 * Bitmap des pages physiques.
 * 1 bit = 1 page de 4 Ko.  1 = réservée/utilisée, 0 = libre.
 * 4 Go / 4 Ko = 1 048 576 pages → 128 Ko de bitmap.
 */
#define MAX_PAGES (1024 * 1024)

static unsigned int bitmap[MAX_PAGES / 32];
static unsigned int total_pages;
static unsigned int free_pages;

/* --- opérations sur le bitmap --- */

static void pmm_set(unsigned int page) {
    bitmap[page / 32] |= (1u << (page % 32));
}

static void pmm_clear(unsigned int page) {
    bitmap[page / 32] &= ~(1u << (page % 32));
}

static int pmm_test(unsigned int page) {
    return (bitmap[page / 32] >> (page % 32)) & 1u;
}

/* --- marquer une région comme libre / réservée --- */

static void pmm_free_region(unsigned int base, unsigned int size) {
    unsigned int page  = base / PAGE_SIZE;
    unsigned int count = size / PAGE_SIZE;
    while (count--) {
        if (pmm_test(page)) {
            pmm_clear(page);
            free_pages++;
        }
        page++;
    }
}

static void pmm_reserve_region(unsigned int base, unsigned int size) {
    unsigned int page  = base / PAGE_SIZE;
    unsigned int count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    while (count--) {
        if (!pmm_test(page)) {
            pmm_set(page);
            if (free_pages) free_pages--;
        }
        page++;
    }
}

/* --- initialisation --- */

void pmm_init(multiboot_info_t *mbi, unsigned int kernel_end) {
    unsigned int i;

    /* Tout marquer comme réservé par défaut */
    for (i = 0; i < MAX_PAGES / 32; i++)
        bitmap[i] = 0xFFFFFFFF;
    total_pages = MAX_PAGES;
    free_pages  = 0;

    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        /* Parcourir la memory map fournie par le BIOS via GRUB */
        multiboot_mmap_entry_t *entry =
            (multiboot_mmap_entry_t *)mbi->mmap_addr;

        while ((unsigned int)entry < mbi->mmap_addr + mbi->mmap_length) {
            if (entry->type == 1 && entry->addr < 0xFFFFFFFFu) {
                unsigned int base = (unsigned int)entry->addr;
                unsigned int len  = (entry->len > 0xFFFFFFFFu)
                                    ? (0xFFFFFFFFu - base)
                                    : (unsigned int)entry->len;
                pmm_free_region(base, len);
            }
            /* size ne compte pas les 4 octets du champ size lui-même */
            entry = (multiboot_mmap_entry_t *)
                    ((unsigned int)entry + entry->size + 4);
        }
    } else {
        /* Fallback : mem_upper = Ko disponibles au-dessus de 1 Mo */
        pmm_free_region(0x100000, mbi->mem_upper * 1024);
    }

    /* Re-réserver les zones critiques */
    pmm_reserve_region(0, kernel_end);     /* page nulle + tout le kernel   */
    pmm_reserve_region(0xA0000, 0x60000);  /* VGA, BIOS, ROM                */
}

/* --- allocation / libération --- */

unsigned int pmm_alloc_page(void) {
    unsigned int i, j;
    for (i = 0; i < MAX_PAGES / 32; i++) {
        if (bitmap[i] == 0xFFFFFFFFu) continue; /* tout utilisé, sauter */
        for (j = 0; j < 32; j++) {
            if (!((bitmap[i] >> j) & 1u)) {
                unsigned int page = i * 32 + j;
                pmm_set(page);
                free_pages--;
                return page * PAGE_SIZE;
            }
        }
    }
    return 0; /* out of memory */
}

void pmm_free_page(unsigned int addr) {
    unsigned int page = addr / PAGE_SIZE;
    if (pmm_test(page)) {
        pmm_clear(page);
        free_pages++;
    }
}

unsigned int pmm_free_count(void) { return free_pages; }
