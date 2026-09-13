#ifndef ATA_H
#define ATA_H

/* Lit un secteur de 512 octets depuis le disque.
 * drive : 0 = primary master,   1 = primary slave
 *         2 = secondary master, 3 = secondary slave
 * lba   : numéro de secteur (LBA28)
 * buf   : buffer de 512 octets minimum, aligné sur 2 octets
 * Retourne 0 si OK, -1 si erreur/timeout.
 */
int ata_read_sector(unsigned char drive, unsigned int lba, unsigned char *buf);

#endif
