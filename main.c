#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define ROWS 6
#define COLS 5
#define FILENAME "parole_5" 
#define MAX_WORDS 15000         

struct termios orig_termios;

typedef enum {
    KEY_NULL = 0,
    CTRL_C = 3,
    BACKSPACE = 127,
    ENTER = 10,
    ESC = 27,
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT
} Key;

typedef enum {
    DEFAULT = 0,
    GRAY,
    GREEN,
    YELLOW,
    RED, 
} Colors;

typedef enum {
    WIN,
    IDLE,
    LOSS,
    SETTINGS,
} State;

typedef struct {
    bool hardMode;
} Settings;

typedef struct {
    char letter;
    Colors color;
} Cell;

typedef struct {
    State state;
    Settings settings;
    bool running;
    bool restart_requested;
    
    char dictionary[MAX_WORDS][COLS + 1];
    int dict_count;

    int cursor_y;
    int cursor_x;
    char word_to_guess[COLS + 2];
    Colors h_keys[26];
    Cell cells[ROWS][COLS];
} GameData;

void init_game(GameData *gd);
void load_dictionary(GameData *gd, const char *filename);
void pick_secret_word(GameData *gd);
void render_master(GameData *gd); 
void render_grid(GameData *gd);
void render_settings(GameData *gd);
void render_result(GameData *gd);
void process_input(GameData *gd);
int  read_key(); 
void set_hints(GameData *gd);
void update_state(GameData *gd);
bool validate_word_dictionary(GameData *gd);
bool validate_word_hard_mode(GameData *gd);
void show_toast(char *msg, Colors color);
void enable_raw_mode();
void disable_raw_mode();
char* get_ansi_color(Colors color, bool is_background);

int main(void) {
    srand(time(NULL));
    enable_raw_mode();

    GameData gd;
    gd.dict_count = 0; 
    load_dictionary(&gd, FILENAME);

    if (gd.dict_count == 0) {
        disable_raw_mode();
        fprintf(stderr, "Errore: File '%s' non trovato o vuoto.\n", FILENAME);
        return 1;
    }

    do {
        init_game(&gd); 
        
        while (gd.running) {
            render_master(&gd);
            process_input(&gd);
            usleep(10000); 
        }

        render_grid(&gd); 
        render_result(&gd);

        while (1) {
            int c = read_key();
            if (c == ESC) { gd.restart_requested = false; break; }
            if (c == 'r' || c == 'R') { gd.restart_requested = true; break; }
        }

    } while (gd.restart_requested);

    disable_raw_mode();
    printf("\033[2J\033[H"); 
    return 0;
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?25h");
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG); 
    raw.c_cc[VMIN] = 0;  
    raw.c_cc[VTIME] = 1; 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?25l");
}

int read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) return KEY_NULL;
        if (nread == 0) return KEY_NULL; 
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESC;
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
        return ESC;
    }
    if (c == 127) return BACKSPACE;
    if (c == 13) return ENTER;
    return c;
}

void init_game(GameData *gd) {
    gd->state = IDLE;
    gd->running = true;
    gd->cursor_x = 0;
    gd->cursor_y = 0;
    gd->restart_requested = false;
    for(int i=0; i<ROWS; i++) {
        for(int j=0; j<COLS; j++) {
            gd->cells[i][j].letter = ' ';
            gd->cells[i][j].color = DEFAULT;
        }
    }
    for(int i=0; i<26; i++) gd->h_keys[i] = DEFAULT;
    pick_secret_word(gd);
}

void load_dictionary(GameData *gd, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return;
    char line[64];
    while(fgets(line, sizeof(line), f) && gd->dict_count < MAX_WORDS) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == COLS) {
            for(int i=0; i<COLS; i++) line[i] = toupper(line[i]);
            strcpy(gd->dictionary[gd->dict_count], line);
            gd->dict_count++;
        }
    }
    fclose(f);
}

void pick_secret_word(GameData *gd) {
    if (gd->dict_count == 0) return;
    int idx = rand() % gd->dict_count;
    strcpy(gd->word_to_guess, gd->dictionary[idx]);
}

void process_input(GameData *gd) {
    int key = read_key();
    if (key == KEY_NULL) return;

    if (gd->state == SETTINGS) {
        if (key == ESC || key == '@') gd->state = IDLE;
        else if (key == ENTER) gd->settings.hardMode = !gd->settings.hardMode;
        return;
    }

    if (gd->state == IDLE) {
        if (key == ESC || key == CTRL_C) { gd->running = false; return; }
        if (key == '@') { gd->state = SETTINGS; return; }
        if (key == BACKSPACE || key == 8) {
            if (gd->cursor_x > 0) {
                gd->cursor_x--;
                gd->cells[gd->cursor_y][gd->cursor_x].letter = ' ';
            }
            return;
        }
        if (key == ENTER) {
            if (gd->cursor_x < COLS) { show_toast("Parola troppo corta!", RED); return; }
            if (gd->settings.hardMode && !validate_word_hard_mode(gd)) return;
            if (!validate_word_dictionary(gd)) return;

            set_hints(gd);
            update_state(gd);
            if (gd->running) { gd->cursor_y++; gd->cursor_x = 0; }
            return;
        }
        if (key >= 'a' && key <= 'z') key = toupper(key);
        if (key >= 'A' && key <= 'Z') {
            if (gd->cursor_x < COLS) {
                gd->cells[gd->cursor_y][gd->cursor_x].letter = (char)key;
                gd->cursor_x++;
            }
        }
    }
}

bool validate_word_dictionary(GameData *gd) {
    char guess[COLS + 1];
    for(int i=0; i<COLS; i++) guess[i] = gd->cells[gd->cursor_y][i].letter;
    guess[COLS] = '\0';
    for(int i=0; i < gd->dict_count; i++) {
        if (strcmp(gd->dictionary[i], guess) == 0) return true;
    }
    show_toast("Parola non nel dizionario!", RED);
    return false;
}

bool validate_word_hard_mode(GameData *gd) {
    if (gd->cursor_y == 0) return true; 
    int r = gd->cursor_y;
    int prev = r - 1;
    char error_msg[100];

    for(int i=0; i<COLS; i++) {
        if (gd->cells[prev][i].color == GREEN) {
            if (gd->cells[r][i].letter != gd->cells[prev][i].letter) {
                snprintf(error_msg, 100, "Manca '%c' in pos %d!", gd->cells[prev][i].letter, i+1);
                show_toast(error_msg, RED);
                return false;
            }
        }
    }
    for(int i=0; i<COLS; i++) {
        if (gd->cells[prev][i].color == YELLOW) {
            char target = gd->cells[prev][i].letter;
            bool found = false;
            for(int j=0; j<COLS; j++) {
                if (gd->cells[r][j].letter == target) { found = true; break; }
            }
            if (!found) {
                snprintf(error_msg, 100, "Devi usare la lettera '%c'!", target);
                show_toast(error_msg, RED);
                return false;
            }
        }
    }
    return true;
}

void set_hints(GameData *gd) {
    int r = gd->cursor_y;
    char *secret = gd->word_to_guess;
    bool link[COLS] = {false};

    for(int i=0; i<COLS; i++) {
        gd->cells[r][i].color = GRAY; 
        char c = gd->cells[r][i].letter;
        int k_idx = c - 'A';
        if (k_idx >= 0 && k_idx < 26 && gd->h_keys[k_idx] == DEFAULT) gd->h_keys[k_idx] = GRAY;

        if (c == secret[i]) {
            gd->cells[r][i].color = GREEN;
            if (k_idx >= 0 && k_idx < 26) gd->h_keys[k_idx] = GREEN;
            link[i] = true; 
        }
    }

    for(int i=0; i<COLS; i++) {
        if (gd->cells[r][i].color == GREEN) continue;
        char c = gd->cells[r][i].letter;
        int k_idx = c - 'A';
        for(int j=0; j<COLS; j++) {
            if (secret[j] == c && !link[j]) {
                gd->cells[r][i].color = YELLOW;
                if (k_idx >= 0 && k_idx < 26 && gd->h_keys[k_idx] != GREEN) gd->h_keys[k_idx] = YELLOW;
                link[j] = true;
                break; 
            }
        }
    }
}

void update_state(GameData *gd) {
    bool all_green = true;
    for(int i=0; i<COLS; i++) {
        if (gd->cells[gd->cursor_y][i].color != GREEN) all_green = false;
    }
    if (all_green) { gd->state = WIN; gd->running = false; return; }
    if (gd->cursor_y >= ROWS - 1) { gd->state = LOSS; gd->running = false; return; }
}

char* get_ansi_color(Colors color, bool is_background) {
    if (is_background) {
        // Colori per le Celle (Sfondo solido)
        switch(color) {
            case GREEN:  return "\033[38;5;255;48;5;28;1m";  // Forest Green
            case YELLOW: return "\033[38;5;255;48;5;178;1m"; // Gold
            case RED:    return "\033[38;5;255;48;5;160;1m"; // Red Error
            case GRAY:   return "\033[38;5;255;48;5;240;1m"; // Dark Gray
            default:     return "\033[0m";
        }
    } else {
        // Colori per la Tastiera (Solo testo, Foreground)
        switch(color) {
            case GREEN:  return "\033[38;5;46;1m";   // Bright Green Text
            case YELLOW: return "\033[38;5;226;1m";  // Bright Yellow Text
            case RED:    return "\033[38;5;196;1m";  // Bright Red Text
            case GRAY:   return "\033[38;5;240;1m";  // Dark Gray Text
            default:     return "\033[38;5;250m";    // Default Light Gray
        }
    }
}

void render_master(GameData *gd) {
    if (gd->state == SETTINGS) render_settings(gd);
    else render_grid(gd);
}

void render_grid(GameData *gd) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    int grid_w = COLS * 8; 
    int grid_h = (ROWS * 3) + 6; 
    int sy = (w.ws_row - grid_h) / 2;
    int sx = (w.ws_col - grid_w) / 2;
    if (sy < 1) sy = 1; if (sx < 1) sx = 1;

    printf("\033[2J"); 
    printf("\033[%d;%dH\033[1mWORDLE C\033[0m (Premi @ per Impostazioni)", sy, (w.ws_col/2) - 15);
    sy += 2;

    for(int r=0; r<ROWS; r++) {
        printf("\033[%d;%dH", sy++, sx);
        for(int c=0; c<COLS; c++){
            Cell cell = gd->cells[r][c];
            if (cell.color != DEFAULT) printf("%s┌─────┐\033[0m ", get_ansi_color(cell.color, true));
            else printf("\033[90m┌─────┐\033[0m ");
        }
        
        printf("\033[%d;%dH", sy++, sx);
        for(int c=0; c<COLS; c++) {
            Cell cell = gd->cells[r][c];
            if (cell.color != DEFAULT) {
                printf("%s│  %c  │\033[0m ", get_ansi_color(cell.color, true), cell.letter);
            } else {
                if (cell.letter != ' ') printf("\033[90m│\033[0m\033[1m  %c  \033[0m\033[90m│\033[0m ", cell.letter);
                else printf("\033[90m│     │\033[0m ");
            }
        }

        printf("\033[%d;%dH", sy++, sx);
        for(int c=0; c<COLS; c++) {
            Cell cell = gd->cells[r][c];
            if (cell.color != DEFAULT) printf("%s└─────┘\033[0m ", get_ansi_color(cell.color, true));
            else printf("\033[90m└─────┘\033[0m ");
        }
    }

    sy += 2; 
    int kbd_sx = (w.ws_col - 52) / 2;
    if (kbd_sx < 1) kbd_sx = 1;
    
    printf("\033[%d;%dH", sy, kbd_sx);

    for (int i = 0; i < 26; i++) {
        Colors k_col = gd->h_keys[i];
        char *ansi = get_ansi_color(k_col, false);
        
        printf("%s%c\033[0m ", ansi, 'A' + i);
    }
    
    fflush(stdout);
}

void render_settings(GameData *gd) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int sx = (w.ws_col - 40) / 2;
    int sy = (w.ws_row - 8) / 2;
    char *bg = "\033[100m"; char *fg = "\033[97m"; char *rst = "\033[0m";

    printf("\033[2J");
    printf("\033[%d;%dH%s%s┌──────────────────────────────────────┐%s", sy, sx, bg, fg, rst);
    printf("\033[%d;%dH%s%s│             IMPOSTAZIONI             │%s", sy+1, sx, bg, fg, rst);
    printf("\033[%d;%dH%s%s│──────────────────────────────────────│%s", sy+2, sx, bg, fg, rst);
    printf("\033[%d;%dH%s%s│                                      │%s", sy+3, sx, bg, fg, rst);
    char status[64];
    if (gd->settings.hardMode) sprintf(status, "\033[1;32m[ ON  ]");
    else sprintf(status, "\033[1;31m[ OFF ]");
    printf("\033[%d;%dH%s%s│      HARD MODE: %s%s%s       │%s", sy+4, sx, bg, fg, status, bg, fg, rst);
    printf("\033[%d;%dH%s%s│                                      │%s", sy+5, sx, bg, fg, rst);
    printf("\033[%d;%dH%s%s│   [INVIO] Cambia      [@/ESC] Esci   │%s", sy+6, sx, bg, fg, rst);
    printf("\033[%d;%dH%s%s└──────────────────────────────────────┘%s", sy+7, sx, bg, fg, rst);
    fflush(stdout);
}

void render_result(GameData *gd) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int sy = (w.ws_row / 2) - 2;
    int sx = (w.ws_col / 2) - 15;
    char *col = (gd->state == WIN) ? "\033[42;1;97m" : "\033[41;1;97m";
    char msg[64];
    
    if (gd->state == WIN) sprintf(msg, "VITTORIA! (%d tentativi)", gd->cursor_y + 1);
    else sprintf(msg, "PERSO! Parola: %s", gd->word_to_guess);

    printf("\033[%d;%dH%s                              \033[0m", sy, sx, col);
    printf("\033[%d;%dH%s  %-26s  \033[0m", sy+1, sx, col, msg);
    printf("\033[%d;%dH%s                              \033[0m", sy+2, sx, col);
    printf("\033[%d;%dH\033[100;97m  PREMI [R] RIAVVIA - [ESC] ESCI  \033[0m", sy+4, sx);
    fflush(stdout);
}

void show_toast(char *message, Colors color) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int sx = (w.ws_col / 2) - (strlen(message)/2) - 2;
    int sy = (w.ws_row / 2) - 2;

    char *ansi = get_ansi_color(color, true);
    printf("\033[%d;%dH%s  %s  \033[0m", sy, sx, ansi, message);
    fflush(stdout);
    usleep(1500000); 
    tcflush(STDIN_FILENO, TCIFLUSH);
}
