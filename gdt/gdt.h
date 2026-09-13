#ifndef GDT_H
#define GDT_H

/* Sélecteurs GDT ---------------------------------------------------------- */
#define SEG_KERNEL_CODE  0x08   /* GDT[1], DPL=0                            */
#define SEG_KERNEL_DATA  0x10   /* GDT[2], DPL=0                            */
#define SEG_USER_CODE    0x1B   /* GDT[3], DPL=3  (0x18 | RPL=3)           */
#define SEG_USER_DATA    0x23   /* GDT[4], DPL=3  (0x20 | RPL=3)           */
#define SEG_TSS          0x28   /* GDT[5]                                   */
#define SEG_DF_TSS       0x30   /* GDT[6] – TSS dédié au Double Fault (#8)  */

/* Structure TSS 32 bits (Intel Vol.3, §7.2.1) ----------------------------- */
typedef struct {
    unsigned int  link;         /* 0x00 – sélecteur tâche précédente        */
    unsigned int  esp0;         /* 0x04 – stack Ring 0 (utilisé par le CPU) */
    unsigned int  ss0;          /* 0x08 – segment stack Ring 0              */
    unsigned int  esp1;         /* 0x0C                                     */
    unsigned int  ss1;          /* 0x10                                     */
    unsigned int  esp2;         /* 0x14                                     */
    unsigned int  ss2;          /* 0x18                                     */
    unsigned int  cr3;          /* 0x1C – répertoire de pages               */
    unsigned int  eip;          /* 0x20                                     */
    unsigned int  eflags;       /* 0x24                                     */
    unsigned int  eax;          /* 0x28                                     */
    unsigned int  ecx;          /* 0x2C                                     */
    unsigned int  edx;          /* 0x30                                     */
    unsigned int  ebx;          /* 0x34                                     */
    unsigned int  esp;          /* 0x38                                     */
    unsigned int  ebp;          /* 0x3C                                     */
    unsigned int  esi;          /* 0x40                                     */
    unsigned int  edi;          /* 0x44                                     */
    unsigned int  es;           /* 0x48                                     */
    unsigned int  cs;           /* 0x4C                                     */
    unsigned int  ss;           /* 0x50                                     */
    unsigned int  ds;           /* 0x54                                     */
    unsigned int  fs;           /* 0x58                                     */
    unsigned int  gs;           /* 0x5C                                     */
    unsigned int  ldt;          /* 0x60                                     */
    unsigned short trap;        /* 0x64                                     */
    unsigned short iomap_base;  /* 0x66 – offset bitmap E/S (=sizeof si absent) */
} __attribute__((packed)) tss_t;

/* Initialise la GDT complète (null + kernel + user + TSS + TSS double
 * fault) et charge le TR.                                                  */
void gdt_init(void);

/* Met à jour esp0 dans le TSS avant chaque entrée en Ring 3.               */
void tss_set_esp0(unsigned int esp0);

/* Renseigne le CR3 du TSS double fault. À appeler après vmm_init() (avant
 * ça, aucun page directory n'existe encore) : voir interrupts/idt.c pour
 * la task gate du vecteur 8 qui bascule sur ce TSS.                        */
void tss_set_double_fault_cr3(unsigned int cr3);

#endif /* GDT_H */
