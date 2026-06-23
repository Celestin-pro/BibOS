#ifndef PMM_H
#define PMM_H

#include "multiboot.h"

#define PAGE_SIZE 4096

void         pmm_init(multiboot_info_t *mbi, unsigned int kernel_end);
unsigned int pmm_alloc_page(void);
void         pmm_free_page(unsigned int addr);
unsigned int pmm_free_count(void);

#endif
