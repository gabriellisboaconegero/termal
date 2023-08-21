CC = gcc
CFLAGS = -Wall -Wextra -g
OBJS = termal.o term_control.o

all: $(OBJS)
	$(CC) $^ -o termal

%.o: %.c %.h
	$(CC) -c $< $(CFLAGS) -o $@

termal.o: termal.c
	$(CC) -c $< $(CFLAGS) -o $@

raw: raw.c raw.h
	$(CC) $^ $(CFLAGS) -o $@

purge: clean
	rm -rf termal

clean:
	rm -rf *.o raw
