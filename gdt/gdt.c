#include "gdt.h"

/* --------------------------------------------------------------------------
 * Structure d'un descripteur GDT 32 bits (8 octets)
 *
 *   [15:0]  limit_low        bits 15:0  de la limite
 *   [31:16] base_low         bits 15:0  de la base
 *   [39:32] base_mid         bits 23:16 de la base
 *   [47:40] access           octet d'accès (P DPL S Type)
 *   [51:48] limit_high       bits 19:16 de la limite (4 bits hauts)
 *   [55:52] flags            G DB L AVL (4 bits hauts du 6ème octet)
 *   [63:56] base_high        bits 31:24 de la base
 * -------------------------------------------------------------------------- */
struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_mid;
    unsigned char  access;
    unsigned char  flags_limit_high; /* [7:4] = flags, [3:0] = limit[19:16] */
    unsigned char  base_high;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

/* Interfaces assembleur */
extern void gdt_flush(unsigned int gdt_ptr_addr);
extern void tss_flush(unsigned short tss_sel);

/* Handler C du double fault (kernel.c), cible du TSS df_tss ci-dessous.      */
extern void double_fault_handler(void);

/* GDT : 7 descripteurs                                                       *
 *  [0] null    [1] kernel code  [2] kernel data                              *
 *  [3] user code  [4] user data  [5] TSS  [6] TSS double fault               */
static struct gdt_entry gdt[7];
static struct gdt_ptr   gdt_p;

/* TSS unique pour tout le kernel (mode logiciel, on change juste esp0/ss0)   */
tss_t tss;

/* Tâche matérielle dédiée au Double Fault (#8) — voir df_tss_init() pour
 * le détail, et interrupts/idt.c pour la task gate qui bascule dessus.     */
static tss_t df_tss;
static unsigned char df_stack[4096] __attribute__((aligned(16)));

/* --------------------------------------------------------------------------
 * gdt_set_entry – remplit un descripteur de segment
 *
 *   access : octet P-DPL-S-Type
 *     0x9A = kernel code  (1 001 1 1010)
 *     0x92 = kernel data  (1 001 1 0010)
 *     0xFA = user code    (1 111 1 1010)
 *     0xF2 = user data    (1 111 1 0010)
 *     0x89 = TSS 32b avail(1 000 0 1001)
 *
 *   flags  : nibble haut du 6ème octet (G DB L AVL)
 *     0x0C = 1100 → G=1 (4 Ko), DB=1 (32 bits), L=0, AVL=0
 *     0x00 = 0000 → granularité octets, utilisé pour le TSS
 * -------------------------------------------------------------------------- */
static void gdt_set_entry(int i,
                          unsigned int   base,
                          unsigned int   limit,
                          unsigned char  access,
                          unsigned char  flags)
{
    gdt[i].base_low         = base & 0xFFFF;
    gdt[i].base_mid         = (base >> 16) & 0xFF;
    gdt[i].base_high        = (base >> 24) & 0xFF;
    gdt[i].limit_low        = limit & 0xFFFF;
    gdt[i].flags_limit_high = ((flags & 0x0F) << 4) | ((limit >> 16) & 0x0F);
    gdt[i].access           = access;
}

/* --------------------------------------------------------------------------
 * tss_init – initialise la structure TSS et son descripteur dans la GDT
 * -------------------------------------------------------------------------- */
static void tss_init(void)
{
    unsigned int i;
    unsigned char *p = (unsigned char *)&tss;
    for (i = 0; i < sizeof(tss_t); i++)
        p[i] = 0;

    tss.ss0        = SEG_KERNEL_DATA;  /* segment stack Ring 0               */
    tss.esp0       = 0;                /* sera mis à jour avant d'aller en R3*/
    tss.iomap_base = sizeof(tss_t);    /* pas de bitmap E/S                  */

    /* Descripteur TSS dans la GDT[5]
     *   base  = adresse de la structure tss
     *   limit = sizeof(tss) - 1  (en octets, G=0)
     *   access= 0x89 = Present, DPL=0, type=9 (TSS 32b available)
     *   flags = 0x00  (granularité octets)                                   */
    unsigned int tss_base  = (unsigned int)&tss;
    unsigned int tss_limit = sizeof(tss_t) - 1;
    gdt_set_entry(5, tss_base, tss_limit, 0x89, 0x00);
}

/* --------------------------------------------------------------------------
 * df_tss_init – configure la tâche matérielle du double fault
 *
 *   Contrairement au TSS principal (mode "logiciel", seul esp0/ss0 changent
 *   pour la transition Ring3→Ring0), celui-ci sert à une VRAIE tâche
 *   matérielle x86 : la task gate du vecteur 8 (interrupts/idt.c) force un
 *   task switch complet (CS/EIP/ESP/SS/CR3/EFLAGS chargés depuis df_tss),
 *   indépendamment de l'état — potentiellement corrompu ou en overflow de
 *   pile — au moment de la faute.
 *
 *   Utilité : un stack overflow du kernel déclenche un Page Fault (accès
 *   sous la pile) ; si la PREMIÈRE instruction du handler normal (pushad)
 *   tente d'écrire sur cette même pile déjà épuisée, ça refait immédiatement
 *   une faute → double fault (#8) → sans pile dédiée, le troisième essai
 *   (triple fault) redémarre la machine sans le moindre message. Avec ce
 *   TSS, le CPU bascule matériellement sur df_stack (jamais touchée par le
 *   code normal), donc le message "*** DOUBLE FAULT ***" a une chance
 *   réelle de s'afficher avant l'arrêt.
 *
 *   cr3 est laissé à 0 ici : la pagination n'est pas encore active à
 *   l'appel de gdt_init() (vmm_init() tourne après). tss_set_double_fault_cr3()
 *   le renseigne une fois le page directory kernel connu — voir kernel.c.
 * -------------------------------------------------------------------------- */
static void df_tss_init(void)
{
    unsigned int i;
    unsigned char *p = (unsigned char *)&df_tss;
    for (i = 0; i < sizeof(tss_t); i++)
        p[i] = 0;

    df_tss.cs         = SEG_KERNEL_CODE;
    df_tss.ss         = SEG_KERNEL_DATA;
    df_tss.ds         = SEG_KERNEL_DATA;
    df_tss.es         = SEG_KERNEL_DATA;
    df_tss.fs         = SEG_KERNEL_DATA;
    df_tss.gs         = SEG_KERNEL_DATA;
    df_tss.esp        = (unsigned int)(df_stack + sizeof(df_stack));
    df_tss.eip        = (unsigned int)double_fault_handler;
    df_tss.eflags     = 0x00000002;   /* bit 1 réservé à 1, IF=0            */
    df_tss.cr3        = 0;            /* renseigné après vmm_init()          */
    df_tss.iomap_base = sizeof(tss_t);

    /* Descripteur TSS dans la GDT[6] — même format que le TSS principal,
     * sélecteur SEG_DF_TSS (0x30). Jamais chargé via ltr : ce n'est pas la
     * tâche COURANTE, seulement la CIBLE de la task gate du vecteur 8.     */
    gdt_set_entry(6, (unsigned int)&df_tss, sizeof(tss_t) - 1, 0x89, 0x00);
}

/* --------------------------------------------------------------------------
 * gdt_init – point d'entrée public
 *   1. Remplit les 6 descripteurs
 *   2. Charge la nouvelle GDT (lgdt) via gdt_flush
 *   3. Charge le TR (task register) via tss_flush
 * -------------------------------------------------------------------------- */
void gdt_init(void)
{
    gdt_p.limit = (unsigned short)(sizeof(struct gdt_entry) * 7 - 1);
    gdt_p.base  = (unsigned int)&gdt;

    /* [0] Descripteur nul obligatoire */
    gdt_set_entry(0, 0, 0, 0x00, 0x00);

    /* [1] Kernel code  – sélecteur 0x08, DPL=0, execute/read, 32 bits, 4 Go */
    gdt_set_entry(1, 0x00000000, 0xFFFFF, 0x9A, 0x0C);

    /* [2] Kernel data  – sélecteur 0x10, DPL=0, read/write,   32 bits, 4 Go */
    gdt_set_entry(2, 0x00000000, 0xFFFFF, 0x92, 0x0C);

    /* [3] User code    – sélecteur 0x18|3=0x1B, DPL=3, execute/read          */
    gdt_set_entry(3, 0x00000000, 0xFFFFF, 0xFA, 0x0C);

    /* [4] User data    – sélecteur 0x20|3=0x23, DPL=3, read/write            */
    gdt_set_entry(4, 0x00000000, 0xFFFFF, 0xF2, 0x0C);

    /* [5] TSS          – sélecteur 0x28, initialisé par tss_init()            */
    tss_init();

    /* [6] TSS double fault – sélecteur 0x30, initialisé par df_tss_init()    */
    df_tss_init();

    /* Charge la GDT et recharge tous les registres de segment */
    gdt_flush((unsigned int)&gdt_p);

    /* Charge le Task Register avec le sélecteur TSS (PAS df_tss : ce n'est
     * jamais la tâche courante, seulement la cible d'une task gate).        */
    tss_flush(SEG_TSS);
}

/* --------------------------------------------------------------------------
 * tss_set_esp0 – appeler avant chaque entrée en Ring 3
 *   Met à jour le pointeur de stack kernel que le CPU restaurera
 *   automatiquement lors d'une transition Ring 3 → Ring 0 (interruption).
 * -------------------------------------------------------------------------- */
void tss_set_esp0(unsigned int esp0)
{
    tss.esp0 = esp0;
}

/* --------------------------------------------------------------------------
 * tss_set_double_fault_cr3 – renseigne le CR3 de la tâche double fault
 *   À appeler après vmm_init() : voir kernel.c.
 * -------------------------------------------------------------------------- */
void tss_set_double_fault_cr3(unsigned int cr3)
{
    df_tss.cr3 = cr3;
}
