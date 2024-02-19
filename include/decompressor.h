#ifndef _DECOMPRESSOR_H_
#define _DECOMPRESSOR_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"

unsigned char* deflate(BitStream* bit_stream, unsigned char* err, unsigned int* stream_length);

static void update_adler_crc(unsigned char value, unsigned int* adler_register) {
    const unsigned int PRIME = 65521L;
    unsigned int low = (*adler_register) & 0X0000FFFFL;
    unsigned int high = ((*adler_register) >> 16) & 0X0000FFFFL;
    low = (low + value) % PRIME;
    high = (low + high) % PRIME;
    *adler_register = (high << 16) | low ;
    return;
}

unsigned char* deflate(BitStream* bit_stream, unsigned char* err, unsigned int* stream_length) {
    unsigned int adler_register = 1;
    unsigned char sliding_window[32768];
    unsigned char* decompressed_data = (unsigned char*) calloc(1, sizeof(unsigned char));
    unsigned int decompressed_index = 0;
    *stream_length = 1;

    unsigned char final = 0;
    // --- debug purpose
    unsigned char counter = 0;

    while (!final && counter < 1) {
        // ---- debug purpose
        counter++;
        
        final = get_next_bit(bit_stream, TRUE);
        unsigned char type = get_next_n_bits(bit_stream, 2, TRUE);

        debug_print(YELLOW, "final: %u\n", final);
        debug_print(YELLOW, "type: %u\n", type);

        if (type == 0) {
            update_adler_crc(bit_stream -> current_byte, &adler_register);
            unsigned short int length = get_next_bytes_us(bit_stream);
            update_adler_crc((length & 0xF0) >> 8, &adler_register);
            update_adler_crc(length & 0x0F, &adler_register);
            unsigned short int length_c = get_next_bytes_us(bit_stream);
            update_adler_crc((length_c & 0xF0) >> 8, &adler_register);
            update_adler_crc(length_c & 0x0F, &adler_register);

            unsigned short int check = ((length ^ length_c) + 1) & 0xFFFF;
            debug_print(YELLOW, "block length: %u, check: %u\n", length, check);

            if (check) {
                *err = 1;
                free(decompressed_data);
                return ((unsigned char*) "corrupted compressed block\n");
            }

            unsigned char* next_bytes = get_next_n_byte_uc(bit_stream, length);
            decompressed_data = (unsigned char*) realloc(decompressed_data, sizeof(unsigned char) * (*stream_length + length));
            for (unsigned int i = 0; i < length; ++i, ++decompressed_index) {
                decompressed_data[decompressed_index] = next_bytes[i];
                update_adler_crc(next_bytes[i], &adler_register);
            }

            free(next_bytes);
            continue;
        } else if (type == 3) {
            *err = 1;
            free(decompressed_data);
            return ((unsigned char*) "invalid compression type\n");
        }
        
        // Decode Huffman Table
        if (type == 2) {
            continue;
        }

        // Decode compressed data, remember to keep adding elements to the sliding window
        while (TRUE) {

        }
    }

    // Read the adler_crc
    unsigned int adler_crc = get_next_bytes_ui(bit_stream);
    
    if (adler_crc != adler_register) {
        *err = 1;
        free(decompressed_data);
        return ((unsigned char*) "corrupted compressed data blocks");
    }

    return decompressed_data;
}

#endif //_DECOMPRESSOR_H_