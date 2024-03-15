#ifndef _DECOMPRESSOR_H_
#define _DECOMPRESSOR_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"
#include "./bitstream.h"

#define SLIDING_WINDOW_SIZE 0x8000
#define SLIDING_WINDOW_MASK 0x7FFF

// Fixed huffman literal/lengths codes
const unsigned short int fixed_val_ptr[] = {256, 0, 280, 144};
const unsigned short int fixed_mins[] = {0x00, 0x30, 0xC0, 0x190};
const unsigned short int fixed_maxs[] = {0x17, 0xBF, 0xC7, 0x1FF};    
const unsigned short int fixed_distance_val_ptr[] = {0x00};
const unsigned short int fixed_distance_mins[] = {0x00};
const unsigned short int fixed_distance_maxs[] = {0x1F};

/* ---------------------------------------------------------------------------------------------------------- */

static void deallocate_dynamic_hf(DynamicHF* hf);
static unsigned char max_value(unsigned char* vec, unsigned short int len);
static void generate_codes(DynamicHF* hf);
static unsigned short int decode_hf_fixed(BitStream* bit_stream, unsigned short int code, const unsigned short int* mins, const unsigned short int* maxs, const unsigned short int* val_ptr, unsigned char bit_length);
static unsigned short int decode_hf(BitStream* bit_stream, unsigned short int code, DynamicHF hf);
static void decode_lengths(BitStream* bit_stream, DynamicHF decoder_hf, DynamicHF* literals_hf, DynamicHF* distance_hf);
static void copy_data(SlidingWindow* sliding_window, unsigned char** dest, unsigned int* index, unsigned short int length, unsigned short int distance);
static unsigned short int get_length(BitStream* bit_stream, unsigned short int value);
static unsigned short int get_distance(BitStream* bit_stream, unsigned short int value);
static void update_adler_crc(unsigned char value, unsigned int* adler_register);
static char* read_zlib_header(BitStream* bit_stream);
static unsigned char read_uncompressed_data(BitStream* bit_stream, unsigned char** decompressed_data, unsigned int* decompressed_data_length);
static void decode_dynamic_huffman_tables(BitStream* bit_stream, DynamicHF* literals_hf, DynamicHF* distance_hf);
unsigned char* inflate(BitStream* bit_stream, unsigned char* err, unsigned int* decompressed_data_length);

/* ---------------------------------------------------------------------------------------------------------- */

static void deallocate_dynamic_hf(DynamicHF* hf) {
    debug_print(CYAN, "deallocating dynamic hf...\n");
    for (unsigned char i = 1; i <= hf -> bit_length; ++i) {
        free((hf -> values)[i]);
    }
    free(hf -> values);
    free(hf -> min_codes);
    free(hf -> max_codes);
    free(hf -> lengths);
    hf -> bit_length = 0;
    hf -> size = 0;
    return;
}

static unsigned char max_value(unsigned char* vec, unsigned short int len) {
    unsigned char max = 0;
    for (unsigned short int i = 0; i < len; ++i) {
        max = (max < vec[i]) ? vec[i] : max;
    }
    return max;
}

static void generate_codes(DynamicHF* hf) {
    hf -> bit_length = max_value(hf -> lengths, hf -> size);
    unsigned char* bl_count = (unsigned char*) calloc((hf -> bit_length) + 1, sizeof(unsigned char));
    for (unsigned short int i = 0; i < hf -> size; ++i) {
        bl_count[(hf -> lengths)[i]]++;
    }

    bl_count[0] = 0;

    hf -> values = (unsigned short int**) calloc((hf -> bit_length) + 1, sizeof(unsigned short int*));
    for (unsigned char i = 1; i <= hf -> bit_length; ++i) {
        (hf -> values)[i] = (unsigned short int*) calloc(bl_count[i], sizeof(unsigned short int));
    }

    hf -> min_codes = (unsigned short int*) calloc((hf -> bit_length) + 1, sizeof(unsigned short int));
    hf -> max_codes = (unsigned short int*) calloc((hf -> bit_length) + 1, sizeof(unsigned short int));

    for (unsigned char bits = 1; bits <= hf -> bit_length; ++bits) {
        (hf -> max_codes)[bits] = ((hf -> max_codes)[bits - 1] + bl_count[bits - 1]) << 1;
        (hf -> min_codes)[bits] = (hf -> max_codes)[bits];
    }
    
    free(bl_count);
    
    unsigned char* values_index = (unsigned char*) calloc((hf -> bit_length) + 1, sizeof(unsigned char));
    for (unsigned short int i = 0; i < hf -> size; ++i) {
        if ((hf -> lengths)[i] != 0) {
            unsigned char value_bit_len = (hf -> lengths)[i];
            ((hf -> max_codes)[value_bit_len])++;
            (hf -> values)[value_bit_len][values_index[value_bit_len]++] = i;
        }
    }

    free(values_index);

    return;
}

static unsigned short int decode_hf_fixed(BitStream* bit_stream, unsigned short int code, const unsigned short int* mins, const unsigned short int* maxs, const unsigned short int* val_ptr, unsigned char bit_length) {
    for (unsigned char i = 0; i < (bit_length == 4 ? 6 : 4); ++i) {
        code = (code << 1) + get_next_bit(bit_stream, TRUE);
    }

    for (unsigned char i = 0; i < bit_length; ++i) {
        if (code <= maxs[i]) {
            return (code - mins[i] + val_ptr[i]);
        }

        if ((i != 1)) {
            code = (code << 1) + get_next_bit(bit_stream, TRUE);
        }
    }
    
    return 0xFFFF;
}

static unsigned short int decode_hf(BitStream* bit_stream, unsigned short int code, DynamicHF hf) {
    for (unsigned char i = 1; i <= hf.bit_length; ++i) {
        if (hf.max_codes[i] > code) {
            return hf.values[i][code - hf.min_codes[i]];
        }
        code = (code << 1) + get_next_bit(bit_stream, TRUE);

        if (bit_stream -> error) {
            break;
        }
    }
    
    debug_print(RED, "\n");
    debug_print(RED, "probably invalid huffman tree, bit_length: %u, size: %u: \n", hf.bit_length, hf.size);

    return 0xFFFF;
}

static void decode_lengths(BitStream* bit_stream, DynamicHF decoder_hf, DynamicHF* literals_hf, DynamicHF* distance_hf) {
    literals_hf -> lengths = (unsigned char*) calloc(286, sizeof(unsigned char));
    distance_hf -> lengths = (unsigned char*) calloc(30, sizeof(unsigned char));
    unsigned short int index = 0;

    while (index < (literals_hf -> size + distance_hf -> size)) {
        unsigned short int code = get_next_bit(bit_stream, TRUE);
        unsigned short int value = decode_hf(bit_stream, code, decoder_hf);

        if (value < 16) {
            if (index < literals_hf -> size) (literals_hf -> lengths)[index] = value;
            else (distance_hf -> lengths)[index - literals_hf -> size] = value;
            index++;
        } else if (value == 16) {
            if (!index) {
                warning_print("shouldn't repeat elements with index 0\n");
            }
            unsigned char count = 3 + get_next_n_bits(bit_stream, 2, TRUE);
            unsigned char value = (index < literals_hf -> size) ? (literals_hf -> lengths)[index - 1] : (distance_hf -> lengths)[index - literals_hf -> size - 1];
            for (unsigned char i = 0; i < count; ++i, ++index) {
                if (index < literals_hf -> size) (literals_hf -> lengths)[index] = value;
                else (distance_hf -> lengths)[index - literals_hf -> size] = value;
            }
        } else if (value == 17) {
            unsigned char count = 3 + get_next_n_bits(bit_stream, 3, TRUE);
            for (unsigned char i = 0; i < count; ++i, ++index) {
                if (index < literals_hf -> size) (literals_hf -> lengths)[index] = 0;
                else (distance_hf -> lengths)[index - literals_hf -> size] = 0;
            }
        } else if (value == 18) {
            unsigned char count = 11 + get_next_n_bits(bit_stream, 7, TRUE);
            for (unsigned char i = 0; i < count; ++i, ++index) {
                if (index < literals_hf -> size) (literals_hf -> lengths)[index] = 0;
                else (distance_hf -> lengths)[index - literals_hf -> size] = 0;
            }
        } else {
            warning_print("invalid value: %u\n", value);
        }
    }

    return;
}

static void copy_data(SlidingWindow* sliding_window, unsigned char** dest, unsigned int* index, unsigned short int length, unsigned short int distance) {
    *dest = (unsigned char*) realloc(*dest, sizeof(unsigned char) * ((*index) + length));
    unsigned int cur_pos = (SLIDING_WINDOW_SIZE + (sliding_window -> out_pos) - distance);
    cur_pos = (cur_pos) & (SLIDING_WINDOW_MASK);

    // Copy from the sliding window
    for (unsigned short int i = 0; i < length; ++i, ++(*index)) {
        (*dest)[*index] = ((sliding_window -> window)[cur_pos]);
        ((sliding_window -> window)[sliding_window -> out_pos]) = ((sliding_window -> window)[cur_pos]);
        
        // Advance to the next byte to copy
        cur_pos = ((cur_pos) + 1) & (SLIDING_WINDOW_MASK);
        sliding_window -> out_pos = ((sliding_window -> out_pos) + 1) & (SLIDING_WINDOW_MASK);
    }
    
    return;
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
    unsigned short int extra = get_next_n_bits(bit_stream, extra_bits[value], TRUE);

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

static char* read_zlib_header(BitStream* bit_stream) {
    unsigned char zlib_compress_data = get_next_byte_uc(bit_stream);
    unsigned char zlib_flags = get_next_byte_uc(bit_stream);
    unsigned char compression_method = zlib_compress_data & 0x0F;
    unsigned char window_size = (zlib_compress_data & 0xF0) >> 4;
    unsigned char preset_dictionary = ((zlib_flags & 0x20) >> 5) & 0x01;
    unsigned char compression_level = ((zlib_flags & 0xC0) >> 6) & 0x03;

    debug_print(YELLOW, "compression method: %u\n", compression_method);
    debug_print(YELLOW, "window size: %u\n", window_size);
    debug_print(YELLOW, "preset dictionary: %u\n", preset_dictionary);
    debug_print(YELLOW, "compression level: %u\n", compression_level);
    
    if (compression_method != 8) {
        return ("invalid compression method");
    } else if (window_size > 7) {
        return ("invalid window size");
    } else if (preset_dictionary) {
        return ("invalid preset dictionary");
    } else if ((zlib_compress_data * 256 + zlib_flags) % 31 != 0) {
        return ("CMF+FLG checksum failed");
    }
    
    return NULL;
}

static unsigned char read_uncompressed_data(BitStream* bit_stream, unsigned char** decompressed_data, unsigned int* decompressed_data_length) {
    unsigned short int length = get_next_bytes_us(bit_stream);
    unsigned short int length_c = get_next_bytes_us(bit_stream);
    unsigned short int check = ((length ^ length_c) + 1) & 0xFFFF;
    debug_print(YELLOW, "block length: %u, check: %u\n", length, check);

    if (check) {
        return TRUE;
    }

    unsigned char* next_bytes = get_next_n_byte_uc(bit_stream, length);
    *decompressed_data = (unsigned char*) realloc(*decompressed_data, sizeof(unsigned char) * (*decompressed_data_length + length));
    
    for (unsigned int i = 0; i < length; ++i, ++(*decompressed_data_length)) {
        (*decompressed_data)[*decompressed_data_length] = next_bytes[i];
    }
    
    free(next_bytes);
    return FALSE;
}

static void decode_dynamic_huffman_tables(BitStream* bit_stream, DynamicHF* literals_hf, DynamicHF* distance_hf) {
    DynamicHF decoder_hf = (DynamicHF) {};
    literals_hf -> size = get_next_n_bits(bit_stream, 5, TRUE) + 257;
    distance_hf -> size = get_next_n_bits(bit_stream, 5, TRUE) + 1;
    decoder_hf.size = get_next_n_bits(bit_stream, 4, TRUE) + 4;
    debug_print(YELLOW, "literal_lengths: %u, distance_lengths: %u, lengths: %u\n", literals_hf -> size, distance_hf -> size, decoder_hf.size);

    // Retrieve the length to build the huffman tree to decode the other two huffman trees (Literals and Distance)
    const unsigned char order_of_code_lengths[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    decoder_hf.lengths = (unsigned char*) calloc(19, sizeof(unsigned char)); 

    for (unsigned char i = 0; i < decoder_hf.size; ++i) {
        (decoder_hf.lengths)[order_of_code_lengths[i]] = get_next_n_bits(bit_stream, 3, TRUE);
    }

    decoder_hf.size = 19; // The real size of the lengths is 19 as the amount alloced

    // Build the huffman tree from the distances
    generate_codes(&decoder_hf);

    decode_lengths(bit_stream, decoder_hf, literals_hf, distance_hf);
    deallocate_dynamic_hf(&decoder_hf);

    literals_hf -> size = 286;
    distance_hf -> size = 30;

    generate_codes(literals_hf);
    generate_codes(distance_hf);

    return;
}

unsigned char* inflate(BitStream* bit_stream, unsigned char* err, unsigned int* decompressed_data_length) {    
    // Initialize decompressed data
    SlidingWindow sliding_window = (SlidingWindow) {.out_pos = 0};
    sliding_window.window = (unsigned char*) calloc(SLIDING_WINDOW_SIZE, sizeof(unsigned char));
    unsigned char* decompressed_data = (unsigned char*) calloc(1, sizeof(unsigned char));
    *decompressed_data_length = 0;

    char* error = read_zlib_header(bit_stream);
    if (error != NULL) {
        *err = 1;
        free(decompressed_data);
        free(sliding_window.window);
        return (unsigned char*) error;
    }

    unsigned short int counter = 0;
    unsigned char final = 0;
    while (!final) {
        final = get_next_bit(bit_stream, TRUE);
        unsigned char type = get_next_n_bits(bit_stream, 2, TRUE);
        debug_print(YELLOW, "\n");
        debug_print(YELLOW, "final: %u, type: %u\n\n", final, type);
        debug_print(WHITE, "START OF COMPRESSED BLOCK [%u]\n", counter + 1);
        counter++;

        if (type == 0) {
            if ((*err = read_uncompressed_data(bit_stream, &decompressed_data, decompressed_data_length))) {
                free(sliding_window.window);
                free(decompressed_data);
                return ((unsigned char*) "corrupted compressed block\n");
            }
            continue;
        } else if (type == 3) {
            *err = 1;
            free(sliding_window.window);
            free(decompressed_data);
            return ((unsigned char*) "invalid compression type\n");
        }
        
        DynamicHF literals_hf = (DynamicHF) {};
        DynamicHF distance_hf = (DynamicHF) {};

        // Select between the two huffman tables
        if (type == 2) {
            decode_dynamic_huffman_tables(bit_stream, &literals_hf, &distance_hf);
        } else {
            literals_hf.bit_length = 4;            
            distance_hf.bit_length = 1;
        }

        // Decode compressed data, remember to keep adding elements to the sliding window
        unsigned short int code = 0;
        while (TRUE) {
            code = get_next_n_bits(bit_stream, 1, TRUE);
            unsigned short int decoded_value = 0;
            if (type == 1) decoded_value = decode_hf_fixed(bit_stream, code, fixed_mins, fixed_maxs, fixed_val_ptr, literals_hf.bit_length);
            else decoded_value = decode_hf(bit_stream, code, literals_hf);
            //debug_print(WHITE, "decoded_value: %u\n", decoded_value);
            
            if (decoded_value == 0xFFFF) {
                *err = 1;
                free(sliding_window.window);
                free(decompressed_data);
                return ((unsigned char*) "invalid decoded value\n");
            }
            
            if (decoded_value < 256) {
                decompressed_data = (unsigned char*) realloc(decompressed_data, sizeof(unsigned char) * ((*decompressed_data_length) + 1));
                decompressed_data[*decompressed_data_length] = decoded_value;
                (*decompressed_data_length)++;
                sliding_window.window[sliding_window.out_pos] = decoded_value;
                sliding_window.out_pos = (sliding_window.out_pos + 1) & SLIDING_WINDOW_MASK;
            } else if (decoded_value == 256) {
                debug_print(WHITE, "END OF COMPRESSED BLOCK\n");
                break;
            } else {
                unsigned short int length = get_length(bit_stream, decoded_value);
                unsigned short int distance_code = get_next_n_bits(bit_stream, 1, TRUE);
                unsigned short int decoded_distance = 0;
                if (type == 1) decoded_distance = decode_hf_fixed(bit_stream, distance_code, fixed_distance_mins, fixed_distance_maxs, fixed_distance_val_ptr, distance_hf.bit_length);
                else decoded_distance = decode_hf(bit_stream, distance_code, distance_hf);
                if (decoded_distance == 0xFFFF) {
                    *err = 1;
                    free(sliding_window.window);
                    free(decompressed_data);
                    return ((unsigned char*) "invalid decoded distance\n");
                }
                unsigned short int distance = get_distance(bit_stream, decoded_distance);
                copy_data(&sliding_window, &decompressed_data, decompressed_data_length, length, distance);
            }
        }

        if (type == 2) {
            deallocate_dynamic_hf(&literals_hf);
            deallocate_dynamic_hf(&distance_hf);
        }
    }

    // Read the adler_crc
    unsigned int adler_crc = get_next_bytes_ui(bit_stream);
    unsigned int adler_register = 1;

    // Calculate the crc of the blocks
    for (unsigned int i = 0; i < (*decompressed_data_length); ++i) {
        update_adler_crc(decompressed_data[i], &adler_register);
    } 

    free(sliding_window.window);

    if (adler_crc != adler_register) {
        *err = 1;
        free(decompressed_data);
        error_print("adler_register: 0x%x, adler_crc: 0x%x\n", adler_register, adler_crc);
        return ((unsigned char*) "corrupted compressed data blocks");
    }

    return decompressed_data;
}

#endif //_DECOMPRESSOR_H_