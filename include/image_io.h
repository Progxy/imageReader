#ifndef _IMAGE_IO_H_
#define _IMAGE_IO_H_

#include <stdio.h>
#include <stdlib.h>
#include "./debug_print.h"
#include "./types.h"

#define CHECK_FILE_TYPE(data) ((data)[0] == 0xFF && (data)[1] == 0xD8)

bool read_image_file(FileData* image_file, const char* filename) {
    FILE* file = fopen(filename, "rb");

    // Check for errors while opening the file
    if (file == NULL) {
        error_print("file '%s' not found!\n", filename);
        return FILE_NOT_FOUND;
    }

    debug_print(YELLOW, "Decoding image: '%s'\n", filename);

    // Get the length of the file
    fseek(file, 0, SEEK_END);
    image_file -> length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Set the data buffer
    image_file -> data = (unsigned char*) calloc(image_file -> length, 1);
    image_file -> length = fread(image_file -> data, 1, image_file -> length, file);

    // Check for errors
    if (ferror(file)) {
        error_print("an error occured while reading the file!\n");
        return FILE_ERROR;
    }

    fclose(file);
    
    if (!CHECK_FILE_TYPE(image_file -> data)) {
        error_print("invalid type of file!\n");
        return INVALID_FILE_TYPE;
    }

    debug_print(GREEN, "The current image file is valid!\n\n");

    return NO_ERROR;
}

bool create_ppm_image(Image* image, const char* filename) {
    FILE* file = fopen(filename, "wb");

    debug_print(YELLOW, "\n");

    // Check for errors while opening the file
    if (file == NULL) {
        error_print("file not found!\n");
        return FILE_NOT_FOUND;
    }

    fwrite("P6\n", sizeof(char), 3, file);

    // Check for errors
    if (ferror(file)) {
        error_print("an error occured while writing the file!\n");
        return FILE_ERROR;
    }

    char size_info[35];
    unsigned int info_len = snprintf(size_info, 35, "%d %d\n255\n", image -> width, image -> height);
    fwrite(size_info, sizeof(char), info_len, file);

    // Check for errors
    if (ferror(file)) {
        error_print("an error occured while writing the file!\n");
        return FILE_ERROR;
    }

    unsigned int b_w = fwrite(image -> decoded_data, 1, image -> size, file);

    if (b_w != image -> size) {
        debug_print(PURPLE, "Diff in size: %u - %u\n", b_w, image -> size);
    }

    // Check for errors
    if (ferror(file)) {
        error_print("an error occured while writing the file!\n");
        return FILE_ERROR;
    }

    fclose(file);

    debug_print(GREEN, "file %s successfully created!\n", filename);
    debug_print(GREEN, "copied %u bytes out of the expected %u\n\n", b_w, image -> components * image -> width * image -> height);

    return NO_ERROR;
}

#endif //_IMAGE_IO_H_