#include "serial.h"

extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

#define COM1 0x3F8

/* Initialise COM1 : 38400 bauds, 8N1, FIFO activé. Pas d'IRQ (on ne fait
 * que transmettre, en polling — largement suffisant pour un dump de panic
 * qui n'a lieu qu'une fois avant l'arrêt complet du système).             */
void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* désactive les interruptions du port        */
    outb(COM1 + 3, 0x80);   /* DLAB=1 pour configurer le diviseur         */
    outb(COM1 + 0, 0x03);   /* diviseur = 3 -> 38400 bauds (octet bas)    */
    outb(COM1 + 1, 0x00);   /* diviseur (octet haut)                      */
    outb(COM1 + 3, 0x03);   /* DLAB=0, 8 bits, pas de parité, 1 stop bit  */
    outb(COM1 + 2, 0xC7);   /* active le FIFO, le vide, seuil 14 octets   */
    outb(COM1 + 4, 0x0B);   /* RTS/DSR actifs                             */
}

/* Bit 5 du registre Line Status = 1 quand le buffer d'émission est vide. */
static int serial_tx_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

/* Nombre d'essais avant d'abandonner et d'écrire quand même. Ce driver est
 * utilisé par le handler de panic (kernel.c) : si le port COM1 n'est pas
 * câblé/configuré (VM sans port série, ou tout autre souci matériel), un
 * busy-wait sans limite bloquerait le handler AVANT même d'avoir affiché
 * le dump à l'écran (isr0-31 tournent avec IF=0, rien ne peut interrompre
 * une boucle infinie ici). Un octet perdu sur un port non câblé est sans
 * conséquence ; un panic qui n'affiche jamais rien en est une grave.     */
#define SERIAL_TIMEOUT 100000

void serial_putchar(char c) {
    unsigned int timeout = SERIAL_TIMEOUT;
    while (!serial_tx_empty() && --timeout) {}
    outb(COM1, (unsigned char)c);
}

void serial_print(const char *s) {
    while (*s) serial_putchar(*s++);
}
