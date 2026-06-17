#!/bin/bash
set -e  # arrête tout si une commande échoue
ROOT=$(dirname "$0")   # répertoire du script
cd "$ROOT"

echo "=== Nettoyage ==="
rm -rf *.o myos.bin mon_os.iso isodir

echo "=== 1. Compilation ==="
nasm -f elf32 boot/boot.asm -o boot.o
nasm -f elf32 interrupts/interrupts.asm -o interrupts.o

gcc -m32 -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c interrupts/idt.c -o idt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c driver/keyboard.c -o driver.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie

gcc -m32 -T boot/linker.ld -o myos.bin \
    -ffreestanding -O2 -nostdlib -no-pie \
    boot.o interrupts.o kernel.o idt.o driver.o \
    -lgcc -Wl,--build-id=none

echo "=== 2. Structure ISO ==="
mkdir -p isodir/boot/grub
cp myos.bin isodir/boot/myos.bin
cp boot/grub.cfg isodir/boot/grub/grub.cfg   # ← chemin corrigé

echo "=== 3. Génération ISO ==="
grub-mkrescue -o mon_os.iso isodir

echo "=== Terminé ! ==="