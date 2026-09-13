#include "./driver/keyboard.h"
#include "./driver/serial.h"
#include "./gdt/gdt.h"
#include "./interrupts/idt.h"
#include "./interrupts/regs.h"
#include "./syscall/syscall.h"
#include "./memory/multiboot.h"
#include "./memory/pmm.h"
#include "./memory/vmm.h"
#include "./memory/heap.h"
#include "./process/process.h"
#include "./process/ring3.h"
#include "./shell.h"

extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

/* Symbole de fin du kernel fourni par le linker */
extern char _kernel_end[];

int   cursor_pos   = 0;
char *video_memory = (char *)0xB8000;

/* Handler par défaut : uniquement les vecteurs 34-255 non câblés
 * explicitement ailleurs (init_idt() n'assigne que 0-33 et 0x80). Le PIC
 * est reprogrammé et masqué pour ne jamais générer ces IRQ (remap_pic()
 * dans interrupts/idt.c), donc ce handler ne devrait jamais s'exécuter en
 * usage normal — c'est un filet de sécurité, pas un chemin réel. */
void default_handler_c(void) {
    volatile char *video = (volatile char *)0xB8000;
    video[0] = 'E';
    video[1] = 0x04;
}

static const char *exception_name(unsigned int vector) {
    switch (vector) {
        case 0:  return "Division par zero";
        case 6:  return "Opcode invalide";
        case 8:  return "Double fault";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        default: return "Exception CPU";
    }
}

/* Écrit simultanément à l'écran (kputchar, via fmt_uint/fmt_hex de shell.c)
 * et sur COM1 (serial_putchar) — même logique de formatage que kprint_uint/
 * kprint_hex, juste vers deux sorties. Utile pour un dump de panic qui
 * dépasse l'écran 80x25 (le VGA scroll effacerait le haut du dump) et
 * capturable depuis l'hôte (VirtualBox peut rediriger COM1 vers un fichier). */
static void panic_print(const char *s) { kprint(s); serial_print(s); }

static void panic_print_uint(unsigned int n) {
    fmt_uint(n, kputchar);
    fmt_uint(n, serial_putchar);
}

static void panic_print_hex(unsigned int n) {
    fmt_hex(n, kputchar);
    fmt_hex(n, serial_putchar);
}

/* Évite de répéter panic_print(label)+panic_print_hex(valeur) à la main
 * pour chacun des ~12 registres du dump (risque de copier-coller une
 * ligne et d'oublier de changer la valeur, cf. review).                  */
static void panic_reg(const char *label, unsigned int value) {
    panic_print(label);
    panic_print_hex(value);
}

/* -----------------------------------------------------------------------
 * exception_handler_c – handler des exceptions CPU 0-31 (isr0..isr31 +
 * isr_common_stub dans interrupts/interrupts.asm)
 *
 *   `regs` pointe directement sur la pile de isr_common_stub (voir
 *   interrupts/regs.h) : EIP/CS/EFLAGS du point de faute, tous les
 *   registres généraux, le vecteur et le code d'erreur exact — de quoi
 *   localiser un bug (page fault dans le filesystem, GPF dans un syscall,
 *   etc.) sans redémarrer en aveugle.
 *
 *   Ring 3 vs Ring 0 : regs->cs indique le niveau de privilège au moment
 *   de la faute (bits RPL, cs & 3). Un process isolé (process_create_isolated,
 *   sa propre page directory) qui plante en Ring 3 n'a PAS besoin de faire
 *   tomber tout le système — c'est exactement ce que l'isolation mémoire
 *   est censée permettre : on le tue (comme sys_exit) et on redonne la
 *   main au scheduler. Seule une faute en Ring 0 (bug dans le kernel
 *   lui-même) reste fatale pour tout le système : là, aucune récupération
 *   n'a de sens, l'état du kernel est possiblement corrompu.             */
void exception_handler_c(regs_t *regs) {
    unsigned int cr2 = 0;
    int          fault_in_ring3 = (regs->cs & 3) != 0;

    if (regs->vector == 14)
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));

    panic_print("\n\n*** ");
    panic_print(fault_in_ring3 ? "PROCESS KILLED : " : "PANIC : ");
    panic_print(exception_name(regs->vector));
    panic_print(" ***\n");

    panic_print("vecteur=");    panic_print_uint(regs->vector);
    panic_print("  erreur=0x"); panic_print_hex(regs->error_code);
    if (regs->vector == 14) {
        panic_print("  cr2=0x");
        panic_print_hex(cr2);
    }
    if (current_proc) {
        panic_print("  pid=");
        panic_print_uint(current_proc->pid);
    }
    panic_print("\n");

    panic_reg("EIP=0x", regs->eip);
    panic_print("  "); panic_reg("CS=0x", regs->cs);
    panic_print("  "); panic_reg("EFLAGS=0x", regs->eflags);
    panic_print("\n");

    panic_reg("EAX=0x", regs->eax);
    panic_print("  "); panic_reg("EBX=0x", regs->ebx);
    panic_print("  "); panic_reg("ECX=0x", regs->ecx);
    panic_print("  "); panic_reg("EDX=0x", regs->edx);
    panic_print("\n");

    panic_reg("ESI=0x", regs->esi);
    panic_print("  "); panic_reg("EDI=0x", regs->edi);
    panic_print("  "); panic_reg("EBP=0x", regs->ebp);
    panic_print("  "); panic_reg("DS=0x", regs->ds);
    panic_print("\n");

    if (fault_in_ring3 && current_proc) {
        panic_print("*** Process Ring 3 tue, reprise du scheduler ***\n");
        current_proc->state = PROC_UNUSED;
        schedule();
        /* schedule() ne revient ici que s'il n'a trouvé AUCUN autre
         * process à lancer (jamais le cas aujourd'hui : shell/proc_a/
         * proc_b tournent toujours) — dans ce cas, rien d'autre à faire
         * que le halt ci-dessous.                                        */
    }

    panic_print("*** Systeme arrete ***\n");

    __asm__ __volatile__("cli");
    while (1) { __asm__ __volatile__("hlt"); }
}

/* -----------------------------------------------------------------------
 * double_fault_handler – cible de la task gate du vecteur 8 (gdt.c/idt.c)
 *
 *   Atteint uniquement via un vrai task switch matériel x86 : le CPU a
 *   chargé CS/EIP/ESP/SS/CR3/EFLAGS depuis df_tss (gdt.c), sur une pile
 *   dédiée (df_stack) totalement indépendante de la pile kernel qui a
 *   probablement débordé pour en arriver là — c'est précisément la
 *   situation où l'ISR normale (isr_common_stub, qui commence par un
 *   pushad sur CETTE MÊME pile déjà épuisée) referait immédiatement une
 *   faute, cascadant vers un triple fault silencieux sans ce mécanisme.
 *
 *   Un task switch ne fournit pas le contexte de la tâche fautive de la
 *   même façon qu'un pushad (pas de regs_t exploitable ici), donc pas de
 *   dump détaillé possible — juste confirmer la cause avant l'arrêt.      */
void double_fault_handler(void) {
    panic_print("\n\n*** DOUBLE FAULT *** (pile kernel probablement epuisee)\n");
    panic_print("*** Systeme arrete ***\n");

    __asm__ __volatile__("cli");
    while (1) { __asm__ __volatile__("hlt"); }
}

/* -----------------------------------------------------------------------
 * Processus de démo : affichent un caractère en haut à droite de l'écran
 * pour prouver que le multitâche tourne en parallèle du shell.
 * ----------------------------------------------------------------------- */
static void proc_a(void) {
    volatile char *vga = (volatile char *)0xB8000;
    unsigned int i = 0;
    while (1) {
        vga[158] = '0' + (i % 10);   /* colonne 79, ligne 0 */
        vga[159] = 0x0A;              /* vert sur noir       */
        i++;
        /* boucle d'attente passive (pas de sleep encore) */
        unsigned int t;
        for (t = 0; t < 2000000; t++)
            __asm__ __volatile__("nop");
    }
}

static void proc_b(void) {
    volatile char *vga = (volatile char *)0xB8000;
    unsigned int i = 0;
    while (1) {
        vga[156] = 'A' + (i % 26);   /* colonne 78, ligne 0 */
        vga[157] = 0x0C;              /* rouge sur noir      */
        i++;
        unsigned int t;
        for (t = 0; t < 3000000; t++)
            __asm__ __volatile__("nop");
    }
}

/* -----------------------------------------------------------------------
 * Fonction exécutée en Ring 3 pour vérifier que le mode user fonctionne.
 *
 * DOIT être Position Independent : pas de const char* pointant vers .rodata,
 * pas d'appels à des fonctions externes. On construit le message char par char
 * sur la stack (adressage ESP-relatif = PIC) et on fait int $0x80 directement.
 * ----------------------------------------------------------------------- */
void user_ring3_test(void) {
    volatile char msg[13];
    msg[0]='['; msg[1]='R'; msg[2]='I'; msg[3]='N'; msg[4]='G';
    msg[5]='3'; msg[6]=']'; msg[7]=' '; msg[8]='O'; msg[9]='K';
    msg[10]='!'; msg[11]='\n'; msg[12]='\0';

    /* SYS_WRITE(0) : buf=msg, len=12 */
    __asm__ volatile("int $0x80" : : "a"(0), "b"(msg), "c"(12) : "memory");

    /* SYS_EXIT(1) : termine proprement → scheduler reprend proc_a/proc_b */
    __asm__ volatile("int $0x80" : : "a"(1) : "memory");

    while (1) {}   /* jamais atteint */
}
void user_ring3_test_end(void) {}

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

    /* Le plus tôt possible : si quoi que ce soit plante avant que le shell
     * soit prêt, exception_handler_c doit pouvoir dumper sur COM1.        */
    serial_init();

    gdt_init();      /* GDT : kernel code/data + user code/data + TSS */
    init_idt();
    syscall_init();  /* gate IDT 0x80 : trap gate DPL=3 (Ring 3 → Ring 0) */
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

    /* Le TSS double fault (gdt.c) a besoin d'un CR3 valide, qui n'existe
     * qu'a partir d'ici — voir la task gate du vecteur 8 dans idt.c.      */
    tss_set_double_fault_cr3((unsigned int)vmm_get_kernel_directory());

    /* --- multitâche --- */
    process_init();          /* le thread courant devient processus 0 (shell) */
    process_create(proc_a);  /* processus 1 */
    process_create(proc_b);  /* processus 2 */
    kprint("[ SCH ] Scheduler actif (3 processus)\n");

    /* --- shell (processus 0) --- */
    kprint("[ OS1 ] Pret. Tapez 'help' pour les commandes.\n> ");

    while (1) {
        kreadline(cmd, 128);
        execute_command(cmd);
        kprint("> ");
    }
}
