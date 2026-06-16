#!/bin/bash
set -e  # Arrête si une commande échoue

echo "=== Nettoyage ==="
rm -rf isodir myos.bin mon_os.iso
nasm -f elf32 boot.asm -o boot.o
nasm -f elf32 interrupts.asm -o interrupts.o
echo "=== 1. Compilation ==="
gcc -m32 -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
gcc -m32 -c idt.c -o idt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
gcc -m32 -c driver.c -o driver.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

# On ajoute driver.o à la liaison finale
gcc -m32 -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o interrupts.o kernel.o idt.o driver.o -lgcc

echo "=== 2. Structure ISO ==="
mkdir -p isodir/boot/grub
cp myos.bin isodir/boot/myos.bin
cp grub.cfg isodir/boot/grub/grub.cfg

echo "=== 3. Génération ISO ==="
grub-mkrescue -o mon_os.iso isodir

echo "=== Terminé ! ==="