#include "ata.h"

#define ATA_DATA       0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_CMD        0x1F7
#define ATA_STATUS     0x1F7
#define ATA_CMD_READ   0x20
#define ATA_BSY        0x80
#define ATA_DRQ        0x08
#define ATA_ERR        0x01
#define ATA_TIMEOUT    100000

extern unsigned char inb(unsigned short port);
extern void          outb(unsigned short port, unsigned char data);

static unsigned short inw(unsigned short port) {
    unsigned short v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void ata_delay(void) {
    inb(ATA_STATUS); inb(ATA_STATUS);
    inb(ATA_STATUS); inb(ATA_STATUS);
}

static int wait_bsy(void) {
    int t = ATA_TIMEOUT;
    while ((inb(ATA_STATUS) & ATA_BSY) && --t > 0);
    return t > 0 ? 0 : -1;
}

static int wait_drq(void) {
    int t = ATA_TIMEOUT;
    unsigned char s;
    while (--t > 0) {
        s = inb(ATA_STATUS);
        if (s & ATA_ERR) return -1;
        if (s & ATA_DRQ) return 0;
    }
    return -1;
}

int ata_read_sector(unsigned char drive, unsigned int lba, unsigned char *buf) {
    unsigned short *w = (unsigned short *)(void *)buf;
    int i;

    if (wait_bsy() != 0) return -1;

    outb(ATA_DRIVE_HEAD, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    ata_delay();

    /* 0xFF sur le bus = aucun périphérique */
    if (inb(ATA_STATUS) == 0xFF) return -1;

    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,  (unsigned char)(lba));
    outb(ATA_LBA_MID, (unsigned char)(lba >>  8));
    outb(ATA_LBA_HI,  (unsigned char)(lba >> 16));
    outb(ATA_CMD,     ATA_CMD_READ);
    ata_delay();

    if (wait_bsy() != 0) return -1;
    if (wait_drq() != 0) return -1;

    for (i = 0; i < 256; i++)
        w[i] = inw(ATA_DATA);
    return 0;
}
