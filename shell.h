#ifndef __SHELL_H__
#define __SHELL_H__

void kprint(const char *s);
void kprint_uint(unsigned int n);
void kprint_hex(unsigned int n);
void kreadline(char *buf, int max);
int  kstrcmp(const char *a, const char *b);
void execute_command(char *input);

#endif /* __SHELL_H__ */