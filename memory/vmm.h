#ifndef VMM_H
#define VMM_H

#include "pmm.h"   /* PAGE_SIZE */

/* Flags PTE/PDE -----------------------------------------------------------  */
#define PAGE_PRESENT (1 << 0)
#define PAGE_RW      (1 << 1)
#define PAGE_USER    (1 << 2)   /* bit obligatoire pour l'accès Ring 3        */

/* Layout de l'espace virtuel user ----------------------------------------- */
#define VMM_USER_CODE_BASE  0x00400000u   /* 4 Mo  – début code/data user      */
#define VMM_USER_STACK_TOP  0x08000000u   /* 128 Mo – sommet stack user (haut) */

/* Initialisation (identity-map 0-4 Mo kernel, activation paging) ---------- */
void vmm_init(void);

/* Mappe une page physique sur une adresse virtuelle dans l'espace
 * d'adressage ACTIF (voir vmm_switch_address_space), avec les flags donnés.
 * PAGE_USER dans flags → PDE et PTE reçoivent tous les deux PAGE_USER.      */
void vmm_map_page(unsigned int phys, unsigned int virt, unsigned int flags);

/* --- Isolation mémoire par process ----------------------------------------
 *
 *   Chaque process isolé a son propre page directory : il ne peut pas voir
 *   ni corrompre la mémoire user d'un autre process. PD[0] (0-4 Mo, code
 *   kernel + IRQ/syscall handlers) reste partagé par construction, sinon
 *   le kernel deviendrait inaccessible dès le premier changement de CR3.
 * -------------------------------------------------------------------------- */

/* Espace d'adressage kernel, utilisé par les process non isolés (shell,
 * process de démo...). Un seul et même page directory pour tous.           */
unsigned int *vmm_get_kernel_directory(void);

/* Alloue un espace d'adressage vierge pour un nouveau process isolé
 * (PD[0] copié depuis le kernel, tout le reste à zéro). Retourne 0 si la
 * PMM est épuisée.                                                          */
unsigned int *vmm_new_address_space(void);

/* Recharge CR3 avec pd et en fait l'espace d'adressage actif. No-op si pd
 * est nul ou déjà actif. À appeler par le scheduler avant switch_to().     */
void vmm_switch_address_space(unsigned int *pd);

/* Libère intégralement un espace d'adressage créé par vmm_new_address_space :
 * toutes les pages physiques mappées dans PD[1..1023] (code/stack user),
 * les page tables elles-mêmes, puis le page directory. PD[0] (kernel
 * partagé) n'est jamais touché. À appeler quand un process isolé se
 * termine, PAS pendant qu'on tourne encore dans cet espace d'adressage
 * (voir process_reap_zombie dans process.c).                               */
void vmm_destroy_address_space(unsigned int *pd);

/* --- API userspace — opère sur l'espace d'adressage ACTIF ----------------- */

/* Alloue une page physique (PMM) et la mappe à virt en mode user (R/W/U).   */
void vmm_map_user_page(unsigned int virt);

/* Démapper une page user : libère la page physique dans la PMM et invalide
 * l'entrée TLB correspondante.                                               */
void vmm_unmap_user_page(unsigned int virt);

/* Alloue et mappe une stack userspace (1 page sous VMM_USER_STACK_TOP).
 * Retourne l'ESP initial (= VMM_USER_STACK_TOP, stack vide).                */
unsigned int vmm_create_user_stack(void);

/* Valide un buffer [virt, virt+len) fourni par du code Ring 3 : renvoie 1
 * si toutes les pages couvertes sont mappées PAGE_USER, 0 sinon. À
 * appeler par tout syscall avant de déréférencer un pointeur utilisateur. */
int vmm_check_user_range(unsigned int virt, unsigned int len);

#endif /* VMM_H */
