# BibOS — Module Interceptions Clavier & Gestion des Interruptions (IDT)

Cette branche contient l'implémentation minimaliste et fonctionnelle de la gestion des interruptions matérielles sur l'architecture x86 (Mode Protégé 32 bits), appliquée spécifiquement à la réception des scancodes du **clavier**.

L'objectif de cette configuration est d'activer le mode interruptions (`sti`) sans provoquer de défaillance critique (*Triple Fault*) et de capturer chaque pression de touche.

## 🏗️ Architecture du Module

Les fichiers impliqués dans cette fonctionnalité sont répartis de la manière suivante :

* `boot/boot.asm` : Point d'entrée du noyau. Gère la configuration de la pile (`.bss`), le chargement de l'IDT via l'instruction assembleur `lidt` et fournit les fonctions de communication I/O (`inb`, `outb`).
* `interrupts/interrupts.asm` : Contient l'enrobage en assembleur bas-niveau (`keyboard_handler_asm`). Il sauvegarde le contexte des registres du processeur (`pushad`), bascule les segments de données et appelle la routine C.
* `idt.c` : Déclaration de la table des descripteurs d'interruptions (IDT), configuration des barrières de sécurité (*gates*) et reprogrammation des contrôleurs d'interruptions (PIC 8259A).
* `driver/keyboard.c` : Contient la routine d'interruption en C (`keyboard_handler_c`). Lit le port de données `0x60`, manipule la mémoire vidéo VGA (`0xB8000`) pour le retour visuel et acquitte le PIC (EOI).

---

## 🛠️ Concepts Clés Implémentés

### 1. Structure de l'IDT & Segments GRUB
L'IDT est configurée pour correspondre à l'environnement mémoire mis en place par GRUB lors du boot Multiboot :
* **Segment de Code (CS)** : Fixé à `0x10`.
* **Segment de Données (DS/ES/FS/GS)** : Fixé à `0x18`.
Chaque descripteur de l'IDT (Gate) est injecté avec le sélecteur `0x10` et le drapeau `0x8E` (présent, anneau 0, type interruption 32-bit).

### 2. Reprogrammation du PIC (8259A)
Par défaut, les interruptions matérielles (IRQ) entrent en conflit avec les exceptions natives d'Intel. Le PIC maître et le PIC esclave sont reprogrammés (mappés) pour rediriger :
* Le Master PIC vers le vecteur **32** (l'IRQ 1 du clavier devient la case **33**).
* Le Slave PIC vers le vecteur **40**.

⚠️ **Masquage du Timer :** Afin d'éviter les crashs liés à l'horloge système (IRQ 0) non gérée, les lignes du PIC sont masquées de manière stricte :
```c
outb(0x21, 0xFD); // Active uniquement l'IRQ 1 (Clavier : 11111101), bloque l'horloge
outb(0xA1, 0xFF); // Bloque complètement le PIC esclave
