#ifndef SERIAL_H
#define SERIAL_H

/* Pilote minimal du port série COM1 (0x3F8), 38400 8N1.
 * Utilisé par le handler de panic pour dumper l'état machine hors de
 * l'écran VGA : plus fiable (pas de limite 80x25, pas de scroll qui
 * efface le début du dump) et capturable facilement depuis l'hôte
 * (VirtualBox peut rediriger COM1 vers un fichier/pipe).                  */

void serial_init(void);
void serial_putchar(char c);
void serial_print(const char *s);

#endif /* SERIAL_H */
