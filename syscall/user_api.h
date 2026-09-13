#ifndef USER_API_H
#define USER_API_H

/* -----------------------------------------------------------------------
 * Interface syscall pour le code Ring 3 (userspace).
 * NE PAS inclure dans les fichiers kernel — utiliser syscall/syscall.h.
 *
 * Convention : EAX=num, EBX=arg1, ECX=arg2, EDX=arg3, retour dans EAX.
 * ----------------------------------------------------------------------- */

#include "syscall.h"   /* SYS_WRITE, SYS_EXIT, SYS_YIELD, SYS_READ */

/* Primitives inline asm ------------------------------------------------- */

#define syscall0(num) __extension__ ({                      \
    unsigned int _r;                                        \
    __asm__ volatile("int $0x80"                            \
        : "=a"(_r) : "a"((unsigned int)(num)) : "memory"); \
    _r; })

#define syscall2(num, a1, a2) __extension__ ({              \
    unsigned int _r;                                        \
    __asm__ volatile("int $0x80"                            \
        : "=a"(_r)                                          \
        : "a"((unsigned int)(num)),                         \
          "b"((unsigned int)(a1)),                          \
          "c"((unsigned int)(a2))                           \
        : "memory");                                        \
    _r; })

#define syscall3(num, a1, a2, a3) __extension__ ({          \
    unsigned int _r;                                        \
    __asm__ volatile("int $0x80"                            \
        : "=a"(_r)                                          \
        : "a"((unsigned int)(num)),                         \
          "b"((unsigned int)(a1)),                          \
          "c"((unsigned int)(a2)),                          \
          "d"((unsigned int)(a3))                           \
        : "memory");                                        \
    _r; })

/* API haut niveau -------------------------------------------------------- */

#define sys_write(buf, len)  syscall2(SYS_WRITE, (buf), (len))
#define sys_read(buf, len)   syscall2(SYS_READ,  (buf), (len))
#define sys_yield()          syscall0(SYS_YIELD)
#define sys_exit(code)       syscall0(SYS_EXIT)

#endif /* USER_API_H */
