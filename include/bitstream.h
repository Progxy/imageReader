#ifndef _BIT_STREAM_H_
#define _BIT_STREAM_H_

#include <stdio.h>
#include <stdlib.h>
#include "./debug_print.h"
#include "./types.h"

BitStream* allocate_bit_stream(unsigned char* data_stream, unsigned int size) {
    BitStream* bit_stream = (BitStream*) calloc(1, sizeof(BitStream));

    bit_stream -> stream = data_stream;
    bit_stream -> bit = 0;
    bit_stream -> byte = 0;
    bit_stream -> size = size;
    bit_stream -> error = NO_ERROR;
    bit_stream -> current_byte = 0;

    return bit_stream;
}

void deallocate_bit_stream(BitStream* bit_stream) {
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
    }

    return data;
}

unsigned char* get_next_n_byte_uc(BitStream* bit_stream, unsigned int n_byte) {
    return ((unsigned char*) get_next_n_byte(bit_stream, n_byte, sizeof(unsigned char)));
}

unsigned char get_next_bit(BitStream* bit_stream, unsigned char reverse_flag) {
    if (bit_stream -> byte >= bit_stream -> size) {
        error_print("exceed bitstream length: %d, with: %d\n", bit_stream -> size, bit_stream -> byte);
        bit_stream -> error = EXCEEDED_LENGTH;
        return 0;
    }
    
    unsigned char bit_value = (((bit_stream -> stream)[bit_stream -> byte]) & (1 << (reverse_flag ? bit_stream -> bit : 7 - bit_stream -> bit))) != 0;

    // Update bit and byte position
    (bit_stream -> bit)++;
    (bit_stream -> byte) += (bit_stream -> bit) == 8;
    (bit_stream -> bit) %= 8;

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

unsigned char get_next_n_bits(BitStream* bit_stream, unsigned char n_bits, unsigned char reverse_flag) {
    unsigned char bits = 0;
    
    for (unsigned char i = 0; i < n_bits; ++i) {
        bits <<= 1;
        bits += get_next_bit(bit_stream, reverse_flag);
    }
    
    return bits;
}

#endif //_BIT_STREAM_H_