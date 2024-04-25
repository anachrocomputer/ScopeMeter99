# Makefile for ScopeMeter 99 utilities

cc=gcc
LD=gcc

all: sm99img
.PHONY: all

sm99img: sm99img.o
	$(LD) -o sm99img sm99img.o

sm99img.o: sm99img.c
	$(CC) -Wall -c -o sm99img.o sm99img.c

clean:
	-rm -rf sm99img sm99img.o
.PHONY: clean

