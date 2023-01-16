CC = gcc
CFLAGS = -Wall -Wextra

termal: termal.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm termal
