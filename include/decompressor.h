#ifndef _DECOMPRESSOR_H_
#define _DECOMPRESSOR_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"

#define SLIDING_WINDOW_SIZE 0x800

unsigned char* deflate(BitStream* bit_stream, unsigned char* err, unsigned int* decompressed_data_length);

// unsigned char sh_get_maximum_bit_length(unsigned char *code_bit_lengths, unsigned int len_of_array) {
//     unsigned char max_bit_length = 0;
//     for(unsigned int i = 0; i < len_of_array; ++i) {
//         if(max_bit_length < code_bit_lengths[i]) {
//             max_bit_length = code_bit_lengths[i];
//         }
//     }

//     return max_bit_length;
// }

// void sh_first_code_for_bitlen(unsigned int *first_codes, unsigned int *code_count, unsigned int max_bit_length) {
//     unsigned int code = 0;
//     for(unsigned int i = 1; i <= max_bit_length; ++i) {
//         code = ( code + code_count[i-1]) << 1; 

//         if(code_count[i] > 0) {
//             first_codes[i] = code;
//         }
//     }
// }

// void sh_assign_huffman_code(unsigned int *assigned_codes, unsigned int *first_codes, unsigned char *code_bit_lengths, unsigned int len_assign_code) {
//     for(unsigned int i = 0; i < len_assign_code; ++i) {
//         if(code_bit_lengths[i]) {
//             assigned_codes[i] = first_codes[code_bit_lengths[i]]++;
//         }
//     }
// }

// void sh_get_bit_length_count(unsigned int *code_count, unsigned char *code_bit_length, unsigned int bit_len_array_len) {
//     for(unsigned int i = 0; i < bit_len_array_len; ++i) {
//         code_count[code_bit_length[i]]++;
//     }
// }

// unsigned int* sh_build_huffman_code(unsigned char *code_bit_lengths, unsigned int len_code_bit_lengths) {
//     unsigned int max_bit_length = sh_get_maximum_bit_length(code_bit_lengths, len_code_bit_lengths);

//     unsigned int *code_counts = (unsigned int *) calloc(( max_bit_length + 1 ), sizeof(unsigned int));
//     unsigned int *first_codes = (unsigned int *) calloc((max_bit_length + 1), sizeof(unsigned int));
//     we have to assign code to every element in the alphabet, even if we have to assign zero
//     unsigned int *assigned_codes = (unsigned int *) calloc((len_code_bit_lengths), sizeof(unsigned int));


//     sh_get_bit_length_count(code_counts,  code_bit_lengths, len_code_bit_lengths);
//     in the real world, when a code of the alphabet has zero bit length, it means it doesn't occur in the data thus we have to reset the count for the zero bit length codes to 0.
//     code_counts[0] = 0; 

//     sh_first_code_for_bitlen(first_codes, code_counts, max_bit_length);
//     sh_assign_huffman_code(assigned_codes, first_codes, code_bit_lengths, len_code_bit_lengths);


//     return assigned_codes;
// }

// void default_hf_lengths(unsigned char* bit_lengths) {
//     for (unsigned char i = 0; i < 144; ++i) {
//         bit_lengths[i] = 8;
//     }
    
//     for (unsigned short int i = 144; i < 256; ++i) {
//         bit_lengths[i] = 9;
//     }
    
//     for (unsigned short int i = 256; i < 280; ++i) {
//         bit_lengths[i] = 7;
//     }
    
//     for (unsigned short int i = 280; i < 288; ++i) {
//         bit_lengths[i] = 8;
//     }
    
//     return;
// }

// void generate_hf_tables(unsigned int* hf_table[3], unsigned int* hf) {
//     unsigned char sizes[] = {24, 152, 112};
//     unsigned short int lengths[] = {0, 0, 0};
//     for (unsigned char i = 0; i < 4; ++i) {
//         hf_table[i] = (unsigned int*) calloc(sizes[i], sizeof(unsigned int));
//     }
    
//     for (unsigned char i = 0; i < 144; ++i, ++lengths[1]) {
//         hf_table[1][lengths[1]] = hf[i];
//     }
    
//     for (unsigned short int i = 144; i < 256; ++i, ++lengths[2]) {
//         hf_table[2][lengths[2]] = hf[i];
//     }
    
//     for (unsigned short int i = 256; i < 280; ++i, ++lengths[0]) {
//         hf_table[0][lengths[0]] = hf[i];
//     }
    
//     for (unsigned short int i = 280; i < 288; ++i, ++lengths[1]) {
//         hf_table[1][lengths[1]] = hf[i];
//     }
    
//     return;
// }

static void copy_data(unsigned char* sliding_window, unsigned short int* sliding_window_index, unsigned char* dest, unsigned int* index, unsigned short int length, unsigned short int distance) {
    dest = (unsigned char*) realloc(dest, sizeof(unsigned char) * (*index + length));
    unsigned short int copy_pos = (SLIDING_WINDOW_SIZE + *sliding_window_index - distance);
    copy_pos = copy_pos & (SLIDING_WINDOW_SIZE - 1);

    // Copy from the sliding window
    for (unsigned short int i = 0; i < length; ++i, ++(*index)) {
        sliding_window[*sliding_window_index] = sliding_window[copy_pos];
        dest[*index] = (sliding_window[*sliding_window_index]);

        // Advance to the next output position
        *sliding_window_index = *sliding_window_index + 1;
        *sliding_window_index = *sliding_window_index & (SLIDING_WINDOW_SIZE - 1);

        // Advance to the next byte to copy
        copy_pos = copy_pos + 1;
        copy_pos = copy_pos & (SLIDING_WINDOW_SIZE - 1);
    }
    
    return;
}

static unsigned short int decode_hf_value(BitStream* bit_stream, unsigned short int code, unsigned short int* mins, unsigned short int* maxs, unsigned short int* val_ptr, unsigned char length, bool is_fixed) {
    for (unsigned char i = 0; i < length; ++i) {
        if (code <= maxs[i]) {
            return (code - mins[i] + val_ptr[i]);
        }

        if ((i != 1) || (!is_fixed)) {
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
    unsigned int decompressed_stream_start = bit_stream -> byte;
    unsigned int adler_register = 1;
    unsigned char sliding_window[SLIDING_WINDOW_SIZE];
    unsigned short int sliding_window_size;

    // Fixed huffman literal/lengths codes
    const unsigned short int fixed_val_ptr[] = {256, 0, 280, 144};
    const unsigned short int fixed_mins[] = {0x00, 0x30, 0xC0, 0x190};
    const unsigned short int fixed_maxs[] = {0x17, 0xBF, 0xC7, 0x1FF};
    
    // Initialize decompressed data
    unsigned char* decompressed_data = (unsigned char*) calloc(1, sizeof(unsigned char));
    *decompressed_data_length = 0;

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
            free(decompressed_data);
            return ((unsigned char*) "invalid compression type\n");
        }
        
        unsigned short int* val_ptr = NULL;
        unsigned short int* mins = NULL;
        unsigned short int* maxs = NULL;
        unsigned char bits_length = 0;

        // Select between the two huffman tables
        if (type == 2) {
            // Decode Dynamic Huffman Table
            // val_ptr = dynamic_val_ptr;
            // mins = dynamic_mins;
            // maxs = dynamic_maxs;
            continue;
        } else {
            val_ptr = (unsigned short int*) fixed_val_ptr;
            mins = (unsigned short int*) fixed_mins;
            maxs = (unsigned short int*) fixed_maxs;
            bits_length = 4;
        }

        // Decode compressed data, remember to keep adding elements to the sliding window
        unsigned short int code = 0;
        while (TRUE) {
            code = get_next_n_bits(bit_stream, type == 1 ? 7 : 1, TRUE);
            unsigned short int decoded_value = decode_hf_value(bit_stream, code, mins, maxs, val_ptr, bits_length, type == 1);
            debug_print(YELLOW, "decoded_value: %u\n", decoded_value);
            code = 0;
            if (decoded_value < 256) {
                decompressed_data = (unsigned char*) realloc(decompressed_data, sizeof(unsigned char) * (*decompressed_data_length + 1));
                decompressed_data[*decompressed_data_length] = decoded_value;
                (*decompressed_data_length)++;
            } else if (decoded_value == 256) {
                debug_print(WHITE, "END OF COMPRESSED BLOCK\n");
                break;
            } else {
                unsigned short int length = get_length(bit_stream, decoded_value);
                unsigned short int decoded_distance = get_next_n_bits(bit_stream, 5, TRUE); // Implement also dynamic hf decoding
                unsigned short int distance = get_distance(bit_stream, decoded_distance);

                debug_print(YELLOW, "length: %u, distance: %u\n", length, distance);
                
                copy_data(sliding_window, &sliding_window_size, decompressed_data, decompressed_data_length, length, distance);
            }
        }
    }

    unsigned int decompressed_stream_end = bit_stream -> byte;

    // Read the adler_crc
    unsigned int adler_crc = get_next_bytes_ui(bit_stream);

    // Calculate the crc of the blocks
    set_byte(bit_stream, decompressed_stream_start);
    for (unsigned int i = 0; i < (decompressed_stream_end - decompressed_stream_start); ++i) {
        unsigned char byte = get_next_byte_uc(bit_stream);
        update_adler_crc(byte, &adler_register);
    } 

    if (adler_crc != adler_register) {
        *err = 1;
        free(decompressed_data);
        return ((unsigned char*) "corrupted compressed data blocks");
    }

    return decompressed_data;
}

#endif //_DECOMPRESSOR_H_