#include "../gdt/gdt.h"

// Définition d'une case de l'IDT (8 octets)
struct idt_entry_struct {
    unsigned short base_low;
    unsigned short sel;
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_high;
} __attribute__((packed)) __attribute__((aligned(8)));

struct idt_ptr_struct {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed)) __attribute__((aligned(4)));

// On force le compilateur à garder ces variables exactes et visibles
__attribute__((aligned(16))) struct idt_entry_struct idt[256];
__attribute__((aligned(16))) struct idt_ptr_struct idt_ptr;

// Fonction de communication matérielle
extern void init_idt_asm(unsigned int idt_ptr_addr);

// Permet de remplir une case spécifique de l'IDT
void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

// Reprogrammation du PIC pour décaler les interruptions matérielles à la case 32
void remap_pic(void) {
    // Fonctions inb/outb externes
    extern void outb(unsigned short port, unsigned char data);
    
    outb(0x20, 0x11); outb(0xA0, 0x11); // Initialisation
    outb(0x21, 0x20); outb(0xA1, 0x28); // Master PIC = case 32, Slave PIC = case 40
    outb(0x21, 0x04); outb(0xA1, 0x02); // Liaison entre les PICs
    outb(0x21, 0x01); outb(0xA1, 0x01); // Mode 8086
    // Au lieu de outb(0x21, 0x0); outb(0xA1, 0x0);
    outb(0x21, 0xFC);  // 0xFC = 11111100 -> IRQ0 (timer) + IRQ1 (clavier) activés
    outb(0xA1, 0xFF);  // 0xFF = 11111111 -> toutes les lignes esclave coupées
}

/* Configure le PIT canal 0 à la fréquence demandée (en Hz).
 * Fréquence d'entrée du PIT : 1 193 180 Hz.
 * À 100 Hz → diviseur = 11 932 → tick toutes les ~10 ms.          */
static void pit_init(unsigned int hz) {
    extern void outb(unsigned short port, unsigned char data);
    unsigned int divisor = 1193180 / hz;
    outb(0x43, 0x36);                        /* canal 0, lobyte/hibyte, mode 3 */
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

// Initialisation globale de l'IDT
extern void default_handler_asm(void);
extern void keyboard_handler_asm(void);
extern void irq0_handler_asm(void);

/* isr0..isr31 : un stub par exception CPU (interrupts.asm). Ils dépilent
 * correctement le code d'erreur que le CPU pousse pour #8, #10-14, #17
 * (dont #14 Page Fault) avant de faire iretd — default_handler_asm ne le
 * fait pas et désaligne la pile sur ces vecteurs, sautant à une adresse
 * arbitraire au retour. Voir isr_common_stub dans interrupts.asm.        */
extern void isr0(void),  isr1(void),  isr2(void),  isr3(void);
extern void isr4(void),  isr5(void),  isr6(void),  isr7(void);
extern void isr8(void),  isr9(void),  isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void);
extern void isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void);
extern void isr28(void), isr29(void), isr30(void), isr31(void);

static void (*const exception_stubs[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

void init_idt(void) {
    int i;
    for (i = 0; i < 256; i++)
        idt_set_gate(i, (unsigned int)default_handler_asm, 0x08, 0x8E);

    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (unsigned int)&idt;

    remap_pic();

    for (i = 0; i < 32; i++)
        idt_set_gate(i, (unsigned int)exception_stubs[i], 0x08, 0x8E); /* exceptions CPU */

    /* Vecteur 8 (Double Fault) : task gate vers un TSS dédié (gdt.c) au
     * lieu de l'interrupt gate isr8 générique. Un double fault survient
     * typiquement quand la pile kernel déborde et qu'un handler normal
     * refait immédiatement une faute en essayant d'y écrire (pushad) — la
     * task gate force un vrai changement de pile matériel (ESP chargé
     * depuis df_tss, jamais celle du kernel) AVANT que le moindre pushad
     * s'exécute, donnant une vraie chance d'afficher un message au lieu
     * d'un triple fault silencieux. base=0 : ignoré pour une task gate,
     * seul le sélecteur TSS (sel) compte. flags=0x85 : present, DPL=0,
     * S=0, type=0101 (task gate 32 bits).                                 */
    idt_set_gate(8, 0, SEG_DF_TSS, 0x85);

    idt_set_gate(32, (unsigned int)irq0_handler_asm,     0x08, 0x8E); /* timer  */
    idt_set_gate(33, (unsigned int)keyboard_handler_asm, 0x08, 0x8E); /* clavier */

    pit_init(100);   /* 100 Hz → context switch toutes les ~10 ms */

    init_idt_asm((unsigned int)&idt_ptr);
}