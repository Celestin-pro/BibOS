#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#define MULTIBOOT_MAGIC    0x2BADB002
#define MULTIBOOT_FLAG_MMAP (1 << 6)

typedef struct {
    unsigned int  flags;
    unsigned int  mem_lower;    /* Ko sous 1 Mo  */
    unsigned int  mem_upper;    /* Ko au-dessus de 1 Mo */
    unsigned int  boot_device;
    unsigned int  cmdline;
    unsigned int  mods_count;
    unsigned int  mods_addr;
    unsigned int  syms[4];
    unsigned int  mmap_length;
    unsigned int  mmap_addr;
} __attribute__((packed)) multiboot_info_t;

/* Une entrée de la memory map BIOS fournie par GRUB */
typedef struct {
    unsigned int       size; /* taille de cette entrée sans ce champ */
    unsigned long long addr;
    unsigned long long len;
    unsigned int       type; /* 1 = RAM utilisable */
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif
