#ifndef _DECODE_PNG_H_
#define _DECODE_PNG_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"
#include "./chunk.h"

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

void decode_ihdr(PNGImage* image, Chunk ihdr_chunk) {
    debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", ihdr_chunk.chunk_type, ihdr_chunk.length, ihdr_chunk.pos);
    set_byte(image -> bit_stream, ihdr_chunk.pos);

    (image -> image_data).width = get_next_bytes_ui(image -> bit_stream);
    (image -> image_data).height = get_next_bytes_ui(image -> bit_stream);

    debug_print(YELLOW, "width: %u\n", (image -> image_data).width);
    debug_print(YELLOW, "height: %u\n", (image -> image_data).height);

    image -> bit_depth = get_next_byte_uc(image -> bit_stream);
    debug_print(YELLOW, "bit_depth: %u\n", image -> bit_depth);

    image -> color_type = get_next_byte_uc(image -> bit_stream);
    debug_print(YELLOW, "color_type: %u\n", image -> color_type);

    image -> compression_method = get_next_byte_uc(image -> bit_stream);
    debug_print(YELLOW, "compression_method: %u\n", image -> compression_method);

    image -> filter_method = get_next_byte_uc(image -> bit_stream);
    debug_print(YELLOW, "filter_method: %u\n", image -> filter_method);

    image -> interlace_method = get_next_byte_uc(image -> bit_stream);
    debug_print(YELLOW, "interlace_method: %u\n", image -> interlace_method);
    debug_print(YELLOW, "\n");

    return;
}

Image decode_png(FileData* image_file) {
    Chunks chunks = find_and_check_chunks(image_file -> data, image_file -> length);
    PNGImage* image = (PNGImage*) calloc(1, sizeof(PNGImage));
    image -> bit_stream = allocate_bit_stream(image_file -> data, image_file -> length);
    (image -> image_data).error = DECODING_ERROR;

    debug_print(BLUE, "image_size: %u\n\n", image_file -> length);

    for (unsigned int i = 0; i < chunks.chunks_count; ++i) {
        Chunk chunk = (chunks.chunks)[i];
        if (is_str_equal((unsigned char*) "IHDR", chunk.chunk_type, 4)) {
            decode_ihdr(image, chunk);
            continue;
        } else if (is_str_equal((unsigned char*) "IEND", chunk.chunk_type, 4)) {
            debug_print(PURPLE, "type: %s, length: %u, pos: %u\n", chunk.chunk_type, chunk.length, chunk.pos);
            debug_print(YELLOW, "End of the image\n");
            continue;
        }

        debug_print(PURPLE, "unknown type: %s, length: %u, pos: %u\n\n", chunk.chunk_type, chunk.length, chunk.pos);
    }

    debug_print(YELLOW, "\n");

    // Deallocate stuff
    deallocate_bit_stream(image -> bit_stream);

    return image -> image_data;
}

#endif //_DECODE_PNG_H_