#ifndef _DECODE_HF_
#define _DECODE_HF_

#include <stdio.h>
#include <stdlib.h>

#include "./types.h"
#include "./bitstream.h"
#include "./debug_print.h"

#define DC 0
#define AC 1
#define CHECK_ERRORS(err) if (*err) return 0

typedef enum DecodeFlag {INVALID_BYTE_STUFFING = 0x0100, DNL_MARKER_DETECTED, LENGTH_EXCEEDED} DecodeFlag;

unsigned char next_bit(BitStream* bit_stream, unsigned short int* err) {
    unsigned char cnt = bit_stream -> bit;
    unsigned char b = bit_stream -> current_byte;

    if (cnt == 0) {
        get_next_byte(bit_stream);
        
        if (bit_stream -> error) {
            *err = LENGTH_EXCEEDED;
            return 0;
        }

        b = bit_stream -> current_byte;
        cnt = 8;
        
        if (b == 0xFF) {
            // Skip the next byte for byte stuffing
            get_next_byte(bit_stream);
        
            if (bit_stream -> error) {
                *err = LENGTH_EXCEEDED;
                return 0;
            }

            unsigned char b2 = bit_stream -> current_byte;

            // If DNL process the marker terminate the scan
            if (b2 == 0xDC) {
                SET_COLOR(RED);
                debug_print(YELLOW, "DNL marker found!\n");
                *err = DNL_MARKER_DETECTED;
                return 0;
            } else if (b2 != 0) {
                SET_COLOR(RED);
                debug_print(YELLOW, "INVALID_BYTE_STUFFING!\n");
                // After a 0xFF should always be a 0 for byte stuffing
                *err = INVALID_BYTE_STUFFING;
                return 0;
            }
        }
    }

    unsigned char bit = (b >> 7) & 0x01;
    b <<= 1;
    cnt--;

    // Update the stored values
    bit_stream -> current_byte = b;
    bit_stream -> bit = cnt;
    
    return bit;
}

short int* generate_huffsize(unsigned char* hf_lengths) {
    short int* huff_size = (short int*) calloc(1, sizeof(short int));
    unsigned short int k = 0;
    unsigned short int i = 0;
    unsigned short int j = 1;

    while (i < 16) {   
        if (j > hf_lengths[i]) {
            i++;
            j = 1;
        } else {
            huff_size = (short int*) realloc(huff_size, sizeof(short int) * (k + 1));
            huff_size[k] = i;
            k++;
            j++;
        }
    }

    huff_size = (short int*) realloc(huff_size, sizeof(short int) * (k + 1));
    huff_size[k] = 0;

    return huff_size;
}

short int* generate_huffcode(short int* huff_size) {
    short int* huff_codes = (short int*) calloc(1, sizeof(short int));
    unsigned char k = 0;
    short int code = 0;
    short int si = huff_size[0];

    while (TRUE) {
        do {
            huff_codes = (short int*) realloc(huff_codes, sizeof(short int) * (k + 1));
            huff_codes[k] = code;
            code++;
            k++;
        } while (huff_size[k] == si);

        if (huff_size[k] == 0) {
            break;
        }

        do {
            code <<= 1;
            si++; 
        } while (huff_size[k] != si);
    }

    return huff_codes;  
}

void decode_tables(HuffmanData hf_data) {
    unsigned char i = 0;
    unsigned char j = 0;
    
    while (i < 16) {        
        if (!hf_data.hf_lengths[i]) {
            hf_data.max_codes[i] = -1;
        } else {
            hf_data.val_ptr[i] = j;
            hf_data.min_codes[i] = hf_data.huff_codes[j];
            j += hf_data.hf_lengths[i] - 1;
            hf_data.max_codes[i] = hf_data.huff_codes[j];
            j++;
        }
        i++;
    }    

    return;
}

unsigned char decode(HuffmanData hf_data, BitStream* bit_stream, unsigned short int* err) {
    unsigned char i = 0;
    short int code = next_bit(bit_stream, err);
    CHECK_ERRORS(err);

    while (code > hf_data.max_codes[i]) {  
        code = (code << 1) + next_bit(bit_stream, err);
        CHECK_ERRORS(err);

        i++;
    } 

    unsigned char j = hf_data.val_ptr[i] + code - hf_data.min_codes[i];

    return hf_data.hf_values[j];
}

int receive(unsigned char bits, BitStream* bit_stream, unsigned short int* err) {
    int val = 0;
    unsigned char i = 0; 

    while (i < bits) {
        val = (val << 1) + next_bit(bit_stream, err);
        CHECK_ERRORS(err);

        i++;
    }

    return val;
}

int extend(int v, unsigned char bits) {
    int vt = 1 << (bits - 1);

    if (v < vt) {
        v -= (1 << bits) - 1;
    }

    return v;
}

int decode_dc(HuffmanData huffman_data, BitStream *bit_stream, unsigned short int* err) {
    unsigned char bits = decode(huffman_data, bit_stream, err);
    CHECK_ERRORS(err);

    int diff = receive(bits, bit_stream, err);
    CHECK_ERRORS(err);
    
    diff = extend(diff, bits);

    return diff;
}

void decode_zz(unsigned char k, int* zz, unsigned char low_bits, BitStream* bit_stream, unsigned short int* err) {
    int rec = receive(low_bits, bit_stream, err);
    zz[k] = extend(rec, low_bits);
    if (*err) {
        return;
    }
    return;
}

void decode_ac(HuffmanData huffman_data, int* zz, BitStream *bit_stream, unsigned short int* err) {
    unsigned char k = 1;
    unsigned char rs = 0;
    unsigned char low_bits = 0;
    unsigned char high_bits = 0;
    unsigned char r = 0;

    while (k < 64) {
        rs = decode(huffman_data, bit_stream, err);
        if (*err) {
            return;
        }
    
        low_bits = rs & 0x0F;
        high_bits = ((rs & 0xF0) >> 4) & 0x0F;
        r = high_bits;

        if (low_bits == 0) {
            if (r == 15) {
                k += 16;
                continue;
            }

            return;
        }

        k += r;
        decode_zz(k, zz, low_bits, bit_stream, err);
        k++;
    }

    return;
}

int* decode_data_unit(HuffmanData* huffman_data, BitStream *bit_stream, unsigned short int* err, int* pred) {
    int* zz = (int*) calloc(64, sizeof(int));

    if (*err == LENGTH_EXCEEDED) {
        return zz;
    }

    *pred += decode_dc(huffman_data[DC], bit_stream, err);
    zz[0] = *pred;

    if (*err) {
        return zz;
    }

    decode_ac(huffman_data[AC], zz, bit_stream, err);
    
    return zz;
}

MCU generate_mcu(unsigned char components, BitStream* bit_stream, DataTables* data_table, unsigned short int* err) {
    MCU mcu;
    mcu.comp_du_count = data_table -> comp_du_count;
    mcu.max_du = data_table -> max_du;

    // Decode the data units and group them based on the subsampling factors
    mcu.components = components;
    mcu.data_units = (int**) calloc(data_table -> sf_count, sizeof(int*));
    mcu.data_units_count = 0;

    // Decode the data units required to create the mcu
    for (unsigned char i = 0; i < components; ++i) {
        // Get the huffman data for DC and AC
        HuffmanData huffman_data[2];
        unsigned char hf_index = (data_table -> components)[i].dc_table_id;
        huffman_data[DC] = (data_table -> hf_dc)[hf_index];
        hf_index = (data_table -> components)[i].ac_table_id;
        huffman_data[AC] = (data_table -> hf_ac)[hf_index];

        // Decode the data units for each component
        for (unsigned char j = 0; j < mcu.comp_du_count[i]; ++j) {
            mcu.data_units[mcu.data_units_count] = decode_data_unit(huffman_data, bit_stream, err, &((data_table -> components)[i].pred));
            mcu.data_units_count++;

            if (*err == LENGTH_EXCEEDED) {
                // If data finish leave the mcu filled with zeros
                warning_print("length exceeded, component: %u, data unit: %u\n", i, j);
                
                unsigned char du_count = 0;
                for (unsigned char s = 0; s < mcu.components; ++s) {
                    du_count += mcu.comp_du_count[s];
                }

                for (unsigned char t = mcu.data_units_count; t < du_count; ++t) {
                    mcu.data_units[t] = (int*) calloc(64, sizeof(int));
                    mcu.data_units_count++;
                } 
                
                return mcu;
            } else if (*err) {
                error_print("error in decode_huff...\n");
            }
        }
    }

    return mcu;
}

#endif //_DECODE_HF_
