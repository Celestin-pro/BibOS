#!/bin/bash
set -e  # arrête tout si une commande échoue
ROOT=$(dirname "$0")   # répertoire du script
cd "$ROOT"

echo "=== Nettoyage ==="
rm -rf *.o myos.bin mon_os.iso isodir disk.img monOS.vdi

echo "=== 1. Compilation ==="
nasm -f elf32 boot/boot.asm -o boot.o
nasm -f elf32 interrupts/interrupts.asm -o interrupts.o

gcc -m32 -c kernel.c           -o kernel.o  -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c interrupts/idt.c   -o idt.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c driver/keyboard.c  -o driver.o  -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c memory/pmm.c       -o pmm.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c memory/vmm.c       -o vmm.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c memory/heap.c      -o heap.o    -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c shell.c            -o shell.o   -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c driver/ata.c      -o ata.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c fs/fat32.c        -o fat32.o   -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie

echo "=== 2. Linkage ==="
gcc -m32 -T boot/linker.ld -o myos.bin \
    -ffreestanding -O2 -nostdlib -no-pie \
    boot.o interrupts.o kernel.o idt.o driver.o pmm.o vmm.o heap.o shell.o ata.o fat32.o \
    -lgcc -Wl,--build-id=none

echo "=== 3. Structure ISO ==="
mkdir -p isodir/boot/grub
cp myos.bin isodir/boot/myos.bin
cp boot/grub.cfg isodir/boot/grub/grub.cfg

echo "=== 4. Génération ISO ==="
grub-mkrescue -o mon_os.iso isodir

echo "=== 5. Création disque FAT32 (optionnel) ==="
dd if=/dev/zero of=disk.img bs=1M count=40
mkfs.vfat -F 32 disk.img
mcopy -i disk.img disk_content/* ::/ || echo "[WARN] mcopy échoué, disk.img ignoré"
/mnt/c/Program\ Files/Oracle/VirtualBox/VBoxManage.exe convertfromraw disk.img monOS.vdi --format VDI \
    || echo "[WARN] VBoxManage échoué, disk.vdi ignoré"

echo "=== Terminé ! ==="
ls -lh mon_os.iso
