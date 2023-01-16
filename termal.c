#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define DEBUG_TTY "log.txt"
#define DEBUG(fmt, ...) fprintf(fd, fmt, __VA_ARGS__)
FILE *fd;

#define ARR_SZ(xs) (sizeof(xs)/sizeof(xs[0]))

typedef struct view_t {
    size_t width;
    size_t height;
    size_t x;
    size_t y;

    int c_x;
    int c_y;

    uint32_t *buffer;
}view_t;

typedef struct text_t {
    uint32_t *data;
    size_t data_sz;
    size_t max_sz;
}text_t;

// Unicode //
int u32_to_utf8(uint32_t n, uint32_t *v){
	*v = 0;
	if(n > 1114112 || ( n >= 0xd800 && n <= 0xdfff ) ){
		//encodes U+fffd; replacement character
		*v |= (0xef << 3 * 8);
		*v |= (0xbf << 2 * 8);
		*v |= (0xdf << 1 * 8);
		*v |= (0x00 << 0 * 8);
		return 3;
	}

	// just like 7-bit ascii
	if(n < 128){
		*v = n;
		return 1;
	}

	uint32_t len = 0;
    // 110xxxxx 10xxxxxx
    // 2 ^ 11 - 1 == 2047
	if(n < 2048){
		len = 2;
	}else{
        // 1110xxxx 10xxxxxx 10xxxxxx
        // 2 ^ 16 - 1 == 65535
		if( n < 65536){
			len = 3;
		}else{
			len = 4;
		}
	}

    unsigned char m = 0x80; // 1000 0000
    int i = len - 1;
	while(i >= 0){
		*v |= (m << (len - 1) * 8);
		i--;
		m >>= 1;
	}
    //          0          1          2          3
    // str [0000 0000][0000 0000][0000 0000][0000 0000]
    // v    0000 0000  0000 0000  0000 0000  0000 0000
    //          3          2          1          0
    // set the bits at the start to indicate number of bytes

	//set the most significant bits in the other bytes
	i = len - 2;
	while(i >= 0){
		*v |= (0x80 << i * 8);
		i--;
	}

	//fill in the codepoint
	uint32_t j = 0;
	while(j < len){
		m = 1;
		i = 0;
		while(n && i < 6){
			if(n%2){
				*v |= (m << j * 8);
			}
			n >>= 1;
			m <<= 1;
			i += 1;
		}
		j++;
	}

    return len;
}

uint32_t buf_add_utf8(uint32_t v, uint32_t len, char *buf, size_t buf_sz){
    for (int i = len - 1; i >= 0; i--){
        buf[buf_sz + len - 1 - i] = (v >> i * 8) & 0xff;
    }

    return len;
}
// Unicode //

// Terminal control //
void clear_screen(size_t height){
    printf("\r");
    printf("\x1B[2K");
    for (size_t i = 1; i < height; i++){
        // move up and clear line
        printf("\x1b[A1\x1b[2K");
    }
    printf("\r");
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
// Terminal control //

// Utils //
void clamp_uint(size_t *x, size_t min, size_t max){
    if (*x > max) *x = max;
    if (*x < min) *x = min;
}

void clamp_int(int *x, int min, int max){
    if (*x > max) *x = max;
    if (*x < min) *x = min;
}
// Utils //

// Text //
text_t *create_text(size_t max_sz){
    text_t *txt = malloc(sizeof(text_t));
    if (txt == NULL)
        return NULL;

    if ((txt->data = malloc(sizeof(uint32_t) * max_sz)) == NULL){
        free(txt);
        return NULL;
    }

    memset(txt->data, 0, sizeof(uint32_t) * max_sz);
    txt->max_sz = max_sz;
    txt->data_sz = 0;

    return txt;
}

text_t *destroy_text(text_t *txt){
    free(txt->data);
    free(txt);

    return NULL;
}

size_t printf_text(text_t *txt, size_t size, const char *fmt, ...){
    if (txt == NULL)
        exit(9);
    if (txt->data_sz + size >= txt->max_sz)
        return 0;

    char buf[size];
    va_list ap;
    size_t writed;

    va_start(ap, fmt);
    writed = vsnprintf(buf, size, fmt, ap);
    va_end(ap);

    for (size_t i = 0; i < writed; i++)
        txt->data[txt->data_sz++] = buf[i];

    return writed;
}
// Text //

// View //
void fill_view(view_t *vw, uint32_t c);
view_t *create_view(size_t width, size_t height, size_t x, size_t y){
    view_t *vw = malloc(sizeof(view_t));

    if (vw == NULL)
        return NULL;

    if ((vw->buffer = malloc(sizeof(uint32_t) * width * height)) == NULL){
        free(vw);
        return NULL;
    }

    vw->width = width;
    vw->height = height;
    vw->x = x;
    vw->y = y;
    vw->c_x = 0;
    vw->c_y = 0;
    fill_view(vw, ' ');

    return vw;
}

view_t *destroy_view(view_t *vw){
    free(vw->buffer);
    free(vw);

    return NULL;
}

void set_cursor_view(view_t *vw, int x, int y){
    clamp_int(&x, 0, vw->width - 1);
    clamp_int(&y, 0, vw->height - 1);

    vw->c_x = x;
    vw->c_y = y;
}

void put_on_cursor(view_t *vw, uint32_t c){
    vw->buffer[vw->c_y * vw->width + vw->c_x] = c;
}

size_t render_vw_to_view(view_t *vw, view_t *vw2){
    size_t x = 0;
    size_t y = 0;
    size_t rendered = 0;
    uint32_t value;
    for (size_t i = 0; i < vw->height; i++){
        y = vw->y + i;
        for (size_t j = 0; j < vw->width; j++){
            x = vw->x + j;
            value = vw->buffer[i * vw->width + j];
            // '\0' -> transparent pixel
            if (value != '\0' && x < vw2->width && y < vw2->height){
                vw2->buffer[y * vw2->width + x] = value;
                rendered++;
            }
        }
    }

    return rendered;
}

void fill_view(view_t *vw, uint32_t c){
    for (size_t i = 0; i < vw->height; i++){
        for (size_t j = 0; j < vw->width; j++)
            vw->buffer[i * vw->width + j] = c;
    }
}

//TODO: suport uint32_t and char texts to be displayed
int is_printable_char(uint32_t c){
    return (' ' <= c && c <= '~') || 128 <= c;
}

size_t render_text_to_view(text_t* txt, view_t *vw){
    uint32_t v;
    size_t rendered = 0;
    for (size_t i = 0; i < txt->data_sz; i++){
        v = txt->data[i];
        if (v == '\0')
            break;
        else if (v == '\n'){
            vw->c_y += 1;
            vw->c_x = 0;
        }else if (is_printable_char(v)){
            if (vw->c_x < (int)vw->width && vw->c_y < (int)vw->height){
                vw->buffer[vw->c_y * vw->width + vw->c_x] = v;
                vw->c_x += 1;
                rendered++;
            }
        }else{
            fprintf(stderr, "[ERRO]: Carcater com codigo '%d' nao suportado\n", (int)v);
            return rendered;
        }

    }

    return rendered;
}

char *dump_view(view_t *vw){
    char *buf = malloc(sizeof(char) * vw->width * vw->height * 4 + vw->height);
    size_t buf_sz = 0;
    size_t len;
    uint32_t v;
    if (buf == NULL)
        return NULL;

    memset(buf, 0, vw->width * vw->height * 4 + vw->height);
    for (size_t y = 0; y < vw->height; y++){
        for (size_t x = 0; x < vw->width; x++){
            v = vw->buffer[y * vw->width + x];
            if ( v != '\0'){
                len = u32_to_utf8(v, &v);
                buf_sz += buf_add_utf8(v, len, buf, buf_sz);
            }
        }

        buf[buf_sz++] = '\n';
    }
    
    // cleaning the last added '\n'
    buf[buf_sz - 1] = '\0';

    return buf;
}

void print_view(FILE *fd, view_t *vw){
    char *buf = dump_view(vw);
    fprintf(fd, "%s", buf);
    free(buf);
}
// View //


int main(){
    int width, height;
    fd = fopen(DEBUG_TTY, "w");

    get_size(&width, &height);
    view_t *root = create_view(width, height, 0, 0);
    if (root == NULL){
        fprintf(stderr, "[ERRO]: Nao foi possvel criar screen\n");
        exit(1);
    }
    fill_view(root, ' ');
    text_t *nums = create_text(root->height * 4);
    for (size_t i = 0; i < root->height; i++){
        printf_text(nums, 5, "%ld\n", i);
        set_cursor_view(root, root->width, i);
        put_on_cursor(root, 0x2745);
    }
    set_cursor_view(root, 0, 0);
    render_text_to_view(nums, root);

    echo_off();
    canon_off();

    //disable_cursor();
    //enter_buffer();

    view_t *w1 = create_view(3, root->height/2, root->width/2 - root->width/8, root->height/2 - root->height/4);
    text_t *w1_nums = create_text(w1->height * 5);
    fill_view(w1, '.');
    for (size_t i = 0; i < w1->height; i++)
        printf_text(w1_nums, 5, "%2ld \n", i);
    set_cursor_view(w1, 0, 0);
    render_text_to_view(w1_nums, w1);

    view_t *input = create_view(root->width/4, w1->height, w1->x + w1->width, w1->y);
    text_t *input_txt = create_text(input->width * input->height + input->height);
    fill_view(input, 0x2581);

    render_vw_to_view(input, root);
    render_vw_to_view(w1, root);
    print_view(stdout, root);

    char c = getchar();

    while (c != 'q'){
        if ((is_printable_char(c) && input->c_x < (int)input->width) || 
                (c == '\n' && input->c_y < (int)input->height-1))
        {
            printf_text(input_txt, 2, "%c", c);
        }else if (c == 127 && input_txt->data_sz > 0){
            input_txt->data_sz--;
        }

        fill_view(input, 0x2581);
        set_cursor_view(input, 0, 0);
        render_text_to_view(input_txt, input);

        put_on_cursor(input, 0x276e);
        render_vw_to_view(input, root);
        render_vw_to_view(w1, root);
        clear_screen(root->height);
        print_view(stdout, root);

        c = getchar();
    }

    echo_on();
    canon_on();
    enable_cursor();
    //exit_buffer();

    if (fd != NULL)
        fclose(fd);

    //destroy_view(w1);
    destroy_text(nums);
    destroy_view(root);
    return 0;
}
