#!/bin/bash

# valid values : unix w32
platform=unix



BINARY=dwatcher
FLAGS= -Wall -std=iso9899:1990 -pedantic -g
LIBS= -lneuro -lm -lneuronet
OBJECTS= main.o packet.o core.o server.o client.o passive_client.o
GCC=gcc

FLAGS += -DVERSION="\"$(MAJOR).$(MINOR).$(MICRO)\""


ifeq ($(platform), w32)
	platform=$(HOME)/cross

	GCC=$(platform)/bin/gcc
#	FLAGS += -L$(platform)/include -I$(platform)/include
else
	FLAGS += -DACTIVE_CLIENT_ENABLED=1
	OBJECTS += active_client.o
	LIBS += -lutil
endif

ALL: $(BINARY)

$(BINARY): $(OBJECTS)
	$(GCC) $(FLAGS) $(LIBS) $(OBJECTS) -o $(BINARY)
	
%.o : %.c
	$(GCC) $(FLAGS) -c $<

clean:
	rm -f $(BINARY) *.o
