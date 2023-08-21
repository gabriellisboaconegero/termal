#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include "raw.h"

static struct globalConfig G;

void setRawTerminal(){
    // refence: https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
    if (tcgetattr(STDINF, &G.rawTerm) == -1)
        KILL("%s", "Erro ao colocar terminal no modo G.rawTerm");
    G.savedTerm = G.rawTerm;

    // IXON: desabilitar ctrl-q e ctrl-s
    // ICRNL: desabilitar a traducao de '\r'(13) para '\n'(10)
    G.rawTerm.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    // OPOST: desabilitar que o terminal traduza '\n' para '\r\n'. Ou seja, apenas um '\n'
    // agora nao volta o cursor para o inicio da linha
    G.rawTerm.c_oflag &= ~(OPOST);
    // ECHO: Desabilitar mostrar os caracteres ao digitar
    // ICANON: Desabilitar esperar por um CR para enviar o input
    // ISIG: Desabiliatr o envio de sinais (ctrl-c(SIGINT), ctrl-z(SIGTSTP))
    // IEXTEN: Desabilitar ctrl-v, que esperava pro outro input antes de enviar o valor 22
    G.rawTerm.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    // G.rawTerm.c_lflag &= ~(ISIG);

    G.rawTerm.c_cflag |= (CS8);
    G.rawTerm.c_cc[VMIN] = 0;
    // time em 1/10 segundos
    G.rawTerm.c_cc[VTIME] = TIME_IN_TENTHS_OFSECONDS;

    if (tcsetattr(STDINF, TCSAFLUSH, &G.rawTerm) == -1)
        KILL("%s", "Erro ao colocar terminal no modo G.rawTerm");
}

void resetTerminal(){
    if (tcsetattr(STDINF, TCSAFLUSH, &G.savedTerm) == -1)
        KILL("%s", "Erro ao restaurar o terminal");
    setMouseEvents(MOUSE_UNSET);
    exitBuffer();
    enableCursor();
}

void enableCursor(){
    if (write(STDOUTF, ESC"[?25h", 6) != 6) KILL("%s", "Nao foi possivel esconder o cursor");
}

void disableCursor(){
    if (write(STDOUTF, ESC"[?25l", 6) != 6) KILL("%s", "Nao foi possivel voltar o cursor");
}

void pushCursor(){
    if (write(STDOUTF, ESC"7", 2) != 2) KILL("%s", "Nao foi possivel salvar posicao do cursor");
}

void popCursor(){
    if (write(STDOUTF, ESC"8", 2) != 2) KILL("%s", "Nao foi possivel restaurar posicao do cursor");
}

void enterBuffer(){
    if (write(STDOUTF, ESC"[?1049h", 8) != 8) KILL("%s", "Nao foi possivel entrar no buffer alternativo");
}

void exitBuffer(){
    if (write(STDOUTF, ESC"[?1049l", 8) != 8) KILL("%s", "Nao foi possivel entrar no buffer alternativo");
}

void drawRec(char c, int x1, int y1, int x2, int y2){
    //printf(ESC"[46;10;10;20;15$x");
    printf(ESC"[%d;%d;%d;%d;%d$x", c, y1, x1, y2, x2);
    fflush(stdout);
}

// Pega o tamanho do terminal
// retorna 0 em caso de erro
// 1 caso contrario
int getTerminalSize(int *cols, int *rows){
    struct winsize ws;

    if (ioctl(STDOUTF, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 0;
    *cols = ws.ws_col;
    *rows = ws.ws_row;

    return 1;
}
void getCursorPos(int *x, int *y){
    char buf[100] = {0};
    char b = '\0';
    if (write(STDOUTF, ESC"[6n", 4) != 4)
        KILL("%s", "Nao foi possivel pegar posicao do cursor");
    while (read(STDINF, &b, 1) > 0){
        printf("%d ", b);
    }
    printf("\r\n");
    // SEND("%s", buf+1);
    // sscanf(buf, "");
}

void setMouseEvents(int mouse_opt){
    if (write(STDOUTF, ESC"[?1003l"ESC"[?1002l"ESC"[?1006l", 24) != 24)
        KILL("%s", "Nao foi possivel habilitar eventos de mouse");
    switch(mouse_opt){
        case MOUSE_BUTTON:
            if (write(STDOUTF, ESC"[?1002h"ESC"[?1006h", 16) != 16)
                KILL("%s", "Nao foi possivel habilitar eventos de mouse");
            break;
        case MOUSE_ALL:
            if (write(STDOUTF, ESC"[?1003h"ESC"[?1006h", 16) != 16)
                KILL("%s", "Nao foi possivel habilitar eventos de mouse");
            break;
        case MOUSE_UNSET: break;
        // TODO: Usar um file de log para erros assim
        default: PERRO("Opcao (%d) invalida para setMouseEvent\n", mouse_opt);
    }
}

void moveCursor(int x, int y){
    printf(ESC"[%d;%dH", y, x);
    fflush(stdout);
}

void clearScreen(int mode){
    switch (mode){
        case FULL: write(STDOUTF, ESC"[2J", 4); break;
        case CURTOEND: 
            moveCursor(G.x, G.y);
            write(STDOUTF, ESC"[J", 3);
            break;
    }
}

void setSigIntHandler(void (*handler)(int)){
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1)
        KILL("%s", "Definindo a funcao para manipular o SIGINT");
}

static int eventBuffer[MAX_EVENT];
static int eventHead = 0;
static int eventCurr = 0;

void pushEvent(int event){
    eventBuffer[eventHead] = event;
    eventHead = MOD_INC(eventHead, MAX_EVENT);
}

static int getInput(int *index){
    char c = '\0';
    if (eventCurr == eventHead){
        if (read(STDINF, &c, 1) == -1)
            KILL("%s", "Erro lendo input (read)");
        // evitar que o buffer fique enchendo de NOKEY
        if (c =='\0')
            return 0;
        eventBuffer[eventHead] = c;
        eventHead = MOD_INC(eventHead, MAX_EVENT);
    }

    *index = eventCurr;
    eventCurr = MOD_INC(eventCurr, MAX_EVENT);
    return 1;
}

// TODO: implementar melhor forma de retornar, usando eventos
// TODO: implementar sistema de push de eventos
int getEvent(struct Event *event){
    char str[100] = {0};
    int bProps, counter = 0, index = 0, savedIndex;
    int funcNum = 0, mod, funcKey;
    
    if (!getInput(&index))
        return NOKEY;

    if (eventBuffer[index] == *ESC){
        savedIndex = index;
        // pega Control Character
        if (!getInput(&index)){
            eventCurr = MOD_INC(savedIndex, MAX_EVENT);
            return eventBuffer[savedIndex];
        }

        // CSI
        if (eventBuffer[index] == '['){

            // pega char de identificacao
            if (!getInput(&index)){
                eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                return eventBuffer[savedIndex];
            }

            // FUNC or CURSOR key
            if (isdigit(eventBuffer[index])){
                funcNum = eventBuffer[index] - '0';
                while (getInput(&index) && isdigit(eventBuffer[index]))
                    funcNum = funcNum * 10 + eventBuffer[index] - '0';
                
                // Tem modifier
                if (eventBuffer[index] == ';'){
                    // se qualquer um falhar retorna
                    if (!getInput(&index) ||            // pega o digito de modifier
                        !isdigit((mod = eventBuffer[index])) || // verificar se e digito
                        !getInput(&index))              // pega o '~'
                    {
                        eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                        return eventBuffer[savedIndex];
                    }
                }

                // se funcNum for 1 eh cursor, nao tem '~' no final
                if (funcNum != 1 && eventBuffer[index] != '~'){
                    eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                    return eventBuffer[savedIndex];
                }

                // event->modifier = mod - 1;
                switch (funcNum){
                    case 1: break; // Cursor key
                    case 2: funcKey = INSERT; break;
                    case 3: funcKey = DELETE; break;
                    case 5: funcKey = PAGE_UP; break;
                    case 6: funcKey = PAGE_DOWN; break;
                    case 15: funcKey = F5; break;
                    case 17: funcKey = F6; break;
                    case 18: funcKey = F7; break;
                    case 19: funcKey = F8; break;
                    case 20: funcKey = F9; break;
                    case 21: funcKey = F10; break;
                    case 23: funcKey = F11; break;
                    case 24: funcKey = F12; break;
                    default:
                        eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                        return eventBuffer[savedIndex];
                    break;
                }
            } // FUNC or CURSOR key

            switch (eventBuffer[index]){
                // CURSOR key
                case 'A': return ARROW_UP;  break;
                case 'B': return ARROW_DOWN; break;
                case 'C': return ARROW_RIGHT; break;
                case 'D': return ARROW_LEFT; break;
                case 'H': return HOME; break;
                case 'F': return END; break;
                case 'P': return F1; break;
                case 'Q': return F2; break;
                case 'R': return F3; break;
                case 'S': return F4; break;
                // FUNC key
                case '~': return funcKey; break;
                // MOUSE pattern: ESC[<%d;%d;%d%c     %c in [m, M]
                case '<':
                    if (event == NULL){
                        PERRO("%s", "Nao foi possivel capturar o evento");
                        eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                        return eventBuffer[savedIndex];
                    }
                    // loop ate chegar no M/m ou n conseguir pegar mais chars
                    while (getInput(&index) &&
                            eventBuffer[index] != 'm' &&
                            eventBuffer[index] != 'M')
                    {
                        if (!isdigit(eventBuffer[index]) && eventBuffer[index] != ';'){
                            eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                            return eventBuffer[savedIndex];
                        }

                        str[counter++] = eventBuffer[index];
                    }

                    // Ultimo char deve ser m/M
                    event->action = eventBuffer[index] == 'm' ? B_RELEASED : B_PRESSED;
                    sscanf(str, "%d;%d;%d",
                            &bProps,
                            &(event->x),
                            &(event->y));

                    // bProbs formato:
                    // [0,1]: button
                    // [2,4]: modifier
                    // [5]: motion
                    // [6]: scroll
                    event->scroll = !!(bProps & 0x40);
                    event->button = bProps & 0x3;
                    // No scroll o botao eh igual a B1 ou B2 se for up ou dawn
                    if (event->scroll){
                        if (event->button == B1)
                            event->button = SCROLL_UP;
                        else
                            event->button = SCROLL_DOWN;
                    }
                    event->modifier = (bProps & 0x1c) >> 2;
                    event->motion =  !!(bProps & 0x20);

                    return MOUSE;
                    break;
                default: break;
            }
        }
        // SS3
        else if (eventBuffer[index] == 'O'){
            // pega char de identificacao
            if (!getInput(&index)){
                eventCurr = MOD_INC(savedIndex, MAX_EVENT);
                return eventBuffer[savedIndex];
            }
            
            switch (eventBuffer[index]){
                case 'P': return F1; break;
                case 'Q': return F2; break;
                case 'R': return F3; break;
                case 'S': return F4; break;
                default: break;
            }   
        }

        eventCurr = MOD_INC(savedIndex, MAX_EVENT);
        return eventBuffer[savedIndex];
    }
    return eventBuffer[index];
}

void exit_termal(int a){
    (void)a;
    char msg[] = "TERMAL - Clique qualquer tecla para sair";
    size_t msg_sz = sizeof(msg);
    clearScreen(FULL);
    moveCursor(G.width / 2 - msg_sz / 2, G.height / 2);
    printf("%s\n", msg);
    setMouseEvents(MOUSE_BUTTON);
    while(getEvent(NULL) == NOKEY);
    EXIT;
}

void exit_raw(int s){
    (void)s;
    char msg[] = "TERMAL - Clique qualquer tecla para sair";
    size_t msg_sz = sizeof(msg);
    clearScreen(CURTOEND);
    moveCursor(G.x + G.width / 2 - msg_sz / 2, G.y + G.height / 2);
    printf("%s\n", msg);
    setMouseEvents(MOUSE_BUTTON);
    while(getEvent(NULL) == NOKEY);
    EXIT;
}

// TODO: jeito de recuperar screen depois de SIGTSTP
/* void sigStpHandler(int s){
    (void)s;
    resetTerminal();
    SEND("%s", ESC"[?47h");
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTSTP, &sa, NULL) == -1)
        KILL("%s", "Definindo a funcao para manipular o SIGTSTP");
    raise(s);
}

void sigContHandler(int s){
    (void)s;
    struct sigaction sa;
    sa.sa_handler = sigStpHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTSTP, &sa, NULL) == -1)
        KILL("%s", "Definindo a funcao para manipular o SIGTSTP");
    sa.sa_handler = sigContHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCONT, &sa, NULL) == -1)
        KILL("%s", "Definindo a funcao para manipular o SIGONT");


    if (tcsetattr(STDINF, TCSAFLUSH, &G.rawTerm) == -1)
        KILL("%s", "Erro ao colocar terminal no modo G.rawTerm");
    setMouseEvents(MOUSE_BUTTON);
    enterBuffer();
    SEND("%s", ESC"[?47l");
} */

int main(){
    int c;
    int quit = 0, lopping = 0, i = 0;
    struct Event event;

    if (!isatty(STDINF))
        KILL("%s", "A entrada nao eh um terminal");
    if (!isatty(STDOUTF))
        KILL("%s", "A saida nao eh um terminal");

    setSigIntHandler(exit_raw);
    setRawTerminal();
    // getCursorPos(&G.x, &G.y);

    setMouseEvents(MOUSE_BUTTON);
    enterBuffer();
    G.x = 1; G.y = 1;
    getTerminalSize(&G.width, &G.height);
    moveCursor(G.x, G.y);

    while (!quit){
        c = getEvent(&event);
        if (lopping && c != NOKEY){
            i = 0;
            lopping = !lopping;
        }
        switch (c){
            case NOKEY:
                if (lopping)
                    SEND("\r%d", i++);
                break;
            case CTRL_KEY('q'):
                exit_termal(0);
                break;
            case 'f':
                clearScreen(CURTOEND);
                // moveCursor(G.x, G.y);
                break;
            case 'r':
                getCursorPos(NULL, NULL);
                lopping = 1;
                break;
            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                printf("Arrow pressed\r\n");
                break;
            case MOUSE:
                if (event.button == B1 && event.action == B_PRESSED){
                    pushCursor();
                    moveCursor(event.x, event.y);
                    drawRec('.', event.x, event.y, event.x + 10, event.y + 5);
                    popCursor();
                }
                break;
            default:
                printf("%d ", c);
                if (isprint(c))
                    printf(" (%c)", c);
                printf("\r\n");
                break;
        }
    }
    resetTerminal();

    return 0;
}

