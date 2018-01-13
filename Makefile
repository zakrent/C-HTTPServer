CC = gcc

all:
	$(CC) main.c -o server

clean:
	rm -f *.o server
