# 30/05/2018

CFLAGS = -Wall -D_GNU_SOURCE
LDFLAGS = -lrt

main: receive.o main.o

main.o: receive.h main.c

receive.o: receive.h receive.c
