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
                            for (unsigned char i = 0; i < 7; ++i)           \
                                if (color_type == valid_color_types[i])     \
                                    index = i;                              \

#define GET_INTERVAL_FROM_BIT_DEPTH(bit_depth) (1 + (bit_depth == 16))

#define GET_PIXEL(decompressed_data, row, row_len, col) decompressed_data[row * (row_len) + col]
#define GET_PIXEL_LEFT(decompressed_data, row, row_len, col, interval) ((col < interval) ? 0 : decompressed_data[row * row_len + col - interval])
#define GET_PIXEL_ABOVE(decompressed_data, row, row_len, col) (!row ? 0 : decompressed_data[row_len * (row - 1) + col])
#define GET_PIXEL_ABOVE_LEFT(decompressed_data, row, row_len, col, interval) ((!row || (col < interval)) ? 0 : decompressed_data[row_len * (row - 1) + col - interval])

const unsigned char valid_bit_depths[] = {1, 2, 4, 8, 16};
const PNGType valid_color_types[] = {GREYSCALE, -1, TRUECOLOR, INDEXED_COLOR, GREYSCALE_ALPHA, -1, TRUECOLOR_ALPHA};
const unsigned char color_types_starts[] = {0, 0, 3, 0, 3, 0, 3};
const unsigned char color_types_lengths[] = {5, 0, 2, 4, 2, 0, 2};
const char* months_names[] = {"", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

/* -------------------------------------------------------------------------------------- */

static bool is_str_equal(unsigned char* str_a, unsigned char* str_b, unsigned int len);
static bool is_valid_depth_color_combination(unsigned char bit_depth, PNGType color_type);
static void assign_components_count(PNGImage* image);
static void convert_to_RGB(PNGImage* image);
static int paeth_predictor(unsigned char left, unsigned char above, unsigned char above_left);
void decode_ihdr(PNGImage* image, Chunk ihdr_chunk);
void decode_plte(PNGImage* image, Chunk plte_chunk);
void decode_idat(PNGImage* image, Chunk idat_chunk);
void decode_iend(PNGImage* image, Chunk iend_chunk);
void decode_time(PNGImage* image, Chunk time_chunk);
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

static void assign_filter_interval(PNGImage* image) {
    if (image -> color_type == GREYSCALE) {
        image -> filter_interval = GET_INTERVAL_FROM_BIT_DEPTH(image -> bit_depth);
    } else if (image -> color_type == GREYSCALE_ALPHA) {
        image -> filter_interval = (2 * GET_INTERVAL_FROM_BIT_DEPTH(image -> bit_depth));
    } else if (image -> color_type == INDEXED_COLOR) {
        image -> filter_interval = 1;
    } else if (image -> color_type == TRUECOLOR) {
        image -> filter_interval = (3 * GET_INTERVAL_FROM_BIT_DEPTH(image -> bit_depth));
    } else if (image -> color_type == TRUECOLOR_ALPHA) {
        image -> filter_interval = (4 * GET_INTERVAL_FROM_BIT_DEPTH(image -> bit_depth));
    }
    return;
}

static bool is_valid_depth_color_combination(unsigned char bit_depth, PNGType color_type) {
    unsigned char index = 0xFF;
    CHECK_VALID_COLOR_TYPE(color_type);

    if (index == 0xFF) {
        return FALSE;
    }

    CHECK_VALID_BIT_DEPTH(bit_depth, color_types_starts[index], color_types_lengths[index]);
    
    return FALSE;
}

static void assign_components_count(PNGImage* image) {
    switch (image -> color_type) {
            case GREYSCALE: {
                (image -> image_data).components = 3;
                break;
            }

            case TRUECOLOR: {
                (image -> image_data).components = 3;
                break;
            }

            case INDEXED_COLOR: {
                (image -> image_data).components = 3;
                break;
            }

            case GREYSCALE_ALPHA: {
                (image -> image_data).components = 4;
                break;
            }

            case TRUECOLOR_ALPHA: {
                (image -> image_data).components = 4;
                break;
            }
        }
    return;
}

static void convert_to_RGB(PNGImage* image) {
    unsigned char components = (image -> image_data).components;
    unsigned char* decoded_data = (image -> image_data).decoded_data;
    unsigned int size = (image -> image_data).size;
    unsigned char bit_depth = image -> bit_depth;
    unsigned int width = (image -> image_data).width;
    unsigned int height = (image -> image_data).height;
    BitStream* bit_stream = allocate_bit_stream(decoded_data, size);
    unsigned int new_size = width * height;

    RGBA rgba = (RGBA) {};
    rgba.R = (unsigned char*) calloc(new_size, sizeof(unsigned char));
    rgba.G = (unsigned char*) calloc(new_size, sizeof(unsigned char));
    rgba.B = (unsigned char*) calloc(new_size, sizeof(unsigned char));
    if (components == 4) rgba.A = (unsigned char*) calloc(new_size, sizeof(unsigned char));

    for (unsigned int index = 0; index < new_size; ++index) {
        unsigned char data = get_next_byte_uc(bit_stream);
        rgba.R[index] = data;
        rgba.G[index] = (image -> is_palette_defined || image -> color_type == GREYSCALE || image -> color_type == GREYSCALE_ALPHA) ? data : get_next_byte_uc(bit_stream);
        rgba.B[index] = (image -> is_palette_defined || image -> color_type == GREYSCALE || image -> color_type == GREYSCALE_ALPHA) ? data : get_next_n_bits(bit_stream, bit_depth, FALSE);
        if (components == 4) rgba.A[index] = get_next_byte_uc(bit_stream);
        if (bit_stream -> error) {
            (image -> image_data).error = DECODING_ERROR;
            return;
        }
    }

    deallocate_bit_stream(bit_stream);
    (image -> image_data).decoded_data = (unsigned char*) calloc(width * height * components, sizeof(unsigned char));
    (image -> image_data).size = 0;
    
    for (unsigned int i = 0, index = 0; i < (width * height * components); i += components, ++index, ((image -> image_data).size) += components) {
        ((image -> image_data).decoded_data)[i] = (image -> is_palette_defined) ? (image -> palette).R[rgba.R[index]] : rgba.R[index];
        ((image -> image_data).decoded_data)[i + 1] = (image -> is_palette_defined) ? (image -> palette).G[rgba.G[index]] : rgba.G[index];
        ((image -> image_data).decoded_data)[i + 2] = (image -> is_palette_defined) ? (image -> palette).B[rgba.B[index]] : rgba.B[index];
        if (components == 4) ((image -> image_data).decoded_data)[i + 3] = rgba.A[index];
    }

    free(rgba.R);
    free(rgba.G);
    free(rgba.B);
    if (components == 4) free(rgba.A);

    return;
}

static int paeth_predictor(unsigned char left, unsigned char above, unsigned char above_left) {
    int p = left + above - above_left;
    int pa = abs(p - left);
    int pb = abs(p - above);
    int pc = abs(p - above_left);

    if ((pa <= pb) && (pa <= pc)) return left;
    if (pb <= pc) return above;
    return above_left;
}

static void defilter(PNGImage* image, unsigned char* decompressed_data, unsigned int decompressed_data_size, unsigned char bit_depth) {
    unsigned char interval = image -> filter_interval;
    unsigned int row_len = (image -> image_data).width * interval;
    unsigned int row = 0;

    unsigned int none = 0;
    unsigned int subtract = 0;
    unsigned int up = 0;
    unsigned int average = 0;
    unsigned int paeth = 0;

    BitStream* decompressed_stream = allocate_bit_stream(decompressed_data, decompressed_data_size);
    unsigned int limit_size = ((image -> image_data).width * (image -> image_data).height * interval) + (image -> image_data).height;
    debug_print(WHITE, "decompressed_data size: %u, limit_size: %u\n", decompressed_data_size, limit_size);

    for (unsigned int i = 0; i < limit_size; ++i, ++row) {
        clean_bits(decompressed_stream);
        unsigned char filter_type = get_next_byte_uc(decompressed_stream);
        unsigned int* size = &((image -> image_data).size);
        (image -> image_data).decoded_data = (unsigned char*) realloc((image -> image_data).decoded_data, sizeof(unsigned char) * ((*size) + row_len));
        switch (filter_type) {
            case 0: {
                for (unsigned int col = 0; col < row_len; ++i, ++col, ++(*size)) {
                    (image -> image_data).decoded_data[*size] = get_next_n_bits(decompressed_stream, bit_depth, FALSE) & 0xFF;
                }
                none++;
                break;
            }            

            case 1: {
                for (unsigned int col = 0; col < row_len; ++i, ++col, ++(*size)) {
                    int data = get_next_n_bits(decompressed_stream, bit_depth, FALSE) & 0xFF;
                    (image -> image_data).decoded_data[*size] = (data + (int) GET_PIXEL_LEFT((image -> image_data).decoded_data, row, row_len, col, interval)) & 0xFF;
                }
                subtract++;
                break;
            }           
            
            case 2: {
                for (unsigned int col = 0; col < row_len; ++i, ++col, ++(*size)) {
                    int data = get_next_n_bits(decompressed_stream, bit_depth, FALSE) & 0xFF;
                    (image -> image_data).decoded_data[*size] = (data + (int) GET_PIXEL_ABOVE((image -> image_data).decoded_data, row, row_len, col)) & 0xFF;
                }
                up++;
                break;
            }
            
            case 3: {
                for (unsigned int col = 0; col < row_len; ++i, ++col, ++(*size)) {
                    int data = get_next_n_bits(decompressed_stream, bit_depth, FALSE) & 0xFF;
                    (image -> image_data).decoded_data[*size] = (data + ((int) GET_PIXEL_LEFT((image -> image_data).decoded_data, row, row_len, col, interval) + (int) GET_PIXEL_ABOVE((image -> image_data).decoded_data, row, row_len, col)) / 2) & 0xFF;
                }
                average++;
                break;
            }            
            
            case 4: {
                for (unsigned int col = 0; col < row_len; ++i, ++col, ++(*size)) {
                    int data = get_next_n_bits(decompressed_stream, bit_depth, FALSE) & 0xFF;
                    (image -> image_data).decoded_data[*size] = (data + paeth_predictor(GET_PIXEL_LEFT((image -> image_data).decoded_data, row, row_len, col, interval), GET_PIXEL_ABOVE((image -> image_data).decoded_data, row, row_len, col), GET_PIXEL_ABOVE_LEFT((image -> image_data).decoded_data, row, row_len, col, interval)));
                }
                paeth++;
                break;
            }

            default: {
                warning_print("invalid filter type: %u, row: %u, row_len: %u, i: %u\n", filter_type, row + 1, row_len, i);
                break;
            }
        }
    }

    debug_print(YELLOW, "none: %u, subtract: %u, up: %u, average: %u, paeth: %u\n", none, subtract, up, average, paeth);
    
    deallocate_bit_stream(decompressed_stream);

    return;
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
    
    debug_print(YELLOW, "bit_depth: %u\n", image -> bit_depth);
    debug_print(YELLOW, "color_type: %s\n", png_types[image -> color_type]);

    if (!is_valid_depth_color_combination(image -> bit_depth, image -> color_type)) {
        error_print("invalid bit depth [%u] and color type [%u] combination\n", image -> bit_depth, image -> color_type);
        (image -> image_data).error = INVALID_DEPTH_COLOR_COMBINATION;
        return;
    }

    assign_components_count(image);
    assign_filter_interval(image);

    debug_print(YELLOW, "components: %u\n", (image -> image_data).components);
    debug_print(YELLOW, "filter interval: %u\n", image -> filter_interval);

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
    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", plte_chunk.chunk_type, plte_chunk.length, plte_chunk.pos);
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

    debug_print(BLUE, "reading palette info...\n");

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

    image -> is_palette_defined = TRUE;

    debug_print(BLUE, "palette info successfully read!\n\n");

    return;
}

void decode_idat(PNGImage* image, Chunk idat_chunk) {
    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", idat_chunk.chunk_type, idat_chunk.length, idat_chunk.pos);
    
    if (((image -> color_type) == INDEXED_COLOR) && (!image -> is_palette_defined)) {
        error_print("the palette should be defined before the IDAT chunk\n");
        (image -> image_data).error = DECODING_ERROR;
        return;
    }

    // Check that there's no other IDAT chunks before this one
    set_byte(image -> bit_stream, idat_chunk.pos);
    debug_print(BLUE, "init deflating...\n");

    unsigned char* compressed_data = get_next_n_byte_uc(image -> bit_stream, idat_chunk.length);
    debug_print(YELLOW, "\n");
    debug_print(YELLOW, "read: %u, length: %u\n", (image -> bit_stream) -> byte, idat_chunk.length + idat_chunk.pos);
    BitStream* compressed_stream = allocate_bit_stream(compressed_data, idat_chunk.length);

    unsigned char err = 0;
    unsigned int stream_length = 0;
    unsigned char* decompressed_stream = inflate(compressed_stream, &err, &stream_length);
    deallocate_bit_stream(compressed_stream);
    
    if (err) {
        error_print((char*) decompressed_stream);
        (image -> image_data).error = DECODING_ERROR;
        return;
    }

    debug_print(YELLOW, "\n");
    debug_print(BLUE, "starting defiltering...\n");
    defilter(image, decompressed_stream, stream_length, image -> bit_depth);
    debug_print(WHITE, "defiltered data len: %u\n", (image -> image_data).size);

    if (image -> interlace_method) {
        error_print("implement the interlacing Adam 7 method...\n");
        (image -> image_data).error = DECODING_ERROR;
        return;
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

void decode_time(PNGImage* image, Chunk time_chunk) {
    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", time_chunk.chunk_type, time_chunk.length, time_chunk.pos);
    set_byte(image -> bit_stream, time_chunk.pos);
    unsigned short int year = get_next_bytes_us(image -> bit_stream);
    unsigned char month = get_next_byte_uc(image -> bit_stream);
    unsigned char day = get_next_byte_uc(image -> bit_stream);
    unsigned char hour = get_next_byte_uc(image -> bit_stream);
    unsigned char minute = get_next_byte_uc(image -> bit_stream);
    unsigned char second = get_next_byte_uc(image -> bit_stream);

    debug_print(WHITE, "Last modification time was at %u:%u:%u of %u %s %u\n\n", hour, minute, second, day, months_names[month], year);

    return;
}

Image decode_png(FileData* image_file) {
    Chunks chunks = find_and_check_chunks(image_file -> data, image_file -> length);
    PNGImage* image = (PNGImage*) calloc(1, sizeof(PNGImage));
    image -> bit_stream = allocate_bit_stream(image_file -> data, image_file -> length);
    image -> palette = (RGBA) { .R = NULL, .G = NULL, .B = NULL, .A = NULL };
    image -> is_palette_defined = FALSE;
    (image -> image_data).decoded_data = (unsigned char*) calloc(1, sizeof(unsigned char));
    (image -> image_data).size = 0;

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
        } else if (is_str_equal((unsigned char*) "tIME", chunk.chunk_type, 4)) {
            decode_time(image, chunk);
            continue;
        }

        debug_print(PURPLE, "unknown type: %s, length: %u, pos: %u\n\n", chunk.chunk_type, chunk.length, chunk.pos);
    }

    if ((image -> image_data).error) {
        return image -> image_data;
    }

    debug_print(YELLOW, "\n");

    convert_to_RGB(image);

    // Deallocate stuff
    deallocate_bit_stream(image -> bit_stream);

    return image -> image_data;
}

#endif //_DECODE_PNG_H_