# 30/05/2018

CFLAGS = -Wall -D_GNU_SOURCE
LDFLAGS = -lrt

main: receive_context.o main.o

main.o: receive_context.h main.c

receive_context.o: receive_context.h receive_context.c
