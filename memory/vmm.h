#ifndef VMM_H
#define VMM_H

#define PAGE_PRESENT (1 << 0)
#define PAGE_RW      (1 << 1)
#define PAGE_USER    (1 << 2)

void vmm_init(void);
void vmm_map_page(unsigned int phys, unsigned int virt, unsigned int flags);

#endif
