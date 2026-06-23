#include "./driver/keyboard.h"
#include "./memory/pmm.h"
#include "./memory/heap.h"
#include "./fs/fat32.h"
#include "./shell.h"

extern int   cursor_pos;
extern char *video_memory;

void kprint(const char *s) {
    while (*s) kputchar(*s++);
}

void kprint_uint(unsigned int n) {
    char buf[12];
    int  i = 0;
    if (n == 0) { kputchar('0'); return; }
    while (n) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) kputchar(buf[i]);
}

void kprint_hex(unsigned int n) {
    const char hex[] = "0123456789ABCDEF";
    int i;
    char buf[8];
    for (i = 7; i >= 0; i--) { buf[i] = hex[n & 0xF]; n >>= 4; }
    for (i = 0; i < 8; i++) kputchar(buf[i]);
}

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
    } else if (kstrcmp(input, "help") == 0) {
        kprint("Commandes : hello, mem, heap, cat <fichier>, help\n");
    } else {
        kprint("Commande inconnue\n");
    }
}
