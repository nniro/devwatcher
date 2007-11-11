#!/bin/bash

include version.mk

BINARY=dwatcher
FLAGS= -Wall -std=iso9899:1990 -pedantic -g -I/usr/local/include/neuro/nnet
LIBS= -lneuro -lm -lneuronet
OBJECTS= main.o packet.o core.o server.o client.o
GCC=gcc

FLAGS+= -DVERSION="\"$(MAJOR).$(MINOR).$(MICRO)\""


ALL: $(BINARY)

$(BINARY): $(OBJECTS)
	$(GCC) $(FLAGS) $(LIBS) $(OBJECTS) -o $(BINARY)
	
%.o : %.c
	$(GCC) $(FLAGS) -c $<

clean:
	rm -f $(BINARY) *.o
