#ifndef RING3_H
#define RING3_H

/* Bascule définitivement en Ring 3 à l'adresse eip avec esp comme stack.
 * Doit être appelé APRÈS gdt_init() et init_idt().
 * Configure le TSS (esp0 = stack kernel dédiée) puis fait iretd.
 * Ne retourne jamais.                                                       */
void enter_ring3(unsigned int eip, unsigned int esp);

/* Helper haut niveau : copie le code [entry, entry_end) en mémoire user,
 * alloue une stack user, configure le TSS, et saute en Ring 3.
 * Le code copié doit être Position Independent (pas d'adresses absolues).
 * Ne retourne jamais.                                                       */
void process_enter_ring3(void (*entry)(void), void (*entry_end)(void));

#endif /* RING3_H */
