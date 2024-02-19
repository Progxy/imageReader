#ifndef _DECODE_PNG_H_
#define _DECODE_PNG_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"
#include "./chunk.h"
#include "./decompressor.h"

#define CHECK_VALID_BIT_DEPTH(bit_depth, start, len)                            \
                            for (unsigned char i = 0; i < len; ++i)             \
                                if (bit_depth == valid_bit_depths[i + start])   \
                                    return TRUE                                 \

#define CHECK_VALID_COLOR_TYPE(color_type)                                  \
                            for (unsigned char i = 0; i < 5; ++i)           \
                                if (color_type == valid_color_types[i])     \
                                    index = i;                              \

const unsigned char valid_bit_depths[] = {1, 2, 4, 8, 16};
const PNGType valid_color_types[] = {GREYSCALE, TRUECOLOR, INDEXED_COLOR, GREYSCALE_ALPHA, TRUECOLOR_ALPHA};
const unsigned char color_types_starts[] = {0, 3, 0, 3, 3};
const unsigned char color_types_lengths[] = {5, 2, 4, 2, 2};

/* -------------------------------------------------------------------------------------- */

static bool is_str_equal(unsigned char* str_a, unsigned char* str_b, unsigned int len);
void decode_ihdr(PNGImage* image, Chunk ihdr_chunk);
Image decode_png(FileData* image_file);

/* -------------------------------------------------------------------------------------- */

static bool is_str_equal(unsigned char* str_a, unsigned char* str_b, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i) {
        if (str_a[i] != str_b[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

static bool is_valid_depth_color_combination(unsigned char bit_depth, PNGType color_type) {
    unsigned char index = 0xFF;
    CHECK_VALID_COLOR_TYPE(color_type);

    if (index == 0xFF) {
        return FALSE;
    }
    
    CHECK_VALID_BIT_DEPTH(bit_depth, color_types_starts[index], color_types_lengths[i]);
    
    return FALSE;
}

void decode_ihdr(PNGImage* image, Chunk ihdr_chunk) {
    if (ihdr_chunk.length != 13) {
        error_print("invalid length of the IHDR chunk: %u while should be 13\n", ihdr_chunk.length);
        (image -> image_data).error = INVALID_CHUNK_LENGTH;
        return;
    }

    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", ihdr_chunk.chunk_type, ihdr_chunk.length, ihdr_chunk.pos);
    set_byte(image -> bit_stream, ihdr_chunk.pos);

    (image -> image_data).width = get_next_bytes_ui(image -> bit_stream);
    (image -> image_data).height = get_next_bytes_ui(image -> bit_stream);

    if ((image -> image_data).width == 0 || (image -> image_data).height == 0) {
        error_print("invalid image size: %u x %u\n", (image -> image_data).width, (image -> image_data).height);
        (image -> image_data).error = INVALID_IMAGE_SIZE;
        return;
    }

    debug_print(YELLOW, "width: %u\n", (image -> image_data).width);
    debug_print(YELLOW, "height: %u\n", (image -> image_data).height);

    image -> bit_depth = get_next_byte_uc(image -> bit_stream);
    image -> color_type = get_next_byte_uc(image -> bit_stream);
    
    if (!is_valid_depth_color_combination(image -> bit_depth, image -> color_type)) {
        error_print("invalid bit depth [%u] and color type [%u] combination\n", image -> bit_depth, image -> color_type);
        (image -> image_data).error = INVALID_DEPTH_COLOR_COMBINATION;
        return;
    }

    debug_print(YELLOW, "bit_depth: %u\n", image -> bit_depth);
    debug_print(YELLOW, "color_type: %s\n", png_types[image -> color_type]);

    image -> compression_method = get_next_byte_uc(image -> bit_stream);
    if (image -> compression_method) {
        error_print("invalid compression method expected 0 instead of %u\n", image -> compression_method);
        (image -> image_data).error = INVALID_COMPRESSION_METHOD;
        return;
    }
    
    debug_print(YELLOW, "compression_method: %u\n", image -> compression_method);

    image -> filter_method = get_next_byte_uc(image -> bit_stream);
    if (image -> filter_method) {
        error_print("invalid filter method expected 0 instead of %u\n", image -> filter_method);
        (image -> image_data).error = INVALID_FILTER_METHOD;
        return;
    }

    debug_print(YELLOW, "filter_method: %u\n", image -> filter_method);

    image -> interlace_method = get_next_byte_uc(image -> bit_stream);
    if (image -> interlace_method > 1) {
        error_print("invalid interlace method expected 0 or 1 instead of %u\n", image -> interlace_method);
        (image -> image_data).error = INVALID_INTERLACE_METHOD;
        return;
    }

    debug_print(YELLOW, "interlace_method: %u\n\n", image -> interlace_method);

    return;
}

void decode_plte(PNGImage* image, Chunk plte_chunk) {
    if (plte_chunk.length % 3) {
        error_print("invalid length as it should be divisible by 3\n");
        (image -> image_data).error = DECODING_ERROR;
        return;
    } else if ((image -> palette).R != NULL) {
        error_print("there's should be only one PLTE chunk\n");
        (image -> image_data).error = DECODING_ERROR;
        return;
    } else if ((image -> color_type) == GREYSCALE || (image -> color_type) == GREYSCALE_ALPHA) {
        error_print("the PLTE chunk shouldn't be defined for greyscale images\n");
        (image -> image_data).error = DECODING_ERROR;
        return;
    }

    set_byte(image -> bit_stream, plte_chunk.pos);

    unsigned int palette_size = plte_chunk.length / 3;
    unsigned int palette_size_limit = 1 << (image -> bit_depth);

    if (palette_size > palette_size_limit || palette_size > 256) {
        error_print("invalid palette size [%u] as it should be less than %u and 256\n", palette_size, palette_size_limit);
        (image -> image_data).error = DECODING_ERROR;
        return;
    }

    (image -> palette).R = (unsigned char*) calloc(palette_size, sizeof(unsigned char));
    (image -> palette).G = (unsigned char*) calloc(palette_size, sizeof(unsigned char));
    (image -> palette).B = (unsigned char*) calloc(palette_size, sizeof(unsigned char));

    for (unsigned int i = 0; i < palette_size; ++i) {
        (image -> palette).R[i] = get_next_byte_uc(image -> bit_stream);
        (image -> palette).G[i] = get_next_byte_uc(image -> bit_stream);
        (image -> palette).B[i] = get_next_byte_uc(image -> bit_stream);
    }

    return;
}

void decode_idat(PNGImage* image, Chunk idat_chunk) {
    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", idat_chunk.chunk_type, idat_chunk.length, idat_chunk.pos);
    
    if (((image -> color_type) == INDEXED_COLOR) && ((image -> palette).R == NULL)) {
        error_print("the palette should be defined before the IDAT chunk\n");
        (image -> image_data).error = DECODING_ERROR;
        return;
    }

    // Check that there's no other IDAT chunks before this one
    set_byte(image -> bit_stream, idat_chunk.pos);

    // Decode the compressed data
    unsigned char window_size = get_next_n_bits(image -> bit_stream, 4);
    debug_print(YELLOW, "window_size: %u\n", window_size);
    unsigned char compression_method = get_next_n_bits(image -> bit_stream, 4);
    debug_print(YELLOW, "compression_method: %u\n", compression_method);
    unsigned char compression_level = get_next_n_bits(image -> bit_stream, 2);
    debug_print(YELLOW, "compression_level: %u\n", compression_level);
    unsigned char preset_dictionary = get_next_bit(image -> bit_stream);
    debug_print(YELLOW, "preset_dictionary: %u\n", preset_dictionary);
    unsigned char check_bits = get_next_n_bits(image -> bit_stream, 5);
    debug_print(YELLOW, "check_bits: %u\n", check_bits);

    // Decode each compressed block
    unsigned char final = 0;
    // --- debug purpose
    unsigned char counter = 0;
    while (!final && counter < 1) {
        final = get_next_bit(image -> bit_stream);
        unsigned char type = get_next_bit(image -> bit_stream);
        type = (type << 1) + get_next_bit(image -> bit_stream);

        debug_print(YELLOW, "final: %u\n", final);
        debug_print(YELLOW, "type: %u\n", type);

        if (type == 0) {
            unsigned short int length = get_next_bytes_us(image -> bit_stream);
            unsigned short int length_c = get_next_bytes_us(image -> bit_stream);
            unsigned short int check = length ^ length_c;
            debug_print(YELLOW, "block length: %u, check: %u\n", length, check);
        }
        // ---- debug purpose
        counter++;
    }

    debug_print(YELLOW, "\n");

    return;
}

void decode_iend(PNGImage* image, Chunk iend_chunk) {
    if (iend_chunk.length) {
        error_print("invalid IEND chunk size [%u] as it should be 0", iend_chunk.length);
        (image -> image_data).error = INVALID_IEND_CHUNK_SIZE;
        return; 
    }

    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", iend_chunk.chunk_type, iend_chunk.length, iend_chunk.pos);
    debug_print(YELLOW, "End of the image\n");
    
    return;
}

Image decode_png(FileData* image_file) {
    Chunks chunks = find_and_check_chunks(image_file -> data, image_file -> length);
    PNGImage* image = (PNGImage*) calloc(1, sizeof(PNGImage));
    image -> bit_stream = allocate_bit_stream(image_file -> data, image_file -> length);
    image -> palette = (RGB) { .R = NULL, .G = NULL, .B = NULL };

    debug_print(BLUE, "image_size: %u\n\n", image_file -> length);

    for (unsigned int i = 0; i < chunks.chunks_count && !((image -> image_data).error); ++i) {
        Chunk chunk = (chunks.chunks)[i];
        if (is_str_equal((unsigned char*) "IHDR", chunk.chunk_type, 4)) {
            decode_ihdr(image, chunk);
            continue;
        } else if (is_str_equal((unsigned char*) "PLTE", chunk.chunk_type, 4)) {
            decode_plte(image, chunk);
            continue;
        } else if (is_str_equal((unsigned char*) "IDAT", chunk.chunk_type, 4)) {
            decode_idat(image, chunk);
            continue;
        } else if (is_str_equal((unsigned char*) "IEND", chunk.chunk_type, 4)) {
            decode_iend(image, chunk);
            continue;
        }

        debug_print(PURPLE, "unknown type: %s, length: %u, pos: %u\n\n", chunk.chunk_type, chunk.length, chunk.pos);
    }

    debug_print(YELLOW, "\n");

    // Deallocate stuff
    deallocate_bit_stream(image -> bit_stream);

    (image -> image_data).error = DECODING_ERROR;
    return image -> image_data;
}

#endif //_DECODE_PNG_H_