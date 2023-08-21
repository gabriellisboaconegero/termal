#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include "term_control.h"

#define DEBUG_TTY "log.txt"
#define DEBUG(fd, fmt, ...) fprintf(fd, fmt, __VA_ARGS__)
#define ARR_SZ(xs) (sizeof(xs)/sizeof(xs[0]))
#define MAX_CHILD 4
#define TRANSPARENT_PIXEL '\0'

#define printf_to_view(vw, x, y, sz, fmt, ...)      \
    do{                                             \
        __b__ = malloc(sizeof(char) * (sz));  \
        if (__b__ == NULL)                          \
            break;                                  \
        snprintf(__b__, (sz), fmt, __VA_ARGS__);    \
        print_to_view((vw), (x), (y), (sz), __b__); \
        free(__b__);                                \
    }while(0);                                      
#define POSTYPE struct {int x, y, width, height;}

FILE *f;

struct BaseView {
    POSTYPE;
    char *buffer;
}BaseView;

struct TextView {
    POSTYPE;
    char *text;
    enum {
        CENTER,
        LEFT
    } alignment;
    enum {
        YES,
        NO
    } wraping;
    char bg;
    int length;
}TextView;

// Utils //
void clamp_int(int *x, int min, int max){
    if (*x > max) *x = max;
    if (*x < min) *x = min;
}

int in_range(int x, int a, int b){
    return (x >= a && x <= b);
}

int is_printable_char(char c){
    return (' ' <= c && c <= '~');
}
// Utils //

// View //
void fill_view(struct BaseView *vw, char c){
    for (int i = 0; i < vw->height; i++){
        for (int j = 0; j < vw->width; j++)
            vw->buffer[i * vw->width + j] = c;
    }
}

struct BaseView *create_view(int width, int height, int x, int y){
    struct BaseView *vw = malloc(sizeof(BaseView));

    if (vw == NULL)
        return NULL;

    if ((vw->buffer = malloc(sizeof(char) * width * height)) == NULL){
        free(vw);
        return NULL;
    }

    vw->width = width;
    vw->height = height;
    vw->x = x;
    vw->y = y;
    fill_view(vw, ' ');

    return vw;
}

struct BaseView *destroy_view(struct BaseView *vw){
    free(vw->buffer);
    free(vw);

    return NULL;
}

// return 1 se conseguir setar o valor
int set_value(struct BaseView *vw, int x, int y, char value){
    if (vw == NULL ||
       !in_range(x, 0, vw->width-1) || !in_range(y, 0, vw->height-1)
    )
        return 0;
    vw->buffer[y * vw->width + x] = value;
    return 1;
}

// Joga o buffer de vw em vw2->buffer
// retorna -1 se tiver erro
int render_vw_to_view(struct BaseView *vw, struct BaseView *vw2){
    int x = 0;
    int y = 0;
    int rendered = 0;
    char value;
    if (vw == NULL || vw2 == NULL)
        return -1;

    for (int i = 0; i < vw->height; i++){
        // y relativo a vw
        y = vw->y + i;
        for (int j = 0; j < vw->width; j++){
            // x relativo a vw
            x = vw->x + j;
            // Pega o valor no buffer vw, para jogar em vw2
            value = vw->buffer[i * vw->width + j];
            if (value != TRANSPARENT_PIXEL &&
                in_range(x, 0, vw2->width - 1) &&
                in_range(y, 0, vw2->height - 1))
            {
                vw2->buffer[y * vw2->width + x] = value;
                rendered++;
            }
        }
    }

    return rendered;
}

// Imprime um texto no buffer de vw
// retorna -1 se algo der errado, caso contratio quantos valores foram impressos
int print_to_view(struct BaseView *vw, int x_off, int y_off,
                           int txt_sz, char *txt)
{
    char v;
    int x = x_off, y = y_off;
    int rendered = 0;
    if (vw == NULL)
        return -1;

    for (int i = 0; i < txt_sz; i++){
        v = txt[i];
        if (v == TRANSPARENT_PIXEL){
            x++;
        }else if (v == '\n'){
            y++;
            x = x_off;
        }else if (is_printable_char(v)){
            if (in_range(x, 0, vw->width - 1) && in_range(y, 0, vw->height - 1)){
                vw->buffer[y * vw->width + x] = v;
                x++;
                rendered++;
            }
        }else{
            fprintf(stderr, "[ERRO]: Carcater com codigo '%d' nao suportado\n", (int)v);
            return rendered;
        }

    }

    return rendered;
}

void render_view(struct BaseView *vw){
    if (vw == NULL)
        return;

    for (int y = 0; y < vw->height - 1; y++)
        printf("%.*s\n", vw->width, &vw->buffer[y * vw->width]);
}
// View //
void set_terminal(void){
    echo_off();
    canon_off();
    enter_buffer();
    disable_cursor();
}

void reset_terminal(void){
    echo_on();
    canon_on();
    enable_cursor();
    exit_buffer();
}

int running = 1;
void on_sigint(int sig){
    running = 0;
}

struct TextView *create_text(int width, int height, int x, int y){
    struct TextView *txt = calloc(1, sizeof(TextView));

    if (txt == NULL)
        return NULL;

    txt->width = width;
    txt->height = height;
    txt->x = x;
    txt->y = y;
    txt->alignment = LEFT;
    txt->wraping = NO;
    txt->bg = '.';
    txt->length = 0;

    return txt;
}
// TODO: melhor forma de retornar
char *load_text(struct TextView *txt, const char *text){
    if (txt == NULL)
        return NULL;
    txt->length = strlen(text);
    txt->text = strdup(text);
    if (txt->text == NULL)
        return NULL;
    return txt->text;
}

struct TextView *destroi_text(struct TextView *txt){
    if (txt == NULL)
        return NULL;
    if (txt->text != NULL)
        free(txt->text);
    free(txt);

    return NULL;
}

// TODO: melhor forma de retornar
void render_text_to_view(struct TextView *txt, struct BaseView *v){
    int dx, dy, x, y;
    dx = dy = 0;
    if (txt == NULL || v == NULL)
        return;
    x = txt->x;
    y = txt->y;
    for (int i = 0; i < txt->height; i++)
        for (int j = 0; j < txt->width; j++)
            v->buffer[(y + i) * v->width + x + j] = txt->bg;      
    switch (txt->wraping){
        case NO:
            for (int i = 0; i < txt->length; i++){
                if (txt->text[i] == '\n'){
                    dx = 0;
                    dy++;
                }
                // TODO: verificar se x + dx e y + dy estao dentro da view
                else if (in_range(dx, 0, txt->width-1) &&
                    in_range(dy, 0, txt->height-1))
                {
                    v->buffer[(y + dy) * v->width + x + dx] = txt->text[i];      
                    dx++;
                }
            }
            break;
        case YES:
            for (int i = 0; i < txt->length; i++){
                if (txt->text[i] == '\n'){
                    dx = 0;
                    dy++;
                }
                // TODO: verificar se x + dx e y + dy estao dentro da view
                else{
                    if (!in_range(dx, 0, txt->width-1)){
                        dx = 0;
                        dy++;
                    }
                    if (in_range(dy, 0, txt->height-1)){
                        v->buffer[(y + dy) * v->width + x + dx] = txt->text[i];      
                        dx++;
                    }
                }
            }
            break;
        default: break;
    }
}

int main(){
    int width, height;
    // __b__ usado para o macro printf_to_view
    char c, *__b__;
    f = fopen(DEBUG_TTY, "w");
    if (signal(SIGINT, on_sigint) == SIG_ERR){
        fprintf(stderr, "[ERRO]: Nao foi possivel capturar o sinal se SIGINT\n");
        exit(1);
    }

    get_size(&width, &height);
    set_terminal();

    struct BaseView *root = create_view(width, height, 0, 0);
    fill_view(root, '_');
    struct TextView *txt = create_text(width/4, height/2, 10, 10);
    load_text(txt, "ola meu velho amigo\nComo esta?\n\n\nMeu mano eu estou meuite0 bem vomo pode algo tao lindo assim nao eh? Como vai pedor\n\n\n\n\n\n\n\n\n\n\nele esta bem????????????\n\n\n\n\nalsadaio  asdasdsdad  adsaddasdsadasd asdadasdad a asdadsadada");

    clear_screen(root->height);
    txt->wraping = YES;
    render_text_to_view(txt, root);
    render_view(root);

    while(running);;

    reset_terminal();
    fclose(f);
    destroy_view(root);
    destroi_text(txt);
    return 0;
}
