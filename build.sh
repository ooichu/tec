#!/bin/sh
gcc tec.c elis/elis.c -std=c99 `sdl2-config --cflags --libs` -lm -o tec -O3 -Wall -Wextra -pedantic
strip tec
