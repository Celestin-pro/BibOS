#ifndef PROCESS_H
#define PROCESS_H

#include "../memory/pmm.h"

#define MAX_PROCS  8

typedef enum { PROC_UNUSED, PROC_READY, PROC_RUNNING } proc_state_t;

typedef struct {
    unsigned int   pid;
    unsigned int   esp;              /* stack pointer sauvegardé lors du context switch */
    unsigned char *stack;            /* base de la stack allouée (pour libération PMM)  */
    unsigned int  *page_directory;   /* espace d'adressage (CR3) de ce process          */
    proc_state_t   state;
    int            pending_free;     /* 1 : stack/page_directory à rendre à la PMM      */
} process_t;

extern process_t *current_proc;

void process_init(void);

/* Process kernel-only : partage l'espace d'adressage du kernel (pas
 * d'isolation — utilisé pour du code de confiance comme proc_a/proc_b).   */
void process_create(void (*entry)(void));

/* Process isolé : reçoit son propre page directory (voir
 * vmm_new_address_space). À utiliser pour tout code destiné à tourner en
 * Ring 3 (process_enter_ring3), pour qu'un crash/bug ne puisse pas
 * corrompre la mémoire d'un autre process.                                 */
void process_create_isolated(void (*entry)(void));

void schedule(void);

#endif
