#!/bin/bash

FLAGS= -Wall -std=iso9899:1990 -pedantic -g
LIBS= `pkg-config --libs x11`
GCC=gcc

ALL: getWSize setWSize

getWSize: getWSize.c
	$(GCC) $(FLAGS) $(LIBS) getWSize.c -o getWSize

setWSize: setWSize.c
	$(GCC) $(FLAGS) $(LIBS) setWSize.c -o setWSize
	
clean:
	rm -f getWSize setWSize *.o
