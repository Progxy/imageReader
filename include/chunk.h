#ifndef _CHUNK_H_
#define _CHUNK_H_

#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"
#include "./bitstream.h"

static void crc_byte(BitStream* bit_stream, unsigned int* crc_register, unsigned int* crc_table) {
    unsigned char data = get_next_byte_uc(bit_stream);
    unsigned int index = ((*crc_register) ^ data) & 0xFF;
    *crc_register = crc_table[index] ^ (((*crc_register) >> 8) & 0X00FFFFFF);
    return;
}

static unsigned int calculate_crc(BitStream* bit_stream, unsigned int length, unsigned int* crc_table) {
    unsigned int crc_register = 0xFFFFFFFFL;
    for (unsigned int ii = 0; ii < length ; ++ii) {
        crc_byte(bit_stream, &crc_register, crc_table); 
    }
    return (~crc_register);
}

static unsigned int* generate_crc_table() {
    unsigned int* crc_table = (unsigned int*) calloc(256, sizeof(unsigned int));
    for (unsigned int ii = 0; ii < 256; ++ii) {
        crc_table[ii] = ii;
        for (unsigned int jj = 0 ; jj < 8; ++jj) {
            if ((crc_table[ii] & 0x1) == 0) {
                crc_table[ii] >>= 1;
            } else {
                crc_table[ii] = 0xEDB88320L ^ (crc_table[ii] >> 1) ;
            }
        }
    }
    return crc_table;
}

Chunks find_and_check_chunks(unsigned char* file_data, unsigned int file_length) {
    Chunks chunks = (Chunks) {.chunks = NULL, .chunks_count = 0, .invalid_chunks = 0};
    chunks.chunks = (Chunk*) calloc(1, sizeof(Chunk));
    BitStream* bit_stream = allocate_bit_stream(file_data, file_length);
    unsigned int* crc_table = generate_crc_table();
    
    // Skip the PNG magic number
    set_byte(bit_stream, 8);

    while (bit_stream -> byte < bit_stream -> size) {
        Chunk chunk = (Chunk) {};
        chunk.length = get_next_bytes_ui(bit_stream);

        chunk.chunk_type[4] = '\0';
        for (unsigned char j = 0; j < 4; ++j) {
            chunk.chunk_type[j] = get_next_byte_uc(bit_stream);
        }

        chunk.pos = bit_stream -> byte;
        set_byte(bit_stream, bit_stream -> byte - 4);
        unsigned int chunk_crc = calculate_crc(bit_stream, chunk.length + 4, crc_table);
        unsigned int crc = get_next_bytes_ui(bit_stream);

        if (chunk_crc != crc) {
            warning_print("the current chunk may be corrupted!\n");
            debug_print(BLUE, "crc chunk value: ");
            print_bits(BLUE, chunk_crc, sizeof(unsigned int) * 8);
            debug_print(BLUE, "\n");
            debug_print(PURPLE, "crc value: ");
            print_bits(PURPLE, crc, sizeof(unsigned int) * 8);
            debug_print(PURPLE, "\n");
            continue;
        }

        chunks.chunks = (Chunk*) realloc(chunks.chunks, sizeof(Chunk) * (chunks.chunks_count + 1));
        chunks.chunks[chunks.chunks_count] = chunk;
        chunks.chunks_count++;
    }

    free(crc_table);
    free(bit_stream);
    
    return chunks;
}

#endif //_CHUNK_H_