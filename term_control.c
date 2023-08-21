#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "term_control.h"

void clear_screen(size_t height){
    printf("\r");
    printf("\x1B[2K");
    for (size_t i = 1; i < height; i++){
        // move up and clear line
        printf("\x1b[A1\x1b[2K");
    }
    printf("\r");
    fflush(stdin);
}

void move_cursor(int x, int y){
    printf("\x1B[%d;%dH", y, x);
}

int get_size(int *width, int *height){
    struct winsize size;
    int r = ioctl(1, TIOCGWINSZ, &size);
    *width = size.ws_col;
    *height = size.ws_row;

    return r;
}

void echo_on(){
    struct termios term;
    tcgetattr(1, &term);
    term.c_lflag |= ECHO;
    tcsetattr(1, TCSANOW, &term);
}

void echo_off(){
    struct termios term;
    tcgetattr(1, &term);
    term.c_lflag &= ~ECHO;
    tcsetattr(1, TCSANOW, &term);
}

void canon_on(){
    struct termios term;
    tcgetattr(1, &term);
    term.c_lflag |= ICANON;
    tcsetattr(1, TCSANOW, &term);
}

void canon_off(){
    struct termios term;
    tcgetattr(1, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(1, TCSANOW, &term);
}

void enable_cursor(){
    printf("\x1B[?25h");
}

void disable_cursor(){
    printf("\x1B[?25l");
}

void enter_buffer(){
    printf("\x1B[?1049h");
}

void exit_buffer(){
    printf("\x1B[?1049l");
}
