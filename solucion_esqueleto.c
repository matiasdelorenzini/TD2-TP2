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
    Arveja arvejas[MAX_ARVEJAS];
    int zombie_spawn_timer;
} GameBoard;

void printRowSegments(GameBoard* board, int row) {
    GardenRow* r = &board->rows[row];
    RowSegment* segment = r->first_segment;

    printf("    ");

    if (!segment) {
        printf("\n");
        return;
    }

    while (segment) {
        printf("[%s start=%d len=%d]",
               (segment->status == STATUS_PLANTA ? "PLANTA" : "VACIO"),
               segment->start_col,
               segment->length);
        if (segment->next) printf(" -> ");
        segment = segment->next;
    }
}

void printZombieList(GameBoard* board, int row) {
    GardenRow* r = &board->rows[row];
    ZombieNode* zombie = r->first_zombie;

    printf("    ");

    if (!zombie) {
        printf("\n");
        return;
    }

    int i = 1;

    while (zombie->next != NULL) {
        printf("[z %i]",i);
        i++;
        zombie = zombie->next;
        if (zombie->next != NULL) printf(" -> ");
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
    // Cantidad de caracteres totales de src
    int n = 0;

    for (int i = 0; *(src+i) != '\0'; i++) {
        n++;
    }
    
    // Asignación de memoria con malloc()
    char *res = (char*)malloc(sizeof(char) * (n + 1));

    // Concatenación secuencial de los caracteres del original en la copia
    for (int i = 0; *(src+i) != '\0'; i++) {
        *(res+i) = *(src+i);
    }
    *(res+n) = '\0';

    return res;
}

int strCompare(char* s1, char* s2) {
    // Se analiza caracter por caracter a ambos strings. Cuando difieren, se calcula cuál es el mayor
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

    // Si tienen distinta longitud, el mayor es el que tenga más caracteres
    if (s1[i] == '\0' && s2[i] != '\0') {
        return 1;
    }
    if (s1[i] != '\0' && s2[i] == '\0') {
        return -1;
    }
    return 0;
}

char* strConcatenate(char* src1, char* src2) {
    // Cantidad de caracteres totales de strConcatenate(src1, src2)
    int n = 0;
    for (int i = 0; *(src1+i) != '\0'; i++) {
        n++;
    }
    for (int i = 0; *(src2+i) != '\0'; i++) {
        n++;
    }

    // Asignación de memoria con malloc()
    char *res = (char*)malloc(sizeof(char) * (n + 1));

    // Se añaden secuencialmente, caracter por caracter, todos los caracteres de ambos strings, y luego el caracter final '\0'
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

    // Se liberan los strings originales
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
    // Se recorren todas las rows
    for (int i = 0; i < GRID_ROWS; i++) {
        // En cada row, se libera la informacion de todos los segmentos
        struct GardenRow *garden_row = &(board->rows[i]);

        if (garden_row->first_segment != NULL) {
            struct RowSegment *segment = garden_row->first_segment;

            while (segment->next !=  NULL)
            {
                struct RowSegment *tofree = segment;
                segment = segment->next;

                // Antes de liberar cada segmento, se liberan sus plantas, de contener alguna
                if(tofree->status == 1) free(tofree->planta_data);
                free(tofree);
            }
            if(segment->planta_data) free(segment->planta_data);
            free(segment);
        }

        // Se procede a la liberación de todos los Zombies
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
    // Por último, le libera el tablero
    free(board);
}

void gameBoardAddPlant(GameBoard* board, int row, int col) {
    // Encuentra la GardenRow y se define una longitud "n" en base 1 para facilitar operaciones con segmentos
    struct GardenRow *r = &(board->rows[row]);
    
    struct RowSegment *segment = r->first_segment;
    struct RowSegment *prev_segment = NULL;
    int n = col + 1;

    // La longitud "n" se recorta segmento a segmento hasta encontrar el segmento que contiene la columna en la que se quiere añadir la planta
    while (segment && segment->length < n) {
        n = n - segment->length;
        prev_segment = segment;
        segment = segment->next;
    }
    if (!segment) return;

    // Si el segmento está vacío, se procede con la lógica de adición de planta
    if (segment->status == 0) {

        // Si el segmento tiene 1 de largo, simplemente se modifica su "status" y "planta_data", sin necesidad de dividir al segmento
        if (segment->length == 1) {
            Planta *planta = (Planta*)malloc(sizeof(Planta));
            segment->planta_data = planta;
            segment->status = 1;
            return;
        }

        // Si el segmento es más largo que 1, se añade una planta y se modifican los segmentos involucrados según sea el caso
        if (segment->length > 1) {

            /* 
            Nuestra longitud n, ya recortada hasta ser igual o menor que la longitud del segmento que contiene a la columna en la 
            cual se quiere añadir la planta, es ahora el índice de la columna relativa a segment en la cual queremos añadir la 
            planta, en base 1.

            Si esta posición es 1, se añade la planta en la primera posición del segmento, por lo tanto:
                - Se modifica el campo next del segmento previo (si existe) para que apunte a la planta
                - Se crea un nuevo segmento para alojar a la planta
                - Se modifica la longitud del segmento que contenía a la columna de la nueva planta para dejarle espacio al de la planta
            */ 
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

            /*
            Si esta posición equivale al largo del segmento, significa que la planta se va a añadir en la última posición del mismo,
            por ende:
                - Se modifica la longitud del segmento que contenía a la columna de la nueva planta para dejarle espacio al de la planta,
                además de su campo next para que apunte a la misma
                - Se crea un nuevo segmento para alojar a la planta
                - Se le hace apuntar al siguiente segmento (si lo hay) a la planta nueva
            */ 
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
            } 
            
            /*
            Si esta posición está en algúna posición del segmento que no sean los extremos:
                - Se crea un nuevo segmento que aloje a la planta
                - Se crea un nuveo segmento que representará a la parte del segmento original anterior a la planta
                - Se crea un nuveo segmento que representará a la parte del segmento original posterior a la planta
                - Se definen todos los datos necesarios de estos tres segmentos, y se apuntan entre si de la forma
                (segmento vacio previo) -> (planta) -> (segmento vacio posterior)
                - Se libera la memoria del segmento original
            */ 
            else {
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
                new_next->next = segment->next;
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
    // Si el puntero al tablero es NULL, o si la fila o la columna está fuera de rango, no hacer nada
    if (!board || row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) return; 
    
    // Saca la fila que corresponde al row dado y agarra el primer segmento y deja un dato para el anterior
    GardenRow* grow = &board->rows[row];                                     
    RowSegment* pos_anterior = NULL;                                                 
    RowSegment* pos_actual = grow->first_segment; 

    // Un bucle va a revisar si la columna dada esta dentro del segmeneto actual (mayor al inicio y menor al inicio del siguiente segmento)                                                            
    while (pos_actual) { 
        if (col >= pos_actual->start_col && col < pos_actual->start_col + pos_actual->length) {   
            break;                                                           
        }
        // Si la columna no esta, pasa al siguiente nodo y guarda el anterior (lo guardo para poder juntar despues si esta vacio)              
        pos_anterior = pos_actual;                                        
        pos_actual = pos_actual->next;                                                      
    }

    // Si no hay planta termina aca
    if (pos_actual->status == STATUS_VACIO) {  
        return;                                                               
    }
 
    // Si hay planta, libero la memoria y dejo data en null (es como lo muestra el esquemita en la consigna)
    if (pos_actual->planta_data) {
        free(pos_actual->planta_data);                                              
        pos_actual->planta_data = NULL;                                              
    }

    // Agarro el siguiente nodo, que ya teniendo el anterior me va a permitir hacer toda la logica de fusion
    RowSegment* pos_siguiente = pos_actual->next;

    // Chequeo si el anterior y el siguiente son vacios
    int anterior_vacio = (pos_anterior != NULL && pos_anterior->status == STATUS_VACIO); 
    int siguiente_vacio = (pos_siguiente != NULL && pos_siguiente->status == STATUS_VACIO);

    // Si los dos tienen planta, le doy propiedades de vacio al actual
    if (!anterior_vacio && !siguiente_vacio) { 
        pos_actual->status = STATUS_VACIO;                                               
        pos_actual->planta_data = NULL;      
        return;                                                                   
    }
    // Si solo el anterior es vacio:
    else if (anterior_vacio && !siguiente_vacio) {
        // Sumo longitud del actual al anterior, cambio el puntero del anterior al del actual y libero el actual
        pos_anterior->length = pos_anterior->length + pos_actual->length; 
        pos_anterior->next = pos_actual->next;                                             
        free(pos_actual); 
        return;                                                              
    }
    //Si solo el siguiente es vacio:
    else if (!anterior_vacio && siguiente_vacio) { 
        // Le doy propiedades de nodo vacio al actual
        pos_actual->status = STATUS_VACIO;
        pos_actual->planta_data = NULL;
        // Sumo la longitud del siguiente al actual, cambio el puntero del actual al del siguiente(osea el siguiente del siguiente) y libero el siguiente
        pos_actual->length = pos_actual->length + pos_siguiente->length;
        pos_actual->next = pos_siguiente->next;
        free(pos_siguiente);
        return;
    }
    // Else porque solo me queda una posibilidad: que los dos sean vacios:
    else {
        // Sumo todas las longitudes en el anterior, cambio el puntero del anterior al del siguiente y libero al actual y al siguiente
        pos_anterior->length = pos_anterior->length + pos_actual->length + pos_siguiente->length;
        pos_anterior->next = pos_siguiente->next;
        free(pos_actual);
        free(pos_siguiente); 
        return;
    }
}

void gameBoardAddZombie(GameBoard* board, int row) {
    // Vamos a crear un zombie y enlazarlo al inicio de la lista de su gardenrow
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
    // Arranca fuera de la grilla, a la derecha.
    z->pos_x = (float)SCREEN_WIDTH;

    z->rect.w = CELL_WIDTH;
    z->rect.h = CELL_HEIGHT;
    // x para dibujar/colisión (snap del float).
    z->rect.x = (int)z->pos_x;
    z->rect.y = GRID_OFFSET_Y + row * CELL_HEIGHT;

    node->next = board->rows[row].first_zombie;
    board->rows[row].first_zombie = node;
}

void gameBoardUpdate(GameBoard* board) {
    if (!board) return;
    for (int i = 0; i < GRID_ROWS; i++) {
        // Para despues
        ZombieNode* pos_anterior = NULL; 
        // Busco en el gameboard que tengo al nodo de zombie
        ZombieNode* pos_actual = board->rows[i].first_zombie;
        // Si existe un zombie, transfiero su dato a un objeto Zombie(que viene de ZombieNode en el board)
        while(pos_actual){
            Zombie* z = &pos_actual->zombie_data;
            if(z->activo){
                float distance_per_tick = ZOMBIE_DISTANCE_PER_CYCLE / (float)(ZOMBIE_ANIMATION_SPEED * ZOMBIE_TOTAL_FRAMES);
                // Se pone menos final porque el zombie arranca al final del eje x
                z->pos_x -= distance_per_tick;
                z->rect.x = (int)z->pos_x;
                z->frame_timer++;
                if(z->frame_timer >= ZOMBIE_ANIMATION_SPEED){
                    z->frame_timer = 0;
                    z->current_frame = (z->current_frame + 1) % ZOMBIE_TOTAL_FRAMES;
                }
                pos_anterior = pos_actual;
                // Paso al siguiente zombie mientras siga activo
                pos_actual = pos_actual->next;
            // En cambio si el zombie esta inactivo
            } else {
                ZombieNode *inactivo = pos_actual;
                if (pos_anterior){ 
                    // En caso de ya haber recorrido un zombie activo, vuelvo a ese zombie(asi libero al actual inactivo de la memoria)
                    pos_anterior->next = pos_actual->next;
                }else{ 
                    // En caso de no haber pos_anterior es que nunca hubo un activo, reemplazo first_zombie para el siguiente nodo(esperando que este activo)
                    board->rows[i].first_zombie = pos_actual->next;
                };
                pos_actual = pos_actual->next;
                free(inactivo);
            };
        };
    };
        
    for (int i = 0; i < GRID_ROWS; i++) {
        RowSegment* segmento = board->rows[i].first_segment; //Inicio definiendo mi primer segmento
        while (segmento) {
            if (segmento->status == STATUS_PLANTA && segmento->planta_data) { //Reviso que haya planta en el segmento
                Planta* p = segmento->planta_data; 
                if (p->cooldown <= 0){
                    p->debe_disparar = 1;
                }else{
                    p->cooldown--;
                }
                p->frame_timer++;
                if (p->frame_timer >= PEASHOOTER_ANIMATION_SPEED) {
                    p->frame_timer = 0;
                    p->current_frame = (p->current_frame + 1) % PEASHOOTER_TOTAL_FRAMES;
                    if (p->debe_disparar && p->current_frame == PEASHOOTER_SHOOT_FRAME) {
                        for (int j = 0; j < MAX_ARVEJAS; j++) {//Hasta que se pase el limite de arvejas
                            if (!board->arvejas[j].activo) { //Si no hay arvejas activas, crearlas en donde empieza la planta
                                Arveja* a = &board->arvejas[j];
                                a->rect.x = GRID_OFFSET_X + segmento->start_col * CELL_WIDTH + CELL_WIDTH / 2;
                                a->rect.y = GRID_OFFSET_Y + i * CELL_HEIGHT + CELL_HEIGHT / 4;
                                a->rect.w = 20;
                                a->rect.h = 20;
                                a->activo = 1;
                                break;
                            }
                        }
                        p->cooldown = 120;
                        p->debe_disparar = 0;
                    }
                }
            }
            segmento = segmento->next; //Una vez termina con un segmento, tiene que revisar lo que sigue en la fila
        }
    }
    
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (board->arvejas[i].activo){ //Chequeo que hayan arvejas
            board->arvejas[i].rect.x += PEA_SPEED; //Copio las lineas originales cambiando las variables a las structs
            if (board->arvejas[i].rect.x > SCREEN_WIDTH) {
                board->arvejas[i].activo = 0;        
            }
        }
    }

    for (int i = 0; i < GRID_ROWS; i++) {
        ZombieNode* nodo = board->rows[i].first_zombie; //Misma manera de reemplazar max zombies que al principio
        while (nodo) { //Recorre todo el nodo de zombies, fijandose 1 por 1 y termina cuando el nodo da NULL, que justo antes de repetir el ciclo nodo pasa a valer el siguiente nodo
            Zombie* z = &nodo->zombie_data;
            if (z->activo) { //Misma logica que al principio
                for (int j = 0; j < MAX_ARVEJAS; j++) { 
                    if (board->arvejas[j].activo){
                        int arveja_row = (board->arvejas[j].rect.y - GRID_OFFSET_Y) / CELL_HEIGHT;
                        if (arveja_row == i && SDL_HasIntersection(&board->arvejas[j].rect, &z->rect)) { //Junto los dos ifs del original en uno y como ahora recorro GRID_ROWS, i es la row donde esta el zombie, asi que igualo i
                            board->arvejas[j].activo = 0;
                            z->vida -= 25;
                            if (z->vida <= 0) z->activo = 0;
                        }    
                    }
                }
            }
            nodo = nodo->next;
        }
    }
            
    board->zombie_spawn_timer--;
    
    if(board->zombie_spawn_timer <=0){
        int row = rand() % GRID_ROWS; //Da a row un valor entre 0 y GRID_ROWS-1
        gameBoardAddZombie(board, row); //Crea un zombie en la fila row
        board->zombie_spawn_timer = ZOMBIE_SPAWN_RATE;
    }
}

void gameBoardDraw(GameBoard* board) {
    //  Se dibuja en este orden:
    // 1) Fondo
    // 2) Plantas
    // 3) Zombis
    // 4) Arvejas
    // 5) Cursor.
    if (!board) return;

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex_background, NULL, NULL);

    // Plantas
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

    // Zombis
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

    // Arvejas
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (board->arvejas[i].activo) {
            SDL_RenderCopy(renderer, tex_pea, NULL, &board->arvejas[i].rect);
        }
    }

    // Cursor visual
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
    
    printf("String de un caracter:\n");
    char* duplicate2 = strDuplicate(s1);
    printf("\"%s\"\n",s1);
    printf("\"%s\"\n",duplicate2);
    free(duplicate2);
    printf("\n");
    
    printf("String que incluya todos los caracteres válidos distintos de cero:\n");
    char printableChars[126 - 32 + 2];
    int index = 0;
    for (int i = 32; i <= 126; i++) {
        printableChars[index] = (char)i;
        index++;
    }
    printableChars[index] = '\0';
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

    printf("Dos string de un caracter:\n");
    int comparison2 = strCompare(s1,s1);
    printf("\"%s\"\n",s1);
    printf("\"%s\"\n",s1);
    printf("Resultado de la comparación: \"%i\"\n",comparison2);
    printf("\n");
    
    printf("Strings iguales hasta un caracter:\n");
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

    char *a1 = strDuplicate(s_empty);
    char *b1 = strDuplicate(s2);
    char *concatenation1 = strConcatenate(a1, b1);
    printf("\"%s\" + \"%s\" = \"%s\"\n", s_empty, s2, concatenation1);
    free(concatenation1);

    char *a2 = strDuplicate(s2);
    char *b2 = strDuplicate(s_empty);
    char *concatenation2 = strConcatenate(a2, b2);
    printf("\"%s\" + \"%s\" = \"%s\"\n", s2, s_empty, concatenation2);
    free(concatenation2);

    char *a3 = strDuplicate(s2);
    char *b3 = strDuplicate(s5);
    char *concatenation3 = strConcatenate(a3, b3);
    printf("\"%s\" + \"%s\" = \"%s\"\n", s2, s5, concatenation3);
    free(concatenation3);

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

    printf("--------- gameBoardAddPlant ---------\n");
    printf("\n");

    printf("Agregar una planta en una fila vacía. Agregar tanto en el medio como en los extremos:\n");
    printf("\n");
    printf("Planta en la primera fila columna 0:\n");
    gameBoardAddPlant(game_board,row1,0);
    printRowSegments(game_board, row1);
    printf("\n");
    printf("Planta en la primera fila columna 3:\n");
    gameBoardAddPlant(game_board,row1,3);
    printRowSegments(game_board, row1);
    printf("\n");
    printf("Planta en la primera fila columna 8:\n");
    gameBoardAddPlant(game_board,row1,8);
    printRowSegments(game_board, row1);
    printf("\n");
    printf("\n");

    printf("Llenar una fila completa de plantas:\n");
    printf("Segunda fila llenada:\n");
    for (int i = 0; i < GRID_COLS; i++) {
        gameBoardAddPlant(game_board,row2,i);
    }
    printRowSegments(game_board, row2);
    printf("\n");
    printf("\n");

    printf("Intentar agregar una planta en una celda ya ocupada:\n");
    gameBoardAddPlant(game_board,row3,5);
    printf("Tercera fila antes de intentar agregar una planta en una celda ya ocupada:\n");
    printRowSegments(game_board, row3);
    printf("\n");
    gameBoardAddPlant(game_board,row3,5);
    printf("Luego:\n");
    printRowSegments(game_board, row3);
    printf("\n");
    printf("\n");

    gameBoardDelete(game_board);
    printf("\n\n\n");
}

void gameBoardRemovePlantTests (){
    game_board = gameBoardNew();

    int row1 = 0;
    int row2 = 1;
    
    printf("--------- gameBoardRemovePlant ---------\n");
    printf("\n");

    printf("Plantar en las columnas 3, 4 y 5. Sacar la planta de la columna 4:\n");
    printf("\n");
    gameBoardAddPlant(game_board,row1,3);
    gameBoardAddPlant(game_board,row1,4);
    gameBoardAddPlant(game_board,row1,5);
    printf("Estado inicial:\n");
    printRowSegments(game_board,row1);
    printf("\n");
    gameBoardRemovePlant(game_board,row1,4);
    printf("Estado final:\n");
    printRowSegments(game_board,row1);
    printf("\n");
    printf("\n");

    printf("Siguiendo el caso anterior, sacar luego la planta en la columna 3:\n");
    printf("\n");
    printf("Estado inicial:\n");
    printRowSegments(game_board,row1);
    printf("\n");
    gameBoardRemovePlant(game_board,row1,3);
    printf("Estado final:\n");
    printRowSegments(game_board,row1);
    printf("\n");
    printf("\n");

    printf("Llenar una fila de plantas. Sacar una del medio:\n");
    printf("\n");
    for (int i = 0; i < GRID_COLS; i++) {
        gameBoardAddPlant(game_board,row2,i);
    }
    printf("Estado inicial:\n");
    printRowSegments(game_board,row2);
    printf("\n");
    gameBoardRemovePlant(game_board,row2,4);
    printf("Estado final:\n");
    printRowSegments(game_board,row2);
    printf("\n");
    printf("\n");

    gameBoardDelete(game_board);
    printf("\n\n\n");
}

void gameBoardAddZombieTests(){
    game_board = gameBoardNew();

    int row1 = 0;
    int row2 = 1;

    printf("--------- gameBoardAddZombie ---------\n");
    printf("\n");

    printf("Tomar una lista de 3 zombies y agregarle uno más:\n");
    printf("\n");
    gameBoardAddZombie(game_board,row1);
    gameBoardAddZombie(game_board,row1);
    gameBoardAddZombie(game_board,row1);
    printf("Lista inicial:\n");
    printZombieList(game_board,row1);
    printf("\n");
    gameBoardAddZombie(game_board,row1);
    printf("Lista final:\n");
    printZombieList(game_board,row1);
    printf("\n");

    printf("Crear una lista de 10000 zombies:\n");
    printf("\n");
    for (int i = 0; i < 1001; i++) {
        gameBoardAddZombie(game_board,row2);
    }
    printZombieList(game_board,row2);
    printf("\n");

    gameBoardDelete(game_board);
    printf("\n\n\n");
}

int main(int argc, char* args[]) {

    stringFunctionsTests();
    gameBoardAddPlantTests();
    gameBoardRemovePlantTests();
    gameBoardAddZombieTests();

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

        for (int i = 0; i < GRID_ROWS && !game_over; i++) {
            ZombieNode* nodo = game_board->rows[i].first_zombie;
            while (nodo) {
                Zombie* z = &nodo->zombie_data;
                if (z->activo) {
                // El borde derecho de un zombie <= El borde izquierdo del tablero
                    if (z->rect.x + z->rect.w <= GRID_OFFSET_X) {
                        printf("GAME OVER - ¡Un zombi llegó a tu casa!\n");
                        game_over = 1;
                        break;
                    }
                }
            // Avanza
            nodo = nodo->next; 
        }
        // Si sucede game_over, el for se corta por la condición && !game_over
    }
        SDL_Delay(16);
    }

    gameBoardDelete(game_board);
    cerrar();
    return 0;
}

/* 

6. Uso de IA

Porcentaje de líneas con asistencia de IA:
Aproximadamente entre un 30 y 40 % del código tiene sugerencias de IA (ChatGPT). El plantéo y estructura es nuestro, pero para algunos errores que no podiamos resolver pediamos ayuda a la IA y tomabamos sus sugerencias. Las soluciones que nos daba no las implementabamos directamente, las interpretamos y adaptamos para que queden bien con lo que veniamos haciendo ya que por lo general la IA genera todo para su propia logica. La misma se usó principalmente para corregir errores de punteros y mejorar la organización de memoria dinámica, mientras que la integración, depuración y pruebas fueron realizadas manualmente.

Verificación de las sugerencias:
Las sugerencias las revisabamos y probabamos para que cumplan los requerimientos del TP. Se compiló y ejecutó el código frecuentemente para validar que la IA no generara errores de tipo o fugas de memoria, y se contrastó con las especificaciones de la consigna. Tambien mencionar que alimentar al workspace donde la IA nos dio sugerencias con los contenidos vistos en clase, es decir las presentaciones mismas, permitio a la IA entender que clase de sugerencias serian utiles y hasta que nivel de complejidad de C.

Dificultades encontradas:
En algunos casos las sugerencias de la IA incluían errores sutiles en el manejo de listas enlazadas o en la liberación de memoria. Lo resolvimos revisando línea por línea y con pruebas manuales. Tambien a veces la IA olvidaba el marco de trabajo o el resto del codigo enviado y sugeria cosas innecesarias debido a que ya estaban implementadas.

Impacto en el aprendizaje:
La IA ayudó a entender mejor como funciona la memoria dinamica en C, ya que permitió comparar soluciones, detectar fallos y entender el porqué de cada corrección. En lugar de reemplazar el aprendizaje, funcionó como una herramienta de apoyo y refuerzo. La IA en consultas basicas a veces llegaba a explicar temas con profundidad antes de aproximar una solucion, para asi demostrarnos el porque hacia las cosas asi y que pudieramos entenderlo.

*/