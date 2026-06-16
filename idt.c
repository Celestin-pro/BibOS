// Définition d'une case de l'IDT (8 octets)
struct idt_entry_struct {
    unsigned short base_low;  // Les 16 bits de poids faible de l'adresse de la fonction
    unsigned short sel;       // Le sélecteur de segment (Code du noyau)
    unsigned char  always0;   // Doit toujours être à 0
    unsigned char  flags;     // Les droits d'accès (0x8E pour une interruption active)
    unsigned short base_high; // Les 16 bits de poids fort de l'adresse de la fonction
} __attribute__((packed));

// Définition du pointeur global que le processeur va lire
struct idt_ptr_struct {
    unsigned short limit;     // Taille de la table
    unsigned int   base;      // Adresse de début de la table
} __attribute__((packed));

// Notre table de 256 interruptions
struct idt_entry_struct idt[256];
struct idt_ptr_struct idt_ptr;

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
    outb(0x21, 0x0);  outb(0xA1, 0x0);  // Activer toutes les lignes
}

// Initialisation globale de l'IDT
void init_idt(void) {
    extern void keyboard_handler_asm(void);

    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (unsigned int)&idt;

    remap_pic(); // 1. On décale le conflit matériel

    // 2. On attribue la case 33 (0x21) à notre enrobage Assembleur du clavier
    idt_set_gate(33, (unsigned int)keyboard_handler_asm, 0x08, 0x8E);

    // 3. On donne le pointeur au processeur via l'assembleur
    init_idt_asm((unsigned int)&idt_ptr);
}