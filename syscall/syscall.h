#ifndef SYSCALL_H
#define SYSCALL_H

/* ---- Numéros de syscall (valeur de EAX avant int $0x80) ----------------- */
#define SYS_WRITE  0   /* write(const char *buf, unsigned int len) → nbytes  */
#define SYS_EXIT   1   /* exit(unsigned int code)          → ne revient pas  */
#define SYS_YIELD  2   /* yield()                          → 0               */
#define SYS_READ   3   /* read(char *buf, unsigned int len) → nbytes lus     */

/* ---- Codes d'erreur (retour EAX < 0 côté user) --------------------------- */
#define SYS_ENOSYS ((unsigned int)-1)   /* numéro de syscall inconnu          */
#define SYS_EFAULT ((unsigned int)-2)   /* buffer hors espace user mappé      */

/* ---- Interface kernel --------------------------------------------------- */

/* Enregistre la gate IDT 0x80 (trap gate, DPL=3). À appeler après init_idt. */
void syscall_init(void);

/* Dispatcher C, appelé depuis syscall_handler_asm.                          */
unsigned int syscall_dispatch(unsigned int num,
                              unsigned int arg1,
                              unsigned int arg2,
                              unsigned int arg3);

#endif /* SYSCALL_H */
