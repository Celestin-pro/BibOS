
// Déclaration des fonctions assembleur
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void init_idt(void);
extern void keyboard_handler_c(void);

int cursor_pos = 0;
char* video_memory = (char*)0xB8000;

void default_handler_c(void) {
    volatile char* video = (volatile char*)0xB8000;
    // Affiche un 'E' rouge (code couleur 0x04) tout en haut à gauche de l'écran
    video[0] = 'E'; 
    video[1] = 0x04;
}
void kernel_main(void) {
    char* vga = (char*)0xB8000;
    
    
    // Efface l'écran
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = 0x07;
    }

    init_idt();
   

    __asm__ __volatile__("sti");
    

    while(1) {}
}