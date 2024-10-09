#ifndef _DECODE_PPM_H_
#define _DECODE_PPM_H_

#include <stdlib.h>
#include "./types.h"
#include "./bitstream.h"

#define ARR_LEN(arr) sizeof(arr) / sizeof(arr[0])

const char str_terminators[] = {' ', '\0', '\t', '\r', '\n'};

Image decode_ppm(FileData* image_file) {
    PPMImage* image = (PPMImage*) calloc(1, sizeof(PPMImage));
    image -> bit_stream = allocate_bit_stream(image_file -> data, image_file -> length, FALSE);
    (image -> image_data).size = 0;
    (image -> image_data).components = 3;

    set_byte(image -> bit_stream, 3);

    char* width_str = get_str(image -> bit_stream, (unsigned char*) str_terminators, ARR_LEN(str_terminators));
    (image -> image_data).width = atoi(width_str);
    free(width_str);

    char* height_str = get_str(image -> bit_stream, (unsigned char*) str_terminators, ARR_LEN(str_terminators));
    (image -> image_data).height = atoi(height_str);
    free(height_str);

    debug_print(WHITE, "width: %u\n", (image -> image_data).width);
    debug_print(WHITE, "height: %u\n", (image -> image_data).height);

    char* max_rgb_value_str = get_str(image -> bit_stream, (unsigned char*) str_terminators, ARR_LEN(str_terminators));
    unsigned short int max_rgb_value = atoi(max_rgb_value_str);
    free(max_rgb_value_str);

    if (max_rgb_value != 255) {
        (image -> image_data).error = DECODING_ERROR;
        error_print("invalid max rgb value, expected 255 instead of %u!\n", max_rgb_value);
        return image -> image_data;
    }

    debug_print(WHITE, "max rgb value: %u\n", max_rgb_value);

    (image -> image_data).size = (image -> image_data).width * (image -> image_data).height * (image -> image_data).components;
    (image -> image_data).decoded_data = get_next_n_byte_uc(image -> bit_stream, (image -> image_data).size);

    deallocate_bit_stream(image -> bit_stream);
	
	Image image_data = image -> image_data;
	free(image);

    return image_data;
}

#endif //_DECODE_PPM_H_
