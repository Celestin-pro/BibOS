extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void init_idt(void);

extern int cursor_pos;
extern char* video_memory;

// Ta Keymap AZERTY optimisée (Index = Scancode)
const char kbd_azerty[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', ')', '^', '\b',
    '\t', 'a', 'z', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '^', '$', '\n',
    0, 'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', '%', '*', 0,
    '*', 'w', 'x', 'c', 'v', 'b', 'n', ',', ';', ':', '!',
};

// C'est la fonction appelée par l'enrobage Assembleur
void keyboard_handler_c(void) {
    // Lire directement — en émulation le scancode est déjà là
    unsigned char scancode = inb(0x60);

    volatile char* video = (volatile char*)0xB8000;
    video[0] = 'A';
    video[1] = 0x02;

    outb(0x20, 0x20);
}