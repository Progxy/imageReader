#ifndef _DEBUG_PRINT_H_
#define _DEBUG_PRINT_H_

#include <stdio.h>
#include <stdarg.h>
#include "./types.h"

void error_print(const char* format, ...) {
    SET_COLOR(RED);
    printf("ERROR: ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    RESET_COLOR();
    return;
}

#ifdef _DEBUG_MODE_ 

static const char hex_values[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

void print_hex(Colors color, unsigned char val) {\
    SET_COLOR(color);
    printf("%c%c ", hex_values[(val >> 4) & 15], hex_values[val & 15]);
    RESET_COLOR();
    return;
}

void print_bits(Colors color, unsigned long int val, unsigned char bits) {
    SET_COLOR(color);
    printf(" 0B");

    for (int i = bits - 1; i >= 0; --i) {
        printf("%d%s", ((val >> i) & 1) != 0, i % 8 == 0 ? " " : "");
    }

    RESET_COLOR();
    return;
}

void print_line(unsigned char* ptr, int start, unsigned int line_len) {
    if (start < 0) {
        return;
    }
    
    SET_COLOR(YELLOW);
    printf("Byte (%d...%d): ", start + 1, start + line_len + 1);
    
    for (unsigned int i = 0; i <= line_len; ++i) {
        print_hex(i == line_len ? GREEN : YELLOW, *(ptr + i + start));
    }

    printf("\n");
    RESET_COLOR();
    
    return;
}

void print_table(Colors color, unsigned char* table) {
    for (unsigned int row = 0; row < 8; ++row) {
        for (unsigned int col = 0; col < 8; ++col) {
            print_hex(color, table[row * 8 + col]);
            printf(" ");
        }

        printf("\n");
    }

    printf("\n");

    return;
}

void debug_print(Colors color, const char* format, ...) {
    SET_COLOR(color);
    if (format[1] == '\0') {
        printf("%s", format);
        return;
    }
    
    printf("DEBUG: ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    RESET_COLOR();
    return;
}

void warning_print(const char* format, ...) {
    SET_COLOR(YELLOW);
    printf("WARNING: ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    RESET_COLOR();
    return;
}

#else

void print_hex(Colors /*color*/, unsigned char /*val*/) {
    return;
}
void print_bits(Colors /*color*/, unsigned long int /*val*/, unsigned char /*bits*/) {
    return;
}
void print_line(unsigned char* /*ptr*/, int /*start*/, unsigned int /*line_len*/) {
    return;
}
void print_table(Colors /*color*/, unsigned char* /*table*/) {
    return;
}

void debug_print(Colors /*color*/, const char* /*format*/, ...) {
    return;
}

void warning_print(const char* /*format*/, ...) {
    return;
}

#endif //_DEBUG_MODE_

#endif //_DEBUG_PRINT_H_