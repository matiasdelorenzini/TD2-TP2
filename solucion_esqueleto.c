#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ========= CONSTANTES DEL JUEGO =========
#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 500

#define GRID_OFFSET_X 220
#define GRID_OFFSET_Y 59
#define GRID_WIDTH 650
#define GRID_HEIGHT 425

#define GRID_ROWS 5
#define GRID_COLS 9
#define CELL_WIDTH (GRID_WIDTH / GRID_COLS)
#define CELL_HEIGHT (GRID_HEIGHT / GRID_ROWS)

#define PEASHOOTER_FRAME_WIDTH 177
#define PEASHOOTER_FRAME_HEIGHT 166
#define PEASHOOTER_TOTAL_FRAMES 31
#define PEASHOOTER_ANIMATION_SPEED 4
#define PEASHOOTER_SHOOT_FRAME 18

#define ZOMBIE_FRAME_WIDTH 164
#define ZOMBIE_FRAME_HEIGHT 203
#define ZOMBIE_TOTAL_FRAMES 90
#define ZOMBIE_ANIMATION_SPEED 2
#define ZOMBIE_DISTANCE_PER_CYCLE 40.0f

#define MAX_ARVEJAS 100
#define PEA_SPEED 5
#define ZOMBIE_SPAWN_RATE 300


// ========= ESTRUCTURAS DE DATOS =========
typedef struct {
    int row, col;
} Cursor;

typedef struct {
    SDL_Rect rect;
    int activo;
    int cooldown;
    int current_frame;
    int frame_timer;
    int debe_disparar;
} Planta;

typedef struct {
    SDL_Rect rect;
    int activo;
} Arveja;

typedef struct {
    SDL_Rect rect;
    int activo;
    int vida;
    int row;
    int current_frame;
    int frame_timer;
    float pos_x;
} Zombie;

// ========= NUEVAS ESTRUCTURAS =========
#define STATUS_VACIO 0
#define STATUS_PLANTA 1

typedef struct RowSegment {
    int status;
    int start_col;
    int length;
    Planta* planta_data;
    struct RowSegment* next;
} RowSegment;

typedef struct ZombieNode {
    Zombie zombie_data;
    struct ZombieNode* next;
} ZombieNode;

typedef struct GardenRow {
    RowSegment* first_segment;
    ZombieNode* first_zombie;
} GardenRow;

typedef struct GameBoard {
    GardenRow rows[GRID_ROWS];
    Arveja arvejas[MAX_ARVEJAS]; //array adicional para manejar las arvejas
    int zombie_spawn_timer; // variable para saber cada cuanto crear un zombie
} GameBoard;


// ========= VARIABLES GLOBALES =========
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* tex_background = NULL;
SDL_Texture* tex_peashooter_sheet = NULL;
SDL_Texture* tex_zombie_sheet = NULL;
SDL_Texture* tex_pea = NULL;

Cursor cursor = {0, 0};
GameBoard* game_board = NULL;

// ========= FUNCIONES =========

char* strDuplicate(char* src) {
    int n = 0;  // Cantidad de caracteres totales de src

    for (int i = 0; *(src+i) != '\0'; i++) {
        n++;
    }
    
    char *res = (char*)malloc(sizeof(char) * (n + 1));

    for (int i = 0; *(src+i) != '\0'; i++) {
        *(res+i) = *(src+i);
    }
    *(res+n) = '\0';

    return res;
}

int strCompare(char* s1, char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if(s1[i] < s2[i]) {
            return 1;
        }
        if(s1[i] > s2[i]) {
            return -1;
        }
        i++;
    }
    if (s1[i] == '\0' && s2[i] != '\0') {
        return 1;
    }
    if (s1[i] != '\0' && s2[i] == '\0') {
        return -1;
    }
    return 0;
}

char* strConcatenate(char* src1, char* src2) {
    int n = 0;  // Cantidad de caracteres totales de strConcatenate(src1, src2)
    for (int i = 0; *(src1+i) != '\0'; i++) {
        n++;
    }
    for (int i = 0; *(src2+i) != '\0'; i++) {
        n++;
    }
    char *res = (char*)malloc(sizeof(char) * (n + 1));

    int index = 0;
    for (int i = 0; src1[i] != '\0'; i++)
    {
        *(res+index) = src1[i];
        index++;
    }
    for (int i = 0; src2[i] != '\0'; i++)
    {
        *(res+index) = src2[i];
        index++;
    }

    *(res+n) = '\0'; 

    free(src1);
    free(src2);

    return res;
}



GameBoard* gameBoardNew() {
    GameBoard* board = (GameBoard*)malloc(sizeof(GameBoard));
    if (!board) return NULL;

    board->zombie_spawn_timer = ZOMBIE_SPAWN_RATE;

    for (int i = 0; i < GRID_ROWS; i++) {
        RowSegment* first = (RowSegment*)malloc(sizeof(RowSegment));
        if (!first) {
            free(board);
            return NULL;
        }
        first->status = STATUS_VACIO;
        first->start_col = 0;
        first->length = GRID_COLS;
        first->planta_data = NULL;
        first->next = NULL;

        board->rows[i].first_segment = first;
        board->rows[i].first_zombie = NULL;
    }
     for(int i = 0; i < MAX_ARVEJAS; i++) {
        board->arvejas[i].activo = 0;
    }
    return board;
}

void gameBoardDelete(GameBoard* board) {

    // RECORRER TODAS LAS ROWS
    for (int i = 0; i < GRID_ROWS; i++)
    {
        // LIBERAR SEGMENTOS
        struct GardenRow *garden_row = &(board->rows[i]);

        if (garden_row->first_segment != NULL) {
            struct RowSegment *segment = garden_row->first_segment;

            while (segment->next !=  NULL)
            {
                struct RowSegment *tofree = segment;
                segment = segment->next;

                // LIBERAR PLANTAS
                if(tofree->status == 1) free(tofree->planta_data);
                free(tofree);
            }
            if(segment->planta_data) free(segment->planta_data);
            free(segment);
        }

        // LIMPIAR ZOMBIS
        if (garden_row->first_zombie != NULL) {
            struct ZombieNode *zombie = garden_row->first_zombie;
            while (zombie->next !=  NULL) {
                struct ZombieNode *tofree = zombie;
                zombie = zombie->next;

                free(tofree);
            }
            free(zombie);
        }
    }
    // LIBERAR BOARD
    free(board);
}

void gameBoardAddPlant(GameBoard* board, int row, int col) {
    
    // TODO: Encontrar la GardenRow correcta.
    struct GardenRow *r = &(board->rows[row]);
    
    struct RowSegment *segment = r->first_segment;
    struct RowSegment *prev_segment = NULL;
    int n = col + 1;

    while(segment->length < n) {
        n = n - segment->length;
        prev_segment = segment;
        segment = segment->next;
    }

    if (segment->status == 0) {
        if (segment->length == 1) {
            segment->status = 1;
            return;
        }
        if (segment->length > 1) {
            if (n == 1) {
                Planta *planta = (Planta*)malloc(sizeof(Planta));

                struct RowSegment *new = (struct RowSegment*)malloc(sizeof(struct RowSegment));
                new->length = 1;
                new->next = segment;
                new->planta_data = planta;
                new->start_col = col;
                new->status = 1;

                if (prev_segment != NULL) prev_segment->next = new;
                else r->first_segment = new;

                segment->length = segment->length - 1;
                segment->start_col = segment->start_col + 1;

                return;
            }
            if (segment->length == n) {
                Planta *planta = (Planta*)malloc(sizeof(Planta));

                struct RowSegment *new = (struct RowSegment*)malloc(sizeof(struct RowSegment));
                new->length = 1;
                new->next = segment->next;
                new->planta_data = planta;
                new->start_col = col;
                new->status = 1;

                segment->next = new;

                segment->length = segment->length - 1;

                return;
            } else {
                Planta *planta = (Planta*)malloc(sizeof(Planta));

                struct RowSegment *new_prev = (struct RowSegment*)malloc(sizeof(struct RowSegment));
                struct RowSegment *new = (struct RowSegment*)malloc(sizeof(struct RowSegment));
                struct RowSegment *new_next = (struct RowSegment*)malloc(sizeof(struct RowSegment));
                
                prev_segment->next = new_prev;

                new_prev->length = n - 1;
                new_prev->next = new;
                new_prev->start_col = segment->start_col;
                new_prev->status = 0;
                new_prev->planta_data = NULL;
                
                new->length = 1;
                new->planta_data = planta;
                new->start_col = col;
                new->status = 1;
                new->next = new_next;

                new_next->length = segment->length - n;
                new_next->next = segment->next;         // <--- sin 'if', siempre
                new_next->start_col = col + 1;
                new_next->status = 0;
                new_next->planta_data = NULL;

                free(segment);
                return;
            }
        }
    }
}

void gameBoardRemovePlant(GameBoard* board, int row, int col) {
    // TODO: Similar a AddPlant, encontrar el segmento que contiene `col`.
    // TODO: Si es un segmento de tipo PLANTA, convertirlo a VACIO y liberar el `planta_data`.
    // TODO: Implementar la lógica de FUSIÓN con los segmentos vecinos si también son VACIO.
    printf("Función gameBoardRemovePlant no implementada.\n");
}

void gameBoardAddZombie(GameBoard* board, int row) {
    // TODO: Crear un nuevo ZombieNode con memoria dinámica.
    // TODO: Inicializar sus datos (posición, vida, animación, etc.).
    // TODO: Agregarlo a la lista enlazada simple de la GardenRow correspondiente.
    printf("Función gameBoardAddZombie no implementada.\n");
}

void gameBoardUpdate(GameBoard* board) {
    if (!board) return;
    // TODO: Re-implementar la lógica de `actualizarEstado` usando las nuevas estructuras.
    // TODO: Recorrer las listas de zombies de cada fila para moverlos y animarlos.
    // TODO: Recorrer las listas de segmentos de cada fila para gestionar los cooldowns y animaciones de las plantas.
    // TODO: Actualizar la lógica de disparo, colisiones y spawn de zombies.
}

void gameBoardDraw(GameBoard* board) {
    if (!board) return;
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex_background, NULL, NULL);

    // TODO: Re-implementar la lógica de `dibujar` usando las nuevas estructuras.
    // TODO: Recorrer las listas de segmentos para dibujar las plantas.
    // TODO: Recorrer las listas de zombies para dibujarlos.
    // TODO: Dibujar las arvejas y el cursor.

    SDL_RenderPresent(renderer);
}

SDL_Texture* cargarTextura(const char* path) {
    SDL_Texture* newTexture = IMG_LoadTexture(renderer, path);
    if (newTexture == NULL) printf("No se pudo cargar la textura %s! SDL_image Error: %s\n", path, IMG_GetError());
    return newTexture;
}
int inicializar() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 0;
    window = SDL_CreateWindow("Plantas vs Zombies - Base para TP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) return 0;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) return 0;
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return 0;
    tex_background = cargarTextura("res/Frontyard.png");
    tex_peashooter_sheet = cargarTextura("res/peashooter_sprite_sheet.png");
    tex_zombie_sheet = cargarTextura("res/zombie_sprite_sheet.png");
    tex_pea = cargarTextura("res/pea.png");
    return 1;
}
void cerrar() {
    SDL_DestroyTexture(tex_background);
    SDL_DestroyTexture(tex_peashooter_sheet);
    SDL_DestroyTexture(tex_zombie_sheet);
    SDL_DestroyTexture(tex_pea);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}


int main(int argc, char* args[]) {

    // TESTS 
    // strDuplicate 

    char* s_empty = "";
    char* s1 = "A";
    char* s2 = "AAA";
    char* s3 = "AAABB";
    char* s4 = "CCC";
    char* s5 = "B";
    char* s6 = "CCCCC";

    printf("--------- strDuplicate ---------\n");
    printf("\n");

    printf("String vacio:\n");
    char* duplicate1 = strDuplicate(s_empty);
    printf("\"%s\"\n",s_empty);
    printf("\"%s\"\n",duplicate1);
    free(duplicate1);
    printf("\n");

    printf("String de un carácter:\n");
    char* duplicate2 = strDuplicate(s1);
    printf("\"%s\"\n",s1);
    printf("\"%s\"\n",duplicate2);
    free(duplicate2);
    printf("\n");

    printf("String que incluya todos los caracteres válidos distintos de cero:\n");
    char printableChars[126 - 32 + 2]; // +1 por el rango, +1 por el '\0'
    int index = 0;
    for (int i = 32; i <= 126; i++) {
        printableChars[index] = (char)i;
        index++;
    }
    printableChars[index] = '\0'; // terminador
    char* duplicate3 = strDuplicate(printableChars);
    printf("\"%s\"\n",printableChars);
    printf("\"%s\"\n",duplicate3);
    free(duplicate3);
    printf("\n");

    printf("\n\n\n");




    printf("--------- strCompare ---------\n");
    printf("\n");

    printf("Dos string vacíos:\n");
    int comparison1 = strCompare(s_empty,s_empty);
    printf("\"%s\"\n",s_empty);
    printf("\"%s\"\n",s_empty);
    printf("Resultado de la comparación: \"%i\"\n",comparison1);
    printf("\n");

    printf("Dos string de un carácter:\n");
    int comparison2 = strCompare(s1,s1);
    printf("\"%s\"\n",s1);
    printf("\"%s\"\n",s1);
    printf("Resultado de la comparación: \"%i\"\n",comparison2);
    printf("\n");
    
    printf("Strings iguales hasta un carácter:\n");
    int comparison3 = strCompare(s2,s3);
    int comparison4 = strCompare(s3,s2);
    printf("s2: \"%s\"\n",s2);
    printf("s3: \"%s\"\n",s3);
    printf("strCompare(s2,s3): \"%i\"\n",comparison3);
    printf("strCompare(s3,s2): \"%i\"\n",comparison4);
    printf("\n");

    printf("Dos strings diferentes:\n");
    int comparison5 = strCompare(s2,s4);
    int comparison6 = strCompare(s4,s2);
    printf("s2: \"%s\"\n",s2);
    printf("s4: \"%s\"\n",s4);
    printf("strCompare(s2,s3): \"%i\"\n",comparison5);
    printf("strCompare(s3,s2): \"%i\"\n",comparison6);
    printf("\n");
    
    printf("\n\n\n");




    printf("--------- strConcatenate ---------\n\n");

    // 1) "" + "AAA"
    char *a1 = strDuplicate(s_empty);
    char *b1 = strDuplicate(s2);
    char *concatenation1 = strConcatenate(a1, b1);  // strConcatenate liberará a1 y b1
    printf("\"%s\" + \"%s\" = \"%s\"\n", s_empty, s2, concatenation1);
    free(concatenation1);

    // 2) "AAA" + ""
    char *a2 = strDuplicate(s2);
    char *b2 = strDuplicate(s_empty);
    char *concatenation2 = strConcatenate(a2, b2);
    printf("\"%s\" + \"%s\" = \"%s\"\n", s2, s_empty, concatenation2);
    free(concatenation2);

    // 3) "AAA" + "B"
    char *a3 = strDuplicate(s2);
    char *b3 = strDuplicate(s5);
    char *concatenation3 = strConcatenate(a3, b3);
    printf("\"%s\" + \"%s\" = \"%s\"\n", s2, s5, concatenation3);
    free(concatenation3);

    // 4) "AAABB" + "CCCCC"
    char *a4 = strDuplicate(s3);
    char *b4 = strDuplicate(s6);
    char *concatenation4 = strConcatenate(a4, b4);
    printf("\"%s\" + \"%s\" = \"%s\"\n", s3, s6, concatenation4);
    free(concatenation4);




    printf("\n\n\n");

    printf("--------- gameBoardAddPlant ---------\n\n");


    srand(time(NULL));
    if (!inicializar()) return 1;

    game_board = gameBoardNew();

    SDL_Event e;
    int game_over = 0;

    while (!game_over) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) game_over = 1;
            if (e.type == SDL_MOUSEMOTION) {
                int mouse_x = e.motion.x;
                int mouse_y = e.motion.y;
                if (mouse_x >= GRID_OFFSET_X && mouse_x < GRID_OFFSET_X + GRID_WIDTH &&
                    mouse_y >= GRID_OFFSET_Y && mouse_y < GRID_OFFSET_Y + GRID_HEIGHT) {
                    cursor.col = (mouse_x - GRID_OFFSET_X) / CELL_WIDTH;
                    cursor.row = (mouse_y - GRID_OFFSET_Y) / CELL_HEIGHT;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                gameBoardAddPlant(game_board, cursor.row, cursor.col);
            }
        }

        gameBoardUpdate(game_board);
        gameBoardDraw(game_board);

        // TODO: Agregar la lógica para ver si un zombie llegó a la casa y terminó el juego

        SDL_Delay(16);
    }

    gameBoardDelete(game_board);
    cerrar();
    return 0;
}
