

// Déclaration des fonctions assembleur
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void init_idt(void);
extern void keyboard_handler_c(void);

int cursor_pos = 0;
char* video_memory = (char*)0xB8000;

void kernel_main(void) {
    char* video_memory = (char*)0xB8000;
    
    // Effacer l'écran proprement (remplir d'espaces vides)
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = 0x07;
    }
    init_idt(); // Initialisation de l'IDT et du PIC
    __asm__ __volatile__("sti");
    while (1) {
        // Boucle infinie pour maintenir le noyau actif
    }
}