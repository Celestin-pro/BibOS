#include "fat32.h"
#include "../driver/ata.h"
#include "../driver/keyboard.h"
#include "../shell.h"

#define SECTOR_SIZE 512
#define FAT32_EOC   0x0FFFFFF8U

static unsigned char sector_buf[SECTOR_SIZE] __attribute__((aligned(4)));
static unsigned char file_buf[SECTOR_SIZE]   __attribute__((aligned(4)));

static unsigned char fat_drive;
static unsigned int  spc;
static unsigned int  fat_start;
static unsigned int  data_start;
static unsigned int  root_clus;

static unsigned short r16(const unsigned char *p) {
    return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
}
static unsigned int r32(const unsigned char *p) {
    return (unsigned int)(p[0] | ((unsigned int)p[1] << 8)
                               | ((unsigned int)p[2] << 16)
                               | ((unsigned int)p[3] << 24));
}

static int fat32_init(void) {
    unsigned char d;
    unsigned int reserved, num_fats, fat_size;

    /* Essaie les deux drives du canal primaire */
    for (d = 0; d <= 1; d++) {
        if (ata_read_sector(d, 0, sector_buf) != 0) continue;
        if (sector_buf[510] == 0x55 && sector_buf[511] == 0xAA
            && sector_buf[13] != 0 && sector_buf[16] != 0) {
            fat_drive = d;
            goto found;
        }
    }
    return -1;

found:
    spc        = sector_buf[13];
    reserved   = r16(sector_buf + 14);
    num_fats   = sector_buf[16];
    fat_size   = r32(sector_buf + 36);
    root_clus  = r32(sector_buf + 44);
    fat_start  = reserved;
    data_start = reserved + num_fats * fat_size;
    return 0;
}

static unsigned int fat_next(unsigned int cluster) {
    unsigned int byte_off  = cluster * 4;
    unsigned int lba       = fat_start + byte_off / SECTOR_SIZE;
    unsigned int in_sector = byte_off % SECTOR_SIZE;
    ata_read_sector(fat_drive, lba, sector_buf);
    return r32(sector_buf + in_sector) & 0x0FFFFFFFU;
}

static unsigned int cluster_lba(unsigned int cluster) {
    return data_start + (cluster - 2) * spc;
}

static int name_match(const unsigned char *entry, const char *filename) {
    unsigned char n83[11];
    int i, j;

    for (i = 0; i < 11; i++) n83[i] = ' ';

    i = 0; j = 0;
    while (filename[j] && filename[j] != '.' && i < 8) {
        unsigned char c = (unsigned char)filename[j++];
        n83[i++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
    }
    if (filename[j] == '.') {
        j++;
        i = 8;
        while (filename[j] && i < 11) {
            unsigned char c = (unsigned char)filename[j++];
            n83[i++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
        }
    }

    for (i = 0; i < 11; i++)
        if (entry[i] != n83[i]) return 0;
    return 1;
}

void fat32_cat(const char *filename) {
    unsigned int dir_clus, lba, i, s;
    unsigned int found_clus = 0, file_size = 0;

    if (fat32_init() != 0) {
        kprint("[FAT32] Aucun disque FAT32 trouve (drives 0 et 1)\n");
        kprint("[FAT32] Verifie que disk.vdi est attache dans VirtualBox\n");
        return;
    }

    kprint("[FAT32] Drive : ");
    kputchar('0' + fat_drive);
    kprint("\n");

    dir_clus = root_clus;
    while (dir_clus < FAT32_EOC) {
        lba = cluster_lba(dir_clus);
        for (s = 0; s < spc; s++) {
            ata_read_sector(fat_drive, lba + s, sector_buf);
            for (i = 0; i < SECTOR_SIZE; i += 32) {
                unsigned char first = sector_buf[i];
                unsigned char attr  = sector_buf[i + 11];
                if (first == 0x00) goto done;
                if (first == 0xE5) continue;
                if (attr  == 0x0F) continue;
                if (attr  &  0x10) continue;
                if (name_match(sector_buf + i, filename)) {
                    found_clus = ((unsigned int)r16(sector_buf + i + 20) << 16)
                               | r16(sector_buf + i + 26);
                    file_size  = r32(sector_buf + i + 28);
                    goto done;
                }
            }
        }
        dir_clus = fat_next(dir_clus);
    }
done:
    if (found_clus < 2) {
        kprint("[FAT32] Fichier non trouve : ");
        kprint(filename);
        kprint("\n");
        return;
    }

    {
        unsigned int fc = found_clus, remaining = file_size;
        while (fc < FAT32_EOC && remaining > 0) {
            unsigned int base = cluster_lba(fc);
            for (s = 0; s < spc && remaining > 0; s++) {
                unsigned int k, to_read = remaining < SECTOR_SIZE ? remaining : SECTOR_SIZE;
                ata_read_sector(fat_drive, base + s, file_buf);
                for (k = 0; k < to_read; k++) kputchar(file_buf[k]);
                remaining -= to_read;
            }
            fc = fat_next(fc);
        }
    }
    kprint("\n");
}
