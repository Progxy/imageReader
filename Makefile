FLAGS = -std=c11 -Wall -Wextra

debug:
	gcc $(FLAGS) -D"_DEBUG_MODE_" -g -lm image.c -o out/image `pkg-config --cflags --libs gtk+-3.0`

all: 
	gcc $(FLAGS) -lm image.c -o out/image `pkg-config --cflags --libs gtk+-3.0`