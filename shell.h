#ifndef __SHELL_H__
#define __SHELL_H__

/* Formatage générique décimal/hexadécimal, paramétré par la sortie (un
 * simple void(char)) : kprint_uint/kprint_hex (VGA) et le dump de panic
 * (kernel.c, VGA + série) partagent la même logique de conversion au lieu
 * de la dupliquer pour chaque sortie.                                     */
void fmt_uint(unsigned int n, void (*put)(char c));
void fmt_hex(unsigned int n, void (*put)(char c));

void kprint(const char *s);
void kprint_uint(unsigned int n);
void kprint_hex(unsigned int n);
void kreadline(char *buf, int max);
int  kstrcmp(const char *a, const char *b);
void execute_command(char *input);

#endif /* __SHELL_H__ */