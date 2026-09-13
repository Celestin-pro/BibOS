#include "ata.h"

/* Offsets depuis la base du canal */
#define ATA_OFF_DATA        0
#define ATA_OFF_SECTOR_CNT  2
#define ATA_OFF_LBA_LO      3
#define ATA_OFF_LBA_MID     4
#define ATA_OFF_LBA_HI      5
#define ATA_OFF_DRIVE_HEAD  6
#define ATA_OFF_CMD_STATUS  7

/* Bases des deux canaux IDE */
#define ATA_PRIMARY_BASE    0x1F0
#define ATA_SECONDARY_BASE  0x170

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

/* drive : 0,1 = canal primaire ; 2,3 = canal secondaire */
static unsigned short ata_base(unsigned char drive) {
    return (drive >= 2) ? ATA_SECONDARY_BASE : ATA_PRIMARY_BASE;
}

static void ata_delay(unsigned short base) {
    inb(base + ATA_OFF_CMD_STATUS);
    inb(base + ATA_OFF_CMD_STATUS);
    inb(base + ATA_OFF_CMD_STATUS);
    inb(base + ATA_OFF_CMD_STATUS);
}

static int wait_bsy(unsigned short base) {
    int t = ATA_TIMEOUT;
    while ((inb(base + ATA_OFF_CMD_STATUS) & ATA_BSY) && --t > 0);
    return t > 0 ? 0 : -1;
}

static int wait_drq(unsigned short base) {
    int t = ATA_TIMEOUT;
    unsigned char s;
    while (--t > 0) {
        s = inb(base + ATA_OFF_CMD_STATUS);
        if (s & ATA_ERR) return -1;
        if (s & ATA_DRQ) return 0;
    }
    return -1;
}

int ata_read_sector(unsigned char drive, unsigned int lba, unsigned char *buf) {
    unsigned short base = ata_base(drive);
    unsigned char  dev  = drive & 1;   /* 0 = master, 1 = slave sur le canal */
    unsigned short *w   = (unsigned short *)(void *)buf;
    int i;

    if (wait_bsy(base) != 0) return -1;

    outb(base + ATA_OFF_DRIVE_HEAD, 0xE0 | (dev << 4) | ((lba >> 24) & 0x0F));
    ata_delay(base);

    /* 0xFF sur le bus = aucun périphérique */
    if (inb(base + ATA_OFF_CMD_STATUS) == 0xFF) return -1;

    outb(base + ATA_OFF_SECTOR_CNT, 1);
    outb(base + ATA_OFF_LBA_LO,  (unsigned char)(lba));
    outb(base + ATA_OFF_LBA_MID, (unsigned char)(lba >>  8));
    outb(base + ATA_OFF_LBA_HI,  (unsigned char)(lba >> 16));
    outb(base + ATA_OFF_CMD_STATUS, ATA_CMD_READ);
    ata_delay(base);

    if (wait_bsy(base) != 0) return -1;
    if (wait_drq(base) != 0) return -1;

    for (i = 0; i < 256; i++)
        w[i] = inw(base + ATA_OFF_DATA);
    return 0;
}
