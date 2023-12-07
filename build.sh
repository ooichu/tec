#!/bin/sh
ELIS="elis/elis.c -DELIS_PAGE_SIZE=16384 -DELIS_STACK_SIZE=1024"
SDL2=`sdl2-config --libs --cflags`

gcc tec.c $ELIS $SDL2 -lm -o tec -O3 -Wall -Wextra -pedantic -std=c99 
strip tec
