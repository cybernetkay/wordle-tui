#include <ctype.h>
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

struct termios orig_termios;

typedef enum {
    DEFAULT = 0,
    GRAY,
    GREEN,
    YELLOW,
} Colors;

typedef enum {
    WIN,
    IDLE,
    LOSS,
} State;

typedef struct {
    char letter;
    Colors color;
} Cell;

// TODO: caricare il file in memoria ram eliminando il tempo di accesso a disco 
typedef struct {
    State state;
    bool running;
    int n_words; 
    int cursor_y;
    int cursor_x;
    char word_to_guess[COLS + 2];
    Colors h_keys[26];
    Cell cells[ROWS][COLS];
    FILE *file;
} GameData;

void render_grid(GameData *game_data);
void set_n_words(GameData *game_data);
void set_file(GameData *game_data, const char * const filename);
void set_guess_word(GameData *game_data);
void print_char_color(char c, Colors color, bool formatted);
int get_random_index(GameData *game_data);
void init_game_data(GameData *game_data, const char * const filename);
void process_input(GameData *game_data);
void cleanup_game(GameData *game_data);
void update_state(GameData *game_data);
bool validate_word(GameData game_data);
void set_hints(GameData *game_data);
void disable_raw_mode();
void enable_raw_mode();
void draw_result_menu(GameData *game_data);

int main(void){
    srand(time(NULL));
    enable_raw_mode();

    GameData game_data;
    
start:
    init_game_data(&game_data, FILENAME);

    if (game_data.file == NULL) {
        return 1; 
    }

    set_guess_word(&game_data);

    // printf("Debug - Parola scelta: %s\n", game_data.word_to_guess);
    // usleep(1000*1000);

    while(game_data.running){
        render_grid(&game_data);
        process_input(&game_data);
    }

    render_grid(&game_data);

    draw_result_menu(&game_data);
    while(1) {
        char c = getchar();
        if (c == 27) break;
        if(c == 'r') goto start;
    }

    cleanup_game(&game_data);

    disable_raw_mode();

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
    
    raw.c_lflag &= ~(ECHO | ICANON);
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    printf("\033[?25l");
}

void init_game_data(GameData *game_data, const char * const filename){
    if(game_data == NULL) return;

    memset(game_data, 0, sizeof(GameData));

    for(int i = 0; i < ROWS; i++){
        for(int j = 0; j < COLS; j++){
            game_data->cells[i][j].letter = ' ';
        }
    }

    set_file(game_data, filename);
    set_n_words(game_data);

    game_data->running = true;
    game_data->state = IDLE;

    if(game_data->file) rewind(game_data->file);
}

void set_file(GameData *game_data, const char * const filename){
    game_data->file = fopen(filename, "r");
    if(game_data->file == NULL){
        fprintf(stderr, "Errore: Impossibile aprire il file '%s'\n", filename);
    }
}

void set_n_words(GameData *game_data){
    if(game_data->file == NULL) return;
    
    game_data->n_words = 0;
    char line[256];
    
    while(fgets(line, sizeof(line), game_data->file) != NULL){
        if (strlen(line) > 1) {
            game_data->n_words++;
        }
    }
}

int get_random_index(GameData *game_data){
    if(game_data->file == NULL || game_data->n_words == 0) return 0;
    return rand() % game_data->n_words;
}

void set_guess_word(GameData *game_data){
    if(game_data->file == NULL) return;

    rewind(game_data->file);
    
    int target_index = get_random_index(game_data);
    
    char temp_buffer[100]; 

    for(int i = 0; i < target_index; i++){
        if(fgets(temp_buffer, sizeof(temp_buffer), game_data->file) == NULL){
            rewind(game_data->file);
            break;
        }
    }

    if(fgets(game_data->word_to_guess, sizeof(game_data->word_to_guess), game_data->file) != NULL){
        game_data->word_to_guess[strcspn(game_data->word_to_guess, "\r\n")] = 0;
    }
}

void print_char_color(char c, Colors color, bool formatted){
    char *color_code;
    char *reset_code = "\033[0m";

    switch(color){
        case GREEN:
            color_code = "\033[1;32m"; // Verde testo
            break;
        case YELLOW:
            color_code = "\033[1;33m"; // Giallo testo
            break;
        case GRAY:
            color_code = "\033[1;90m"; // Grigio scuro testo
            break;
        default: 
            color_code = "\033[0m";    // Default
            break;
    }

    if(formatted)
        printf("│ %s%c%s │ ", color_code, c, reset_code);
    else 
        printf("%s%c%s", color_code, c, reset_code);
}

void render_grid(GameData *game_data){
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    int term_rows = w.ws_row;
    int term_cols = w.ws_col;

    int grid_width = COLS * 6; 
    int grid_height = (ROWS * 3) + 4;

    int start_y = (term_rows - grid_height) / 2;
    if (start_y < 1) start_y = 1;

    int start_x = (term_cols - grid_width) / 2;
    if (start_x < 1) start_x = 1;


    printf("\033[2J");

    printf("\033[%d;%dH", start_y, start_x + (grid_width/2) - 7);
    printf("WORDLE C - TUI\n");
    
    start_y += 2;

    for (int r = 0; r < ROWS; r++) {
        
        printf("\033[%d;%dH", start_y++, start_x);
        for (int c = 0; c < COLS; c++) printf("┌───┐ ");

        printf("\033[%d;%dH", start_y++, start_x);
        for (int c = 0; c < COLS; c++) {
            Cell cell = game_data->cells[r][c];
            print_char_color(cell.letter, cell.color, true);
        }

        printf("\033[%d;%dH", start_y++, start_x);
        for (int c = 0; c < COLS; c++) printf("└───┘ ");
    }

    start_y += 2;
    
    printf("\033[%d;%dH", start_y, start_x-10);
    for(int i = 0; i < 26; i++){
        print_char_color('A'+i, game_data->h_keys[i], false);
        putchar(' ');
    }
}

void cleanup_game(GameData *game_data) {
    if (game_data->file != NULL) {
        fclose(game_data->file);
        game_data->file = NULL;
    }
}

void process_input(GameData *game_data){
    if(game_data == NULL) return;

    char c = getchar();

    if(c == 10 && game_data->cursor_y < ROWS && validate_word(*game_data)){
        set_hints(game_data);
        update_state(game_data);
        game_data->cursor_y++;
        game_data->cursor_x = 0;
    }

    else if(c == 27) game_data->running = false;

    else if (c == 127 || c == 8){
        if(game_data->cursor_x > 0) game_data->cursor_x--;
        game_data->cells[game_data->cursor_y][game_data->cursor_x].letter = ' ';
    }

    if(!isalpha(c)) return;

    if(game_data->cursor_x < COLS){
        game_data->cells[game_data->cursor_y][game_data->cursor_x++].letter = c;
    }
}

bool validate_word(GameData game_data){
    rewind(game_data.file);

    char current_guess[COLS+1];

    for(int i = 0; i < COLS; i++){
        current_guess[i] = game_data.cells[game_data.cursor_y][i].letter;
    }
    current_guess[COLS] = '\0';

    char line[COLS+1];
    while(fgets(line, sizeof(line), game_data.file) != NULL){
        line[COLS] = '\0';
        if(strcmp(line, current_guess) == 0) return true;
    }

    rewind(game_data.file);
    return false;
}

void set_hints(GameData *game_data){
    int r = game_data->cursor_y;
    
    bool link[COLS] = {false};
    
    char *secret = game_data->word_to_guess;

    for(int i = 0; i < COLS; i++){
        char user_char = game_data->cells[r][i].letter;
        
        int key_idx = tolower(user_char) - 'a';

        game_data->cells[r][i].color = GRAY;

        if(key_idx >= 0 && key_idx < 26) {
            if (game_data->h_keys[key_idx] == DEFAULT) {
                game_data->h_keys[key_idx] = GRAY;
            }
        }

        if(user_char == secret[i]){
            game_data->cells[r][i].color = GREEN;
            link[i] = true;

            if(key_idx >= 0 && key_idx < 26) {
                game_data->h_keys[key_idx] = GREEN;
            }
        }
    }

    for(int i = 0; i < COLS; i++){
        if(game_data->cells[r][i].color == GREEN) continue;

        char user_char = game_data->cells[r][i].letter;
        int key_idx = tolower(user_char) - 'a';

        for(int j = 0; j < COLS; j++){
            if(secret[j] == user_char && !link[j]){
                
                game_data->cells[r][i].color = YELLOW;
                link[j] = true;
                
                if(key_idx >= 0 && key_idx < 26) {
                    if (game_data->h_keys[key_idx] != GREEN) {
                        game_data->h_keys[key_idx] = YELLOW;
                    }
                }

                break;
            }
        }
    }
}

void update_state(GameData *game_data){
    if(game_data->cursor_y >= ROWS) {
        game_data->state = LOSS;
        game_data->running = false;
        return;
    }

    game_data->state = WIN;
    for(int i = 0; i < COLS; i++){
        if(game_data->cells[game_data->cursor_y][i].color != GREEN){
            game_data->state = IDLE;
            return;
        }
    }


    game_data->running = false;
}

void draw_result_menu(GameData *game_data) {
    
    char *color;
    char *title;
    char msg[100];

    if (game_data->state == WIN) {
        color = "\033[42;1;37m"; // Sfondo Verde, Testo Bianco Bold
        title = " VITTORIA! ";
        sprintf(msg, "Hai indovinato in %d tentativi!", game_data->cursor_y);
    } else {
        color = "\033[41;1;37m"; // Sfondo Rosso, Testo Bianco Bold
        title = " HAI PERSO ";
        sprintf(msg, "La parola era: %s", game_data->word_to_guess);
    }

    printf("\n");
    printf("      %s%s\033[0m\n", color, "                       "); // Barra superiore
    printf("      %s%s%s%s%s\n", color, "      ", title, "      ", "\033[0m"); // Titolo centrato
    printf("      %s%s\033[0m\n", color, "                       "); // Spazio
    
    printf("      \033[100;97m %-21s \033[0m\n", msg); 
    
    printf("      \033[100;97m %-21s \033[0m\n", " Premere ESC per uscire o R per iniziare"); 
    printf("\n");

}
