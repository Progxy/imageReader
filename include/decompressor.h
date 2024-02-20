#ifndef _DECOMPRESSOR_H_
#define _DECOMPRESSOR_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"

#define SLIDING_WINDOW_SIZE 0x8000
#define SLIDING_WINDOW_MASK 0x7FF

unsigned char* deflate(BitStream* bit_stream, unsigned char* err, unsigned int* decompressed_data_length);

static unsigned char max_value(unsigned char* vec, unsigned char len) {
    unsigned char max = 0;
    for (unsigned char i = 0; i < len; ++i) {
        max = (max < vec[i]) ? vec[i] : max;
    }
    return max;
}

unsigned char* generate_counts(unsigned char* lengths, unsigned char len, unsigned char max_bit_length) {
    unsigned char* bl_count = (unsigned char*) calloc(max_bit_length + 1, sizeof(unsigned char));
    for (unsigned char i = 0; i < len; ++i) {
        bl_count[lengths[i]]++;
    }
    return bl_count;
}

unsigned char* generate_hf(unsigned char* lengths, unsigned char len) {
    unsigned short int code = 0;
    unsigned char max_bit_length = max_value(lengths, len);
    unsigned char* bl_count = generate_counts(lengths, len, max_bit_length);
    
    bl_count[0] = 0;    
    unsigned short int* next_code = (unsigned short int*) calloc(max_bit_length + 1, sizeof(unsigned short int));
    for (unsigned char bits = 1; bits <= max_bit_length; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    free(bl_count);
    
    unsigned char* codes = (unsigned char*) calloc(len, sizeof(unsigned char));
    for (unsigned char i = 0; i < len; ++i) {
        codes[i] = next_code[lengths[i]]++;
    }
    free(next_code);

    return codes;
}

unsigned char* decode_lengths(unsigned int* hf_tree) {
    unsigned char* lengths;
    return lengths;
}

// Procedure ReadLengths (LENGTHCOUNT, LENGTHS [])
// Begin
// INDEX = 0
// While INDEX < LENGTHCOUNT DO
// Begin
// CODE = HuffmanDecodeInputStream ()
// If CODE < 16 Then
// Begin
// LENGTHS [INDEX] = CODE
// INDEX = INDEX + 1
// End
// Else If CODE = 16 Then
// Begin
// COUNT = 3 + ReadRawBitsFromInputStream (3)
// For I = 1 To COUNT Do
// Begin
// LENGTHS [INDEX] = LENGTHS [INDEX - 1]
// INDEX = INDEX + 1
// End
// End
// Else If CODE = 17 Then
// Begin
// COUNT = 3 + ReadRawBitsFromInputStream (3)
// For I = 1 To COUNT Do
// Begin
// LENGTHS [INDEX] = 0
// INDEX = INDEX + 1
// End
// End
// Else If CODE = 18 Then
// Begin
// COUNT = 11 + ReadRawBitsFromInputStream (7)
// For I = 1 To COUNT Do
// Begin
// LENGTHS [INDEX] = 0
// INDEX = INDEX + 1
// End
// End
// End
// End



static void copy_data(unsigned char* sliding_window, unsigned short int* sliding_window_index, unsigned char** dest, unsigned int* index, unsigned short int length, unsigned short int distance) {
    *dest = (unsigned char*) realloc(*dest, sizeof(unsigned char) * ((*index) + length));
    unsigned short int copy_pos = (SLIDING_WINDOW_SIZE + *sliding_window_index - distance);
    copy_pos = copy_pos & (SLIDING_WINDOW_MASK);

    // Copy from the sliding window
    for (unsigned short int i = 0; i < length; ++i, ++(*index)) {
        sliding_window[*sliding_window_index] = sliding_window[copy_pos];
        (*dest)[*index] = (sliding_window[*sliding_window_index]);

        // Advance to the next output position
        *sliding_window_index = *sliding_window_index + 1;
        *sliding_window_index = *sliding_window_index & (SLIDING_WINDOW_MASK);

        // Advance to the next byte to copy
        copy_pos = copy_pos + 1;
        copy_pos = copy_pos & (SLIDING_WINDOW_MASK);
    }
    
    return;
}

static unsigned short int decode_hf_value(BitStream* bit_stream, unsigned short int code, unsigned short int* mins, unsigned short int* maxs, unsigned short int* val_ptr, unsigned char length, bool is_fixed) {
    for (unsigned char i = 0; i < length; ++i) {
        if (code <= maxs[i]) {
            return (code - mins[i] + val_ptr[i]);
        }

        if ((!is_fixed) || (i != 1)) {
            code = (code << 1) + get_next_bit(bit_stream, TRUE);
        }
    }
    
    return 0xFFFF;
}

static unsigned short int get_length(BitStream* bit_stream, unsigned short int value) {
    const unsigned short int base_values[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    const unsigned char extra_bits[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    
    unsigned short int length = base_values[value - 257];
    unsigned char extra = get_next_n_bits(bit_stream, extra_bits[value - 257], TRUE);

    return (length + extra);
}

static unsigned short int get_distance(BitStream* bit_stream, unsigned short int value) {
    const unsigned short int base_values[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    const unsigned char extra_bits[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13}; 
    
    unsigned short int distance = base_values[value];
    unsigned char extra = get_next_n_bits(bit_stream, extra_bits[value], TRUE);

    return (distance + extra);
}

static void update_adler_crc(unsigned char value, unsigned int* adler_register) {
    const unsigned int PRIME = 65521L;
    unsigned int low = (*adler_register) & 0X0000FFFFL;
    unsigned int high = ((*adler_register) >> 16) & 0X0000FFFFL;
    low = (low + value) % PRIME;
    high = (low + high) % PRIME;
    *adler_register = (high << 16) | low ;
    return;
}

unsigned char* deflate(BitStream* bit_stream, unsigned char* err, unsigned int* decompressed_data_length) {
    unsigned int adler_register = 1;
    unsigned char* sliding_window = (unsigned char*) calloc(SLIDING_WINDOW_SIZE, sizeof(unsigned char));
    unsigned short int sliding_window_size = 0;

    // Fixed huffman literal/lengths codes
    const unsigned short int fixed_val_ptr[] = {256, 0, 280, 144};
    const unsigned short int fixed_mins[] = {0x00, 0x30, 0xC0, 0x190};
    const unsigned short int fixed_maxs[] = {0x17, 0xBF, 0xC7, 0x1FF};    
    const unsigned short int fixed_distance_val_ptr[] = {0x00};
    const unsigned short int fixed_distance_mins[] = {0x00};
    const unsigned short int fixed_distance_maxs[] = {0x1F};
    
    // Initialize decompressed data
    unsigned char* decompressed_data = (unsigned char*) calloc(1, sizeof(unsigned char));
    *decompressed_data_length = 0;

    // Decode the compressed data
    unsigned char zlib_compress_data = get_next_byte_uc(bit_stream);
    unsigned char zlib_flags = get_next_byte_uc(bit_stream);

    unsigned char compression_method = zlib_compress_data & 0x0F;
    debug_print(YELLOW, "compression_method: %u\n", compression_method);

    if (compression_method != 8) {
        *err = 1;
        free(decompressed_data);
        free(sliding_window);
        return ((unsigned char*) "invalid compression method");
    }

    unsigned char window_size = (zlib_compress_data & 0xF0) >> 4;
    debug_print(YELLOW, "window_size: %u\n", window_size);

    if (window_size > 7) {
        *err = 1;
        free(decompressed_data);
        free(sliding_window);
        return ((unsigned char*) "invalid compression method");
    }
    
    unsigned char check_bits = zlib_flags & 0x1F;
    debug_print(YELLOW, "check_bits: %u\n", check_bits);
    
    unsigned char preset_dictionary = ((zlib_flags & 0x20) >> 5) & 0x01;
    debug_print(YELLOW, "preset_dictionary: %u\n", preset_dictionary);

    if (preset_dictionary) {
        *err = 1;
        free(decompressed_data);
        free(sliding_window);
        return ((unsigned char*) "invalid compression method");
    }
    
    unsigned char compression_level = ((zlib_flags & 0xC0) >> 6) & 0x03;
    debug_print(YELLOW, "compression_level: %u\n", compression_level);

    unsigned char final = 0;
    // --- debug purpose
    unsigned char counter = 0;

    while (!final && counter < 2) {
        // ---- debug purpose
        counter++;
        
        final = get_next_bit(bit_stream, TRUE);
        unsigned char type = get_next_n_bits(bit_stream, 2, TRUE);
        debug_print(YELLOW, "\n");
        debug_print(YELLOW, "final: %u\n", final);
        debug_print(YELLOW, "type: %u\n", type);
        debug_print(WHITE, "\n");
        debug_print(WHITE, "START OF COMPRESSED BLOCK\n");

        if (type == 0) {
            unsigned short int length = get_next_bytes_us(bit_stream);
            unsigned short int length_c = get_next_bytes_us(bit_stream);
            unsigned short int check = ((length ^ length_c) + 1) & 0xFFFF;
            debug_print(YELLOW, "block length: %u, check: %u\n", length, check);

            if (check) {
                *err = 1;
                free(sliding_window);
                free(decompressed_data);
                return ((unsigned char*) "corrupted compressed block\n");
            }

            unsigned char* next_bytes = get_next_n_byte_uc(bit_stream, length);
            decompressed_data = (unsigned char*) realloc(decompressed_data, sizeof(unsigned char) * (*decompressed_data_length + length));
            
            for (unsigned int i = 0; i < length; ++i, ++(*decompressed_data_length)) {
                decompressed_data[*decompressed_data_length] = next_bytes[i];
            }
            
            free(next_bytes);

            continue;
        } else if (type == 3) {
            *err = 1;
            free(sliding_window);
            free(decompressed_data);
            return ((unsigned char*) "invalid compression type\n");
        }
        
        unsigned short int* val_ptr = NULL;
        unsigned short int* mins = NULL;
        unsigned short int* maxs = NULL;
        unsigned char bits_length = 0;        
        unsigned short int* distance_val_ptr = NULL;
        unsigned short int* distance_mins = NULL;
        unsigned short int* distance_maxs = NULL;
        unsigned char distance_bits_length = 0;

        // Select between the two huffman tables
        if (type == 2) {
            // Decode Dynamic Huffman Table
            unsigned short int literals_lengths = (((bit_stream -> current_byte & 0xF8) >> 3) & 0x1F) + 257;
            unsigned short int distance_lengths = get_next_n_bits(bit_stream, 5, FALSE) + 1;
            unsigned short int lengths = get_next_n_bits(bit_stream, 4, FALSE) + 4;
            debug_print(YELLOW, "literal_lengths: %u, distance_lengths: %u, lengths: %u\n", literals_lengths, distance_lengths, lengths);

            // Retrieve the length to build the huffman tree to decode the other two huffman trees (Literals and Distance)
            unsigned char order_of_code_lengths[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            unsigned char code_lengths[19] = {}; //maximum alphabet symbol is 18

            for (unsigned char i = 0; i < lengths; ++i) {
                code_lengths[order_of_code_lengths[i]] = get_next_n_bits(bit_stream, 3, TRUE);
            }

            // Build the huffman tree from the distances
            unsigned char* hf_of_hftrees = generate_hf(code_lengths, 19);

            // val_ptr = dynamic_val_ptr;
            // mins = dynamic_mins;
            // maxs = dynamic_maxs;
            continue;
        } else {
            val_ptr = (unsigned short int*) fixed_val_ptr;
            mins = (unsigned short int*) fixed_mins;
            maxs = (unsigned short int*) fixed_maxs;
            bits_length = 4;            
            distance_val_ptr = (unsigned short int*) fixed_distance_val_ptr;
            distance_mins = (unsigned short int*) fixed_distance_mins;
            distance_maxs = (unsigned short int*) fixed_distance_maxs;
            distance_bits_length = 1;
        }

        // Decode compressed data, remember to keep adding elements to the sliding window
        unsigned short int code = 0;
        while (TRUE) {
            code = get_next_n_bits(bit_stream, type == 1 ? 7 : 1, TRUE);
            unsigned short int decoded_value = decode_hf_value(bit_stream, code, mins, maxs, val_ptr, bits_length, type == 1);
            debug_print(YELLOW, "decoded_value: %u\n", decoded_value);
            
            if (decoded_value < 256) {
                decompressed_data = (unsigned char*) realloc(decompressed_data, sizeof(unsigned char) * ((*decompressed_data_length) + 1));
                decompressed_data[*decompressed_data_length] = decoded_value;
                (*decompressed_data_length)++;
            } else if (decoded_value == 256) {
                debug_print(WHITE, "END OF COMPRESSED BLOCK\n");
                break;
            } else {
                unsigned short int length = get_length(bit_stream, decoded_value);
                unsigned short int distance_code = get_next_n_bits(bit_stream, type == 1 ? 5 : 1, TRUE);
                unsigned short int decoded_distance = decode_hf_value(bit_stream, distance_code, distance_mins, distance_maxs, distance_val_ptr, distance_bits_length, type == 1);
                unsigned short int distance = get_distance(bit_stream, decoded_distance);
                copy_data(sliding_window, &sliding_window_size, &decompressed_data, decompressed_data_length, length, distance);
            }
        }
    }

    // Read the adler_crc
    unsigned int adler_crc = get_next_bytes_ui(bit_stream);

    // Calculate the crc of the blocks
    for (unsigned int i = 0; i < *decompressed_data_length; ++i) {
        update_adler_crc(decompressed_data[i], &adler_register);
    } 

    if (adler_crc != adler_register) {
        *err = 1;
        free(sliding_window);
        free(decompressed_data);
        return ((unsigned char*) "corrupted compressed data blocks");
    }

    return decompressed_data;
}

#endif //_DECOMPRESSOR_H_