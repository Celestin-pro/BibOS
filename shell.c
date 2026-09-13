#include "./driver/keyboard.h"
#include "./memory/pmm.h"
#include "./memory/heap.h"
#include "./fs/fat32.h"
#include "./process/process.h"
#include "./process/ring3.h"
#include "./shell.h"

extern void user_ring3_test(void);
extern void user_ring3_test_end(void);

extern int   cursor_pos;
extern char *video_memory;

/* Lance le test ring3 dans un process dédié : quand sys_exit le termine,
 * seul ce process est marqué PROC_UNUSED — le shell (process 0) survit.  */
static void ring3_launch(void) {
    process_enter_ring3(user_ring3_test, user_ring3_test_end);
}

void kprint(const char *s) {
    while (*s) kputchar(*s++);
}

/* Formatage décimal/hexadécimal générique : `put` reçoit chaque caractère
 * produit. kprint_uint/kprint_hex ci-dessous les appellent avec kputchar
 * (VGA) ; le dump de panic (kernel.c) les appelle aussi avec serial_putchar
 * — même conversion, deux sorties, sans dupliquer la logique.            */
void fmt_uint(unsigned int n, void (*put)(char)) {
    char buf[12];
    int  i = 0;
    if (n == 0) { put('0'); return; }
    while (n) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) put(buf[i]);
}

void fmt_hex(unsigned int n, void (*put)(char)) {
    const char hex[] = "0123456789ABCDEF";
    int i;
    for (i = 28; i >= 0; i -= 4) put(hex[(n >> i) & 0xF]);
}

void kprint_uint(unsigned int n) { fmt_uint(n, kputchar); }
void kprint_hex(unsigned int n)  { fmt_hex(n, kputchar); }

void kreadline(char *buf, int max) {
    int  i = 0;
    char c;
    while (i < max - 1) {
        c = kgetchar();
        kputchar(c);
        if (c == '\n') break;
        if (c == '\b') { if (i > 0) i--; }
        else           buf[i++] = c;
    }
    buf[i] = '\0';
}

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

static int kstrncmp(const char *a, const char *b, int n) {
    while (n-- > 0) {
        if (*a != *b) return *a - *b;
        if (*a == '\0') return 0;
        a++; b++;
    }
    return 0;
}

void execute_command(char *input) {
    int i;

    for (i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i]     = ' ';
        video_memory[i + 1] = 0x07;
    }
    cursor_pos = 0;

    if (kstrncmp(input, "cat ", 4) == 0) {
        fat32_cat(input + 4);
    } else if (kstrcmp(input, "hello") == 0) {
        kprint("Bonjour !\n");
    } else if (kstrcmp(input, "mem") == 0) {
        kprint("Pages libres : ");
        kprint_uint(pmm_free_count());
        kprint("\n");
    } else if (kstrcmp(input, "heap") == 0) {
        void *a = kmalloc(10);
        void *b = kmalloc(100);
        void *c = kmalloc(500);
        kprint("kmalloc(10)  -> 0x"); kprint_hex((unsigned int)a); kprint("\n");
        kprint("kmalloc(100) -> 0x"); kprint_hex((unsigned int)b); kprint("\n");
        kprint("kmalloc(500) -> 0x"); kprint_hex((unsigned int)c); kprint("\n");
        kprint("Pages libres avant kfree : "); kprint_uint(pmm_free_count()); kprint("\n");
        kfree(a); kfree(b); kfree(c);
        kprint("Pages libres apres kfree : "); kprint_uint(pmm_free_count()); kprint("\n");
    } else if (kstrcmp(input, "ring3") == 0) {
        kprint("[ R3 ] Lancement du process ring3 (espace d'adressage isole)...\n");
        process_create_isolated(ring3_launch);
    } else if (kstrcmp(input, "leaktest") == 0) {
        /* TEST : lance 20 process ring3 isoles en sequence et verifie que
         * pmm_free_count() revient a sa valeur de depart apres chacun.
         * Avant le fix de process_reap_zombie() : chaque cycle perdait
         * 1 page directory + 1 page table (PD[1]) + 1 page code user +
         * 1 page stack user + 1 page stack kernel du process = 5 pages,
         * qui ne revenaient jamais a la PMM. */
        unsigned int before = pmm_free_count();
        unsigned int after;
        int n;
        unsigned int t;

        kprint("[ LEAK ] Pages libres avant : ");
        kprint_uint(before);
        kprint("\n");

        for (n = 0; n < 20; n++) {
            process_create_isolated(ring3_launch);
            /* laisser tourner le scheduler (100 Hz) le temps que ce
             * process demarre, fasse ses 2 syscalls, sorte, ET soit
             * reape par le schedule() suivant.                        */
            for (t = 0; t < 5000000; t++)
                __asm__ __volatile__("nop");
        }

        after = pmm_free_count();
        kprint("[ LEAK ] Pages libres apres 20 cycles ring3 : ");
        kprint_uint(after);
        if (after == before)
            kprint("  -> OK, aucune fuite\n");
        else {
            kprint("  -> FUITE DETECTEE (delta ");
            kprint_uint(before - after);
            kprint(" pages)\n");
        }
    } else if (kstrcmp(input, "help") == 0) {
        kprint("Commandes : hello, mem, heap, cat <fichier>, ring3, leaktest, help\n");
    } else {
        kprint("Commande inconnue\n");
    }
}
