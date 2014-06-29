#!/bin/bash

# valid values : unix w32
platform=unix

BINDIR:=../bin/
OBJDIR:=../objs/

BINARY=../bin/dwatcher
CFLAGS += -Wall -std=iso9899:1990 -pedantic -g
LIBS= -lneuro -lm -lneuronet -lcrypto
CC ?=gcc

FLAGS += -DVERSION="\"$(MAJOR).$(MINOR).$(MICRO)\""

ifeq ($(platform), w32)
	platform=$(HOME)/cross

	CC=$(platform)/bin/gcc
	CFLAGS += -L$(platform)/include -I$(platform)/include
else
	CFLAGS += -DACTIVE_CLIENT_ENABLED=1
	LIBS += -lutil
endif

SRCS := $(wildcard *.cpp)
SRCS := $(SRCS) $(wildcard *.c) 
OBJS := $(patsubst %.cpp,$(OBJDIR)%.o,$(SRCS))
OBJS := $(patsubst %.c,$(OBJDIR)%.o,$(OBJS))

ALL: DIRS $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $(BINARY)
	
$(OBJDIR)%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BINARY) *.o 
	rm -rf $(OBJDIR)
	rm -rf $(BINDIR)

.PHONY: DIRS
DIRS: 
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)
