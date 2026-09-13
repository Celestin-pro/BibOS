#!/bin/bash
set -e  # arrête tout si une commande échoue
ROOT=$(dirname "$0")   # répertoire du script
cd "$ROOT"

echo "=== Nettoyage ==="
VBOXMANAGE="/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"
OLD_VDI_UUID=$("$VBOXMANAGE" showmediuminfo disk disk.vdi 2>/dev/null | grep "^UUID:" | awk '{print $2}' | tr -d '\r' || true)
rm -rf *.o myos.bin mon_os.iso isodir disk.img disk.vdi

echo "=== 1. Compilation ==="
nasm -f elf32 boot/boot.asm -o boot.o
nasm -f elf32 interrupts/interrupts.asm -o interrupts.o
nasm -f elf32 gdt/gdt.asm -o gdt.o

gcc -m32 -c kernel.c           -o kernel.o  -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c gdt/gdt.c          -o gdt_c.o   -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c interrupts/idt.c   -o idt.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c driver/keyboard.c  -o driver.o  -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c driver/serial.c    -o serial.o  -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c memory/pmm.c       -o pmm.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c memory/vmm.c       -o vmm.o     -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c memory/heap.c      -o heap.o    -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c shell.c            -o shell.o   -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c driver/ata.c      -o ata.o      -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c fs/fat32.c        -o fat32.o    -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c process/process.c -o process.o  -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
gcc -m32 -c process/ring3.c   -o ring3.o    -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
nasm -f elf32 process/switch.asm -o switch.o
nasm -f elf32 process/ring3.asm  -o ring3_asm.o
gcc -m32 -c syscall/syscall.c  -o syscall.o   -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie
nasm -f elf32 syscall/syscall.asm -o syscall_asm.o

echo "=== 2. Linkage ==="
gcc -m32 -T boot/linker.ld -o myos.bin \
    -ffreestanding -O2 -nostdlib -no-pie \
    boot.o interrupts.o gdt.o gdt_c.o kernel.o idt.o driver.o serial.o pmm.o vmm.o heap.o shell.o ata.o fat32.o process.o ring3.o switch.o ring3_asm.o syscall.o syscall_asm.o \
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
"$VBOXMANAGE" convertfromraw disk.img disk.vdi --format VDI \
    || echo "[WARN] VBoxManage échoué, disk.vdi ignoré"
if [ -n "$OLD_VDI_UUID" ] && [ -f disk.vdi ]; then
    "$VBOXMANAGE" internalcommands sethduuid disk.vdi "$OLD_VDI_UUID"
fi

echo "=== Terminé ! ==="
ls -lh mon_os.iso
