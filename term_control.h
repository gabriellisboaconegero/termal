#ifndef TERM_CONTROL_H_
#define TERM_CONTROL_H_


void clear_screen(size_t height);

void move_cursor(int x, int y);

int get_size(int *width, int *height);

void echo_on();

void echo_off();

void canon_on();

void canon_off();

void enable_cursor();

void disable_cursor();

void enter_buffer();

void exit_buffer();
#endif
