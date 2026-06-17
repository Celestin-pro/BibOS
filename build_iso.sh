#!/bin/bash
echo "=== Nettoyage ==="
rm -rf *.o myos.bin mon_os.iso isodir

echo "=== 1. Compilation ==="
nasm -f elf32 boot.asm -o boot.o
nasm -f elf32 interrupts.asm -o interrupts.o

# On ajoute -fno-pie à chaque fichier C
gcc -m32 -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c ./interrupts/idt.c -o ./interrupts/idt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c ./driver/keyboard.c -o ./driver/keyboard.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie

# On ajoute -no-pie à l'étape finale de liaison (Linker)
# On ajoute -Wl,--build-id=none à la toute fin pour supprimer la signature de GCC
gcc -m32 -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib -no-pie boot.o interrupts.o kernel.o idt.o keyboard.o -lgcc -Wl,--build-id=none
echo "=== 2. Structure ISO ==="
mkdir -p isodir/boot/grub
cp myos.bin isodir/boot/myos.bin
cp grub.cfg isodir/boot/grub/grub.cfg

echo "=== 3. Génération ISO ==="
grub-mkrescue -o mon_os.iso isodir

echo "=== Terminé ! ==="