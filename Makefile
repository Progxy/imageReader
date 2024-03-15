FLAGS = -std=c11 -Wall -Wextra -lm

# Compile the decoder in debug mode (every information needed is printed) (the debug mode can be also enabled by defining DEBUG_MODE in the code implementation)
debug:
	gcc $(FLAGS) -D"_DEBUG_MODE_" -g image.c -o out/image `pkg-config --cflags --libs gtk+-3.0`

# Compile the decoder in normal mode (only the warnings and the errors are shown)
image: image.c 
	gcc $(FLAGS) image.c -o out/image `pkg-config --cflags --libs gtk+-3.0`

# Compile as a shared library to use with the definitions contained in idl.h
idl: 
	gcc $(FLAGS) -fPIC ./include/image_io.h -shared -o out/idl.so