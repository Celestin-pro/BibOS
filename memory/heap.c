#include "heap.h"
#include "pmm.h"

/* ------------------------------------------------------------------ *
 *  Classes de taille
 *  Toute requête est arrondie à la classe supérieure.
 * ------------------------------------------------------------------ */
#define NUM_CLASSES 9
static const unsigned int size_classes[NUM_CLASSES] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048
};

/* ------------------------------------------------------------------ *
 *  Layout mémoire d'une page allouée par le heap
 *
 *  ┌──────────────────────────────────────┐  ← adresse physique (4Ko alignée)
 *  │  page_hdr_t  (12 octets)             │
 *  ├──────────────────────────────────────┤
 *  │  block_hdr_t (4 o) │ données (N o)  │  bloc 0
 *  │  block_hdr_t (4 o) │ données (N o)  │  bloc 1
 *  │  ...                                 │
 *  └──────────────────────────────────────┘
 *
 *  Quand un bloc est LIBRE, ses données contiennent un pointeur
 *  vers le prochain bloc libre (free_block_t).
 *  Quand il est ALLOUÉ, les données appartiennent à l'appelant.
 * ------------------------------------------------------------------ */

typedef struct {
    unsigned int size_class; /* classe de ce bloc              */
} block_hdr_t;

typedef struct free_block {
    struct free_block *next; /* chaînage dans la free list     */
} free_block_t;

typedef struct {
    unsigned int size_class; /* classe desservie par la page   */
    unsigned int used;       /* nombre de blocs alloués        */
    unsigned int total;      /* nombre total de blocs          */
} page_hdr_t;

/* Une free list par classe (pointeur vers les données du 1er bloc libre) */
static free_block_t *free_lists[NUM_CLASSES];

/* ------------------------------------------------------------------ *
 *  Fonctions internes
 * ------------------------------------------------------------------ */

/* Arrondir à la classe supérieure → renvoie l'index, -1 si trop grand */
static int class_index(unsigned int size) {
    int i;
    for (i = 0; i < NUM_CLASSES; i++)
        if (size <= size_classes[i]) return i;
    return -1;
}

/* Index exact d'une size_class (pour kfree) */
static int class_index_exact(unsigned int sc) {
    int i;
    for (i = 0; i < NUM_CLASSES; i++)
        if (size_classes[i] == sc) return i;
    return -1;
}

/* La page à laquelle appartient un bloc : on aligne vers le bas sur 4 Ko */
static page_hdr_t *page_of(block_hdr_t *hdr) {
    return (page_hdr_t *)((unsigned int)hdr & ~0xFFFu);
}

/* Découper une nouvelle page PMM en blocs et les ajouter à la free list */
static int expand_class(int idx) {
    unsigned int  phys = pmm_alloc_page();
    unsigned int  block_total, avail, i;
    char         *cursor;
    page_hdr_t   *pg;

    if (!phys) return 0;

    pg             = (page_hdr_t *)phys;
    pg->size_class = size_classes[idx];
    pg->used       = 0;

    block_total    = sizeof(block_hdr_t) + size_classes[idx];
    avail          = PAGE_SIZE - sizeof(page_hdr_t);
    pg->total      = avail / block_total;

    /* Chaîner tous les blocs dans la free list */
    cursor = (char *)pg + sizeof(page_hdr_t);
    for (i = 0; i < pg->total; i++) {
        block_hdr_t  *hdr = (block_hdr_t *)cursor;
        free_block_t *fb  = (free_block_t *)(cursor + sizeof(block_hdr_t));
        hdr->size_class   = size_classes[idx];
        fb->next          = free_lists[idx];
        free_lists[idx]   = fb;
        cursor           += block_total;
    }
    return 1;
}

/* Retirer de la free list tous les blocs appartenant à pg
 * (appelé juste avant de rendre la page au PMM)              */
static void remove_page_from_freelist(int idx, page_hdr_t *pg) {
    unsigned int  base    = (unsigned int)pg;
    free_block_t *rebuilt = 0;
    free_block_t *cur     = free_lists[idx];

    while (cur) {
        free_block_t *next = cur->next;
        block_hdr_t  *hdr  = (block_hdr_t *)((char *)cur - sizeof(block_hdr_t));

        if (((unsigned int)hdr & ~0xFFFu) != base) {
            /* Ce bloc n'appartient pas à la page → on le garde */
            cur->next = rebuilt;
            rebuilt   = cur;
        }
        cur = next;
    }
    free_lists[idx] = rebuilt;
}

/* ------------------------------------------------------------------ *
 *  API publique
 * ------------------------------------------------------------------ */

/*
 * kmalloc(size)
 *   Arrondit size à la classe supérieure.
 *   Détache le premier bloc libre de la liste et le renvoie.
 *   Si la liste est vide, ouvre une nouvelle page via le PMM.
 */
void *kmalloc(unsigned int size) {
    block_hdr_t  *hdr;
    free_block_t *fb;
    page_hdr_t   *pg;
    int           idx;

    if (!size) return 0;

    idx = class_index(size);
    if (idx < 0) return 0; /* > 2048 o : non géré */

    if (!free_lists[idx])
        if (!expand_class(idx)) return 0;

    /* Détacher le premier bloc libre */
    fb              = free_lists[idx];
    free_lists[idx] = fb->next;

    hdr = (block_hdr_t *)((char *)fb - sizeof(block_hdr_t));

    pg = page_of(hdr);
    pg->used++;

    return (void *)fb; /* pointeur sur les données (après le header) */
}

/*
 * kfree(ptr)
 *   Lit le header juste avant ptr pour retrouver la classe.
 *   Réinjecte le bloc en tête de la free list.
 *   Si tous les blocs de la page sont libres, fusionne et rend la page au PMM.
 */
void kfree(void *ptr) {
    block_hdr_t  *hdr;
    free_block_t *fb;
    page_hdr_t   *pg;
    int           idx;

    if (!ptr) return;

    hdr = (block_hdr_t *)((char *)ptr - sizeof(block_hdr_t));
    idx = class_index_exact(hdr->size_class);
    if (idx < 0) return;

    pg = page_of(hdr);
    pg->used--;

    /* Réinjecter en tête de liste */
    fb              = (free_block_t *)ptr;
    fb->next        = free_lists[idx];
    free_lists[idx] = fb;

    /* Page entièrement libre → rendre au PMM */
    if (pg->used == 0) {
        remove_page_from_freelist(idx, pg);
        pmm_free_page((unsigned int)pg);
    }
}
