#ifndef _DECODE_PNG_H_
#define _DECODE_PNG_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"
#include "./bitstream.h"
#include "./chunk.h"

Image decode_png(FileData* image_file);

Image decode_png(FileData* image_file) {
    Image image = (Image) { .error = DECODING_ERROR };

    debug_print(YELLOW, "image_size: %u\n", image_file -> length);

    BitStream* bit_stream = allocate_bit_stream(image_file -> data + 8, image_file -> length - 8);

    // Skip the magic number and then find and validate each chunk
    Chunks chunks = find_and_check_chunks(bit_stream);

    for (unsigned int i = 0; i < chunks.chunks_count; ++i) {
        Chunk chunk = (chunks.chunks)[i];
        debug_print(YELLOW, "length: %u, type: %s, pos: %u\n", chunk.length, chunk.chunk_type, chunk.pos);
    }

    // Deallocate BitStream
    free(bit_stream);

    return image;
}

#endif //_DECODE_PNG_H_