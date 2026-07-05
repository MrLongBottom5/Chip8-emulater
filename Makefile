CC = gcc
CFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all:
	$(CC) main.c -o program $(CFLAGS)