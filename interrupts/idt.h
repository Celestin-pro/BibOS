#ifndef IDT_H
#define IDT_H

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags);
void remap_pic(void);
void init_idt(void);

#endif // IDT_H