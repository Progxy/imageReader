#ifndef _BIT_STREAM_H_
#define _BIT_STREAM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./debug_print.h"
#include "./types.h"

BitStream* allocate_bit_stream(unsigned char* data_stream, unsigned int size, bool copy_flag) {
    BitStream* bit_stream = (BitStream*) calloc(1, sizeof(BitStream));
    if (copy_flag) {
        unsigned char* new_data_stream = (unsigned char*) calloc(size, sizeof(unsigned char));
        for (unsigned int i = 0; i < size; ++i) {
            new_data_stream[i] = data_stream[i];
        }
        bit_stream -> stream = new_data_stream;
    } else bit_stream -> stream = data_stream;
    bit_stream -> bit = 0;
    bit_stream -> byte = 0;
    bit_stream -> size = size;
    bit_stream -> error = NO_ERROR;
    bit_stream -> current_byte = 0;

    return bit_stream;
}

void deallocate_bit_stream(BitStream* bit_stream) {
    debug_print(BLUE, "deallocating bitstream...\n");
    free(bit_stream -> stream);
    free(bit_stream);
    return;
}

void get_next_byte(BitStream* bit_stream) {
    if (bit_stream -> byte >= bit_stream -> size) {
        error_print("exceed bitstream length: %d, with: %d\n", bit_stream -> size, bit_stream -> byte);
        bit_stream -> error = EXCEEDED_LENGTH;
        bit_stream -> current_byte = 0;
        bit_stream -> bit = 0;
        return;
    }

    bit_stream -> current_byte = (bit_stream -> stream)[bit_stream -> byte];
    (bit_stream -> byte)++;
    bit_stream -> bit = 0;
    return;
}

unsigned char get_next_byte_uc(BitStream* bit_stream) {
    get_next_byte(bit_stream);
    return bit_stream -> current_byte;
}

void* get_next_n_byte(BitStream* bit_stream, unsigned int n, unsigned char size) {
    void* data = calloc(n, size);
    unsigned char* data_ptr = (unsigned char*) data;

    for (unsigned int i = 0; i < (n * size); ++i) {
        get_next_byte(bit_stream);
        data_ptr[i] = bit_stream -> current_byte;
        if (bit_stream -> error) return data;
    }

    return data;
}

unsigned char* get_next_n_byte_uc(BitStream* bit_stream, unsigned int n_byte) {
    return ((unsigned char*) get_next_n_byte(bit_stream, n_byte, sizeof(unsigned char)));
}

unsigned char get_next_bit(BitStream* bit_stream, unsigned char reverse_flag) {
    if (!reverse_flag) {
        if (bit_stream -> bit == 0) get_next_byte(bit_stream);
        unsigned char bit_value = (bit_stream -> current_byte >> (7 - bit_stream -> bit)) & 1;
        (bit_stream -> bit) = ((bit_stream -> bit) + 1) & 7;
        return bit_value;
    }

    if (bit_stream -> bit <= 0) {
        get_next_byte(bit_stream);
        bit_stream -> bit = 8;
    }

    unsigned char bit_value = (bit_stream -> current_byte) & 1;
    bit_stream -> current_byte >>= 1;
    (bit_stream -> bit)--;

    return bit_value;
}

void skip_n_bits(BitStream* bit_stream, unsigned int bits) {
    (bit_stream -> bit) += bits;
    (bit_stream -> byte) += (bit_stream -> bit - (bit_stream -> bit % 8)) / 8;
    (bit_stream -> bit) %= 8;
    return;
}

void set_byte(BitStream* bit_stream, unsigned int byte) {
    if (byte >= bit_stream -> size) {
        error_print("Set byte to %u while the length of the BitStream is %u\n", byte, bit_stream -> size);
        return;
    }

    bit_stream -> byte = byte;

    return;
}

unsigned int get_next_bytes_ui(BitStream* bit_stream) {
    unsigned int bytes = 0;
    for (unsigned char j = 0; j < 4; ++j) {
        bytes |= get_next_byte_uc(bit_stream) << ((3 - j) * 8);
    }
    return bytes;
}

unsigned short int get_next_bytes_us(BitStream* bit_stream) {
    unsigned short int bytes = get_next_byte_uc(bit_stream);
    bytes = (bytes << 8) + get_next_byte_uc(bit_stream);
    return bytes;
}

unsigned short int get_next_n_bits(BitStream* bit_stream, unsigned char n_bits, unsigned char reverse_flag) {
    unsigned short int bits = 0;

    for (unsigned char i = 0; i < n_bits; ++i) {
        if (reverse_flag) bits += get_next_bit(bit_stream, reverse_flag) << i;
        else bits = (bits << 1) + get_next_bit(bit_stream, reverse_flag);
        if (bit_stream -> error) return bits;
    }

    return bits;
}

static bool is_contained(unsigned char val, unsigned char* container, unsigned int container_len) {
    for (unsigned int i = 0; i < container_len; ++i) {
        if (container[i] == val) {
            return TRUE;
        }
    }
    return FALSE;
}

bool contains(unsigned char* values, unsigned int values_len, unsigned char* container, unsigned int container_len) {
    for (unsigned int i = 0; i < values_len; ++i) {
        if (is_contained(values[i], container, container_len)) {
            return TRUE;
        }
    }
    return FALSE;
}

unsigned int get_symbol_pos(char* str, unsigned char symbol, unsigned int pos) {
    while ((str[pos] != symbol) && (pos < strlen(str))) {
        pos++;
    }
    return pos;
}

char* get_str(BitStream* bit_stream, unsigned char* str_terminators, unsigned int terminators_num) {
    char* str = (char*) calloc(1, sizeof(char));
    unsigned int index = 0;
    char str_data = get_next_byte_uc(bit_stream);

    while (!is_contained(str_data, str_terminators, terminators_num)) {
        str = (char*) realloc(str, (index + 1) * sizeof(char));
        str[index] = str_data;
        index++;
        str_data = get_next_byte_uc(bit_stream);
    }

    return str;
}

void append_n_bytes(BitStream* bit_stream, unsigned char* data, unsigned int length) {
    unsigned int old_size = bit_stream -> size;
    (bit_stream -> size) += length;
    bit_stream -> stream = (unsigned char*) realloc(bit_stream -> stream, sizeof(unsigned char) * (bit_stream -> size));

    for (unsigned int i = 0; i < length; ++i) {
        (bit_stream -> stream)[old_size + i] = data[i];
    }

    free(data);

    debug_print(WHITE, "successfully appended %u bytes, new size: %u\n", length, bit_stream -> size);
    return;
}

void read_until(BitStream* bit_stream, char* symbols, char** data) {
    unsigned int size = 0;
    get_next_byte(bit_stream); // Start the reading

    while (!is_contained(bit_stream -> current_byte, (unsigned char*) symbols, strlen(symbols)) && (bit_stream -> byte < bit_stream -> size)) {
        if (data != NULL) {
            (*data)[size] = bit_stream -> current_byte;
            size++;
        }

        get_next_byte(bit_stream);
    }

    if (data != NULL) {
        (*data)[size] = '\0';
        size++;
        (*data) = (char*) realloc(*data, size * sizeof(char));
    }

    return;
}

void skip_back(BitStream* bit_stream, unsigned int n) {
    bit_stream -> byte -= n;
    bit_stream -> current_byte = (bit_stream -> stream)[bit_stream -> byte];
}

#endif //_BIT_STREAM_H_
