#!/bin/bash

include version.mk

ALL:
	make -C src

clean:
	make -C src clean
