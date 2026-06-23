extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

extern int cursor_pos;
extern char* video_memory;

#define SC_ESC     0x01
#define SC_BSP     0x0E
#define SC_ENTER   0x1C
#define SC_CTRL    0x1D
#define SC_LSHIFT  0x2A
#define SC_RSHIFT  0x36
#define SC_ALT     0x38
#define SC_SPACE   0x39
#define SC_CAPS    0x3A
#define SC_RELEASE 0x80

#define COLS 80
#define ROWS 25

#define KB_BUF_SIZE 64

typedef enum {
    STATE_NORMAL,
    STATE_SHIFT,
    STATE_CAPS,
    STATE_SHIFT_CAPS,
    STATE_CTRL,
    STATE_ALT,
    STATE_SPACE,
} KeyboardState;

static KeyboardState state = STATE_NORMAL;

static volatile char kb_buf[KB_BUF_SIZE];
static volatile int  kb_head = 0;  /* prochain index d'écriture */
static volatile int  kb_tail = 0;  /* prochain index de lecture  */

static const char kbd_normal[58] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',  /* 0x00-0x09 */
    '9', '0', ')', '^', '\b','\t','a', 'z', 'e', 'r',  /* 0x0A-0x13 */
    't', 'y', 'u', 'i', 'o', 'p', '^', '$', '\n', 0,   /* 0x14-0x1D */
    'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm',  /* 0x1E-0x27 */
    '%', '*',  0,  '*', 'w', 'x', 'c', 'v', 'b', 'n',  /* 0x28-0x31 */
    ',', ';', ':', '!',  0,   0,   0,  ' ',             /* 0x32-0x39 */
};

static const char kbd_shift[58] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',  /* 0x00-0x09 */
    '(', ')',  0,  '+', '\b','\t','A', 'Z', 'E', 'R',  /* 0x0A-0x13 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   /* 0x14-0x1D */
    'Q', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M',  /* 0x1E-0x27 */
    '/', '~',  0,  '|', 'W', 'X', 'C', 'V', 'B', 'N',  /* 0x28-0x31 */
    '?', '.', '/', '_',  0,   0,   0,  ' ',             /* 0x32-0x39 */
};

/*
 * Transitions de la machine d'état :
 *
 *  NORMAL ──shift press──► SHIFT ──shift release──► NORMAL
 *  NORMAL ──caps press───► CAPS  ──caps press─────► NORMAL
 *  NORMAL ──ctrl press───► CTRL  ──ctrl release───► NORMAL
 *  NORMAL ──alt press────► ALT   ──alt release────► NORMAL
 *  SHIFT  ──caps press───► SHIFT_CAPS ──shift release──► CAPS
 *  CAPS   ──shift press──► SHIFT_CAPS ──caps press─────► SHIFT
 */
static KeyboardState transition(KeyboardState s, unsigned char sc) {
    int released = sc & SC_RELEASE;
    unsigned char key = sc & ~SC_RELEASE;

    switch (s) {
        case STATE_NORMAL:
            if (!released) {
                if (key == SC_LSHIFT || key == SC_RSHIFT) return STATE_SHIFT;
                if (key == SC_CTRL)                       return STATE_CTRL;
                if (key == SC_ALT)                        return STATE_ALT;
                if (key == SC_CAPS)                       return STATE_CAPS;
            }
            break;

        case STATE_SHIFT:
            if (released && (key == SC_LSHIFT || key == SC_RSHIFT)) return STATE_NORMAL;
            if (!released && key == SC_CAPS)                         return STATE_SHIFT_CAPS;
            break;

        case STATE_CAPS:
            if (!released && key == SC_CAPS)                         return STATE_NORMAL;
            if (!released && (key == SC_LSHIFT || key == SC_RSHIFT)) return STATE_SHIFT_CAPS;
            break;

        case STATE_SHIFT_CAPS:
            if (released && (key == SC_LSHIFT || key == SC_RSHIFT)) return STATE_CAPS;
            if (!released && key == SC_CAPS)                        return STATE_SHIFT;
            break;

        case STATE_CTRL:
            if (released && key == SC_CTRL) return STATE_NORMAL;
            break;

        case STATE_ALT:
            if (released && key == SC_ALT) return STATE_NORMAL;
            break;

        case STATE_SPACE:
            if (released && key == SC_SPACE) return STATE_SPACE;
            break;
    }
    return s;
}

static char resolve(unsigned char key, KeyboardState s) {
    if (key >= 58) return 0;
    switch (s) {
        case STATE_NORMAL: return kbd_normal[key];
        case STATE_SHIFT:  return kbd_shift[key];
        case STATE_CAPS: {
            char c = kbd_normal[key];
            return (c >= 'a' && c <= 'z') ? c - 32 : c;
        }
        case STATE_SHIFT_CAPS: {
            char c = kbd_shift[key];
            return (c >= 'A' && c <= 'Z') ? c + 32 : c;
        }
        default: return 0;
    }
}

/* ---------- buffer circulaire ---------- */

static void kb_push(char c) {
    int next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail)            /* ignorer si plein */
        kb_buf[kb_head] = c, kb_head = next;
}

/* Bloque jusqu'à ce qu'un caractère soit disponible, puis le retourne. */
char kgetchar(void) {
    char c;
    while (kb_head == kb_tail)
        __asm__ __volatile__("hlt"); /* CPU en veille jusqu'au prochain IRQ */
    c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}

/* ---------- affichage ---------- */

static void scroll(void) {
    int i;
    for (i = 0; i < (ROWS - 1) * COLS * 2; i++)
        video_memory[i] = video_memory[i + COLS * 2];
    for (i = (ROWS - 1) * COLS * 2; i < ROWS * COLS * 2; i += 2) {
        video_memory[i]     = ' ';
        video_memory[i + 1] = 0x07;
    }
    cursor_pos -= COLS * 2;
}

static void put_char(char c) {
    if (c == '\b') {
        if (cursor_pos >= 2) {
            cursor_pos -= 2;
            video_memory[cursor_pos]     = ' ';
            video_memory[cursor_pos + 1] = 0x07;
        }
        return;
    }
    if (c == '\n') {
        int col = (cursor_pos / 2) % COLS;
        cursor_pos += (COLS - col) * 2;
    } else if (c == '\t') {
        /* aligne sur le prochain multiple de 8 */
        int col = (cursor_pos / 2) % COLS;
        int spaces = 8 - (col % 8);
        int i;
        for (i = 0; i < spaces; i++) {
            video_memory[cursor_pos]     = ' ';
            video_memory[cursor_pos + 1] = 0x07;
            cursor_pos += 2;
        }
    } else {
        video_memory[cursor_pos]     = c;
        video_memory[cursor_pos + 1] = 0x07;
        cursor_pos += 2;
    }
    if (cursor_pos >= ROWS * COLS * 2)
        scroll();
}

/*
 * Décide quoi faire selon la nature du caractère :
 *   - caractères imprimables : afficher à l'écran
 *   - '\n', '\b', '\t'      : traitement spécial via put_char
 *   - ESC (27)              : ignoré (extensible)
 *   - autres contrôles      : ignorés
 */
void kputchar(char c) {
    if (c == '\n' || c == '\b' || c == '\t') {
        put_char(c);
    } else if (c == 27) {
        /* ESC : rien pour l'instant */
    } else if ((unsigned char)c >= 32 && (unsigned char)c < 127) {
        put_char(c);
    }
    /* autres caractères de contrôle : ignorés */
}

/* ---------- handler IRQ1 ---------- */

void keyboard_handler_c(void) {
    unsigned char sc = inb(0x60);

    state = transition(state, sc);

    if (!(sc & SC_RELEASE)) {
        char c = resolve(sc & ~SC_RELEASE, state);
        if (c) kb_push(c);          /* stocker dans le buffer, pas afficher */
    }

    outb(0x20, 0x20);
}
