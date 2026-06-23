#include "./driver/keyboard.h"
#include "./interrupts/idt.h"
#include "./memory/multiboot.h"
#include "./memory/pmm.h"
#include "./memory/vmm.h"
#include "./memory/heap.h"
#include "./shell.h"

extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

/* Symbole de fin du kernel fourni par le linker */
extern char _kernel_end[];

int   cursor_pos  = 0;
char *video_memory = (char *)0xB8000;

void default_handler_c(void) {
    volatile char *video = (volatile char *)0xB8000;
    video[0] = 'E';
    video[1] = 0x04;
}

/* --- point d'entrée --- */

void kernel_main(unsigned int mb_magic, multiboot_info_t *mbi) {
    int i;
    char cmd[128];

    /* Effacer l'écran */
    for (i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i]     = ' ';
        video_memory[i + 1] = 0x07;
    }
    cursor_pos = 0;

    init_idt();
    __asm__ __volatile__("sti");

    /* --- mémoire physique --- */
    if (mb_magic == MULTIBOOT_MAGIC) {
        pmm_init(mbi, (unsigned int)&_kernel_end);
        kprint("[ PMM ] Memoire physique initialisee. Pages libres : ");
        kprint_uint(pmm_free_count());
        kprint("\n");
    } else {
        kprint("[ ERR ] Magic Multiboot invalide !\n");
    }

    /* --- mémoire virtuelle --- */
    vmm_init();
    kprint("[ VMM ] Pagination activee (identity-map 0-4Mo)\n");

    /* --- shell --- */
    kprint("[ OS1 ] Pret. Tapez 'help' pour les commandes.\n> ");

    while (1) {
        kreadline(cmd, 128);
        execute_command(cmd);
        kprint("> ");
    }
}
