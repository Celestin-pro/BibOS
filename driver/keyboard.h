#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_handler(void);

char kgetchar(void);   /* bloque jusqu'à un caractère disponible et le retourne */
void kputchar(char c); /* affiche ou traite c selon sa nature                   */

#endif
