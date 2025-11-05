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

void printRowSegments(GameBoard* board, int row) {
    GardenRow* r = &board->rows[row];
    RowSegment* seg = r->first_segment;

    printf("    ");

    if (!seg) {
        printf("\n"); // si no hay segmentos, no imprime nada más
        return;
    }

    while (seg) {
        printf("[%s start=%d len=%d]",
               (seg->status == STATUS_PLANTA ? "PLANTA" : "VACIO"),
               seg->start_col,
               seg->length);
        if (seg->next) printf(" -> ");
        seg = seg->next;
    }
    printf("\n");
}




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

    while (segment && segment->length < n) {
        n -= segment->length;
        prev_segment = segment;
        segment = segment->next;
    }
    if (!segment) return; // defensivo, por las dudas

    if (segment->status == 0) {
        if (segment->length == 1) {
            Planta *planta = (Planta*)malloc(sizeof(Planta));
            segment->planta_data = planta;
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
                
                if (prev_segment) prev_segment->next = new_prev;
                else r->first_segment = new_prev;

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
    printf(">>> REMOVE: fila=%d, col=%d\n", row, col);
    if (!board){ 
    return; }// Si el puntero al tablero es NULL, no hacer nada
    if (row < 0 || row >= GRID_ROWS){
    return; }// Lo mismo si la fila está fuera de rango
    if (col < 0 || col >= GRID_COLS) {
        return;} // Lo mismo para la columna
    GardenRow* grow = &board->rows[row];                                     
    RowSegment* pos_anterior = NULL;                                                 
    RowSegment* pos_actual = grow->first_segment; // Saca la fila que corresponde al row dado y agarra el primer segmento y deja un dato para el anterior

    while (pos_actual) {  //Un bucle va a revisar si la columna dada esta dentro del segmeneto actual(mayor al inicio y menor al inicio del siguiente segmento)                                                            
        if (col >= pos_actual->start_col && col < pos_actual->start_col + pos_actual->length) {   
            break;                                                           
        }
        pos_anterior = pos_actual; //Si la columna no esta, pasa al siguiente nodo y guarda el anterior(lo gaurdo para poder juntar despues si esta vacio)                                                     
        pos_actual = pos_actual->next;                                                      
    }

    if (pos_actual->status == STATUS_VACIO) {  //Si no hay planta termina aca
        printRowSegments(board, row);
        return;                                                               
    }

 
    if (pos_actual->planta_data) {  //Si hay planta, libero la memoria y dejo data en null(es como lo muestra el esquemita en la consigna)
        free(pos_actual->planta_data);                                              
        pos_actual->planta_data = NULL;                                              
    }

    RowSegment* pos_siguiente = pos_actual->next; // Agarro el siguiente nodo, que ya teniendo el anterior me va a permitir hacer toda la logica de fusion

    int anterior_vacio = (pos_anterior != NULL && pos_anterior->status == STATUS_VACIO); //Chequeo si el anterior y el siguiente son vacios
    int siguiente_vacio = (pos_siguiente != NULL && pos_siguiente->status == STATUS_VACIO);

    if (!anterior_vacio && !siguiente_vacio) { //Si los dos tienen planta, le doy propiedades de vacio al actual                                   
        pos_actual->status = STATUS_VACIO;                                               
        pos_actual->planta_data = NULL;      
        printRowSegments(board, row);                                            
        return;                                                                   
    }else if (anterior_vacio && !siguiente_vacio) { //Si solo el anterior es vacio:
        pos_anterior->length = pos_anterior->length + pos_actual->length; //Sumo longitud del actual al anterior, cambio el puntero del anterior al del actual y libero el actual
        pos_anterior->next = pos_actual->next;                                             
        free(pos_actual); 
        printRowSegments(board, row);                                                         
        return;                                                              
    }else if (!anterior_vacio && siguiente_vacio) { //Si solo el siguiente es vacio:
        pos_actual->status = STATUS_VACIO; //Le doy propiedades de nodo vacio al actual
        pos_actual->planta_data = NULL;
        pos_actual->length = pos_actual->length + pos_siguiente->length; //Sumo la longitud del siguiente al actual, cambio el puntero del actual al del siguiente(osea el siguiente del siguiente) y libero el siguiente
        pos_actual->next = pos_siguiente->next;
        free(pos_siguiente);
        printRowSegments(board, row);
        return;
    }else { //Else porque solo me queda una posibilidad: que los dos sean vacios:
        pos_anterior->length = pos_anterior->length + pos_actual->length + pos_siguiente->length; //Sumo todas las longitudes en el anterior, cambio el puntero del anterior al del siguiente y libero al actual y al siguiente
        pos_anterior->next = pos_siguiente->next;
        free(pos_actual);
        free(pos_siguiente); 
        printRowSegments(board, row);
        return;
    }
}

void gameBoardAddZombie(GameBoard* board, int row) {
    // Idea: crear un nodo de zombi y encadenarlo al inicio de la lista de su fila.
    // Mantener una lista por fila abarata colisiones (solo se compara dentro de la fila).

    if (!board) return;
    if (row < 0 || row >= GRID_ROWS) return;

    ZombieNode* node = (ZombieNode*)malloc(sizeof(ZombieNode));
    if (!node) return;

    Zombie* z = &node->zombie_data;
    z->activo = 1;
    z->vida = 100;
    z->row = row;
    z->current_frame = 0;
    z->frame_timer = 0;
    z->pos_x = (float)SCREEN_WIDTH;            // Arranca fuera de la grilla, a la derecha.

    z->rect.w = CELL_WIDTH;
    z->rect.h = CELL_HEIGHT;
    z->rect.x = (int)z->pos_x;                 // x para dibujar/colisión (snap del float).
    z->rect.y = GRID_OFFSET_Y + row * CELL_HEIGHT;

    // Inserción O(1) al frente: suficiente para este TP.
    node->next = board->rows[row].first_zombie;
    board->rows[row].first_zombie = node;
}

void gameBoardUpdate(GameBoard* board) {
    if (!board) return;
    // TODO: Re-implementar la lógica de `actualizarEstado` usando las nuevas estructuras.
    // TODO: Recorrer las listas de zombies de cada fila para moverlos y animarlos.
    // TODO: Recorrer las listas de segmentos de cada fila para gestionar los cooldowns y animaciones de las plantas.
    // TODO: Actualizar la lógica de disparo, colisiones y spawn de zombies.
}

void gameBoardDraw(GameBoard* board) {
    // Idea: separar dibujo de lógica. Se dibuja en orden:
    // 1) Fondo, 2) Plantas por segmentos (solo las de estado PLANTA),
    // 3) Zombis por listas de fila, 4) Arvejas del array, 5) Cursor.
    if (!board) return;

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex_background, NULL, NULL);

    // --- Plantas (segmentos por fila) ---
    for (int r = 0; r < GRID_ROWS; r++) {
        RowSegment* seg = board->rows[r].first_segment;
        while (seg) {
            if (seg->status == STATUS_PLANTA && seg->planta_data) {
                Planta* p = seg->planta_data;
                int frame = p->current_frame % PEASHOOTER_TOTAL_FRAMES;

                SDL_Rect src = {
                    frame * PEASHOOTER_FRAME_WIDTH, 0,
                    PEASHOOTER_FRAME_WIDTH, PEASHOOTER_FRAME_HEIGHT
                };
                SDL_Rect dst = {
                    GRID_OFFSET_X + seg->start_col * CELL_WIDTH,
                    GRID_OFFSET_Y + r * CELL_HEIGHT,
                    CELL_WIDTH, CELL_HEIGHT
                };
                SDL_RenderCopy(renderer, tex_peashooter_sheet, &src, &dst);
            }
            seg = seg->next;
        }
    }

    // --- Zombis (lista por fila) ---
    for (int r = 0; r < GRID_ROWS; r++) {
        ZombieNode* node = board->rows[r].first_zombie;
        while (node) {
            Zombie* z = &node->zombie_data;
            if (z->activo) {
                int frame = z->current_frame % ZOMBIE_TOTAL_FRAMES;
                SDL_Rect src = {
                    frame * ZOMBIE_FRAME_WIDTH, 0,
                    ZOMBIE_FRAME_WIDTH, ZOMBIE_FRAME_HEIGHT
                };
                SDL_RenderCopy(renderer, tex_zombie_sheet, &src, &z->rect);
            }
            node = node->next;
        }
    }

    // --- Arvejas ---
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (board->arvejas[i].activo) {
            SDL_RenderCopy(renderer, tex_pea, NULL, &board->arvejas[i].rect);
        }
    }

    // --- Cursor visual ---
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
    SDL_Rect cursor_rect = {
        GRID_OFFSET_X + cursor.col * CELL_WIDTH,
        GRID_OFFSET_Y + cursor.row * CELL_HEIGHT,
        CELL_WIDTH, CELL_HEIGHT
    };
    SDL_RenderDrawRect(renderer, &cursor_rect);

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


// ========= TESTING =========

void stringFunctionsTests(){
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
}

void gameBoardAddPlantTests(){
    game_board = gameBoardNew();

    int row1 = 0;
    int row2 = 1;
    int row3 = 2;
    int col = 0;
    
    printf("--------- gameBoardAddPlant ---------\n");
    printf("\n");

    printf("Agregar una planta en una fila vacía. Agregar tanto en el medio como en los extremos:\n");
    printf("\n");
    printf("Planta en la primera fila columna 0:\n");
    gameBoardAddPlant(game_board,row1,0);
    printRowSegments(game_board, row1);
    printf("Planta en la primera fila columna 3:\n");
    gameBoardAddPlant(game_board,row1,3);
    printRowSegments(game_board, row1);
    printf("Planta en la primera fila columna 8:\n");
    gameBoardAddPlant(game_board,row1,8);
    printRowSegments(game_board, row1);
    printf("\n");

    printf("Llenar una fila completa de plantas:\n");
    printf("Segunda fila llenada:\n");
    while (col < 9) {
        gameBoardAddPlant(game_board,row2,col);
        col++;
    }
    printRowSegments(game_board, row2);
    printf("\n");

    printf("Intentar agregar una planta en una celda ya ocupada:\n");
    gameBoardAddPlant(game_board,row3,5);
    printf("Tercera fila antes de intentar agregar una planta en una celda ya ocupada:\n");
    printRowSegments(game_board, row3);
    gameBoardAddPlant(game_board,row3,5);
    printf("Luego:\n");
    printRowSegments(game_board, row3);
    printf("\n");

    gameBoardDelete(game_board);
    printf("\n\n\n");
}




int main(int argc, char* args[]) {

    stringFunctionsTests();

    gameBoardAddPlantTests();


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
                if (e.button.button == SDL_BUTTON_LEFT) {
                    gameBoardAddPlant(game_board, cursor.row, cursor.col);
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    gameBoardRemovePlant(game_board, cursor.row, cursor.col);
                }
            }

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_z) {
                    gameBoardAddZombie(game_board, cursor.row);
                }
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