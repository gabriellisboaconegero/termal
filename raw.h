#ifndef RAW_H_
#define RAW_H_
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

// ERROS
#define perro(msg, ...) do{             \
    fprintf(stderr, msg, __VA_ARGS__);  \
    fflush(stdout);                     \
    perror(NULL);} while(0)

#define PERRO(msg, ...) perro("[ERRO][%s, %d]: "msg": ", __FILE__, __LINE__, __VA_ARGS__)
#define KILL(msg, ...) {resetTerminal(); PERRO(msg, __VA_ARGS__); exit(1);}
#define EXIT {resetTerminal(); exit(0);}
// ERROS

#define SEND(fmt, ...) do {printf(fmt, __VA_ARGS__); fflush(stdout);} while(0)

// DEFINES
#define STDINF STDIN_FILENO
#define STDOUTF STDOUT_FILENO
#define TIME_IN_TENTHS_OFSECONDS 0
#define MAX_EVENT 100
#define MOD_INC(var, mod) ((var + 1) % (mod))
// 0000 0000 0001 1111 = 0x1f
#define CTRL_KEY(c) ((c) & 0x1f)
#define ESC "\x1b"

#define MOUSE_BUTTON (0)
#define MOUSE_ALL    (1)
#define MOUSE_UNSET  (2)

enum EventKey {
    NOKEY = 128,
    ARROW_UP,
    ARROW_DOWN,
    ARROW_RIGHT,
    ARROW_LEFT,
    HOME,
    END,
    INSERT,
    DELETE,
    PAGE_UP,
    PAGE_DOWN,
    MOUSE,
    F1, F2, F3, F4, F5, F6, F7,
    F8, F9, F10, F11, F12,
};
#define B1          (0x0)
#define B2          (0x1)
#define B3          (0x2)
#define B4          (0x3)
#define SCROLL_UP   (0x4)
#define SCROLL_DOWN (0x5)

#define B_PRESSED   (0x0)
#define B_RELEASED  (0x1)

// EXEMPLO: para verificar CTRL e ALT ao mesmo tempo fazer:
// if (modifier == (CTRL_MOD | ALT_MOD)){...}
#define SHIFT_MOD   (0x1)
#define ALT_MOD     (0x2)
#define CTRL_MOD    (0x4)
// DEFINES

#define BOXIDEF struct {int x, y, height, width;}
struct globalConfig {
    struct termios savedTerm;   
    struct termios rawTerm;   
    BOXIDEF;
};

struct Event {
    int x, y;
    int action;
    int motion;
    int scroll;
    int modifier;
    int button;
};

void setRawTerminal();

void resetTerminal();

void enableCursor();

void disableCursor();

void pushCursor();

void popCursor();

void enterBuffer();

void exitBuffer();

void drawRec(char c, int x1, int y1, int x2, int y2);

void setMouseEvents(int mouse_opt);

void moveCursor(int x, int y);

#define FULL      0
#define CURTOEND  1
void clearScreen(int mode);

void setSigIntHandler(void (*handler)(int));

// Pega o tamanho do terminal
// retorna 0 em caso de erro
// 1 caso contrario
int getTerminalSize(int *rows, int *cols);

// pega um caracter do stdin
int getEvent(struct Event *e);
#endif
