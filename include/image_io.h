#ifndef _IMAGE_IO_H_
#define _IMAGE_IO_H_

#include <stdio.h>
#include <stdlib.h>

#ifndef _USE_IMAGE_LIBRARY_
#include "./debug_print.h"
#include "./types.h"
#include "./decode_jpeg.h"
#include "./decode_png.h"
#include "./decode_ppm.h"
#endif //_USE_IMAGE_LIBRARY_

#ifdef _USE_IMAGE_LIBRARY_

typedef enum ImageError {NO_ERROR, FILE_NOT_FOUND, INVALID_FILE_TYPE, FILE_ERROR, INVALID_MARKER_LENGTH, INVALID_QUANTIZATION_TABLE_NUM, INVALID_HUFFMAN_TABLE_NUM, INVALID_IMAGE_SIZE, EXCEEDED_LENGTH, UNSUPPORTED_JPEG_TYPE, INVALID_DEPTH_COLOR_COMBINATION, INVALID_CHUNK_LENGTH, INVALID_COMPRESSION_METHOD, INVALID_FILTER_METHOD, INVALID_INTERLACE_METHOD, INVALID_IEND_CHUNK_SIZE, DECODING_ERROR} ImageError; 
typedef enum FileType {JPEG, PNG, PPM} FileType;
typedef unsigned char bool;

const char* err_codes[] = {"NO_ERROR", "FILE_NOT_FOUND", "INVALID_FILE_TYPE", "FILE_ERROR", "INVALID_MARKER_LENGTH", "INVALID_QUANTIZATION_TABLE_NUM", "INVALID_HUFFMAN_TABLE_NUM", "INVALID_IMAGE_SIZE", "EXCEEDED_LENGTH", "UNSUPPORTED_JPEG_TYPE", "INVALID_DEPTH_COLOR_COMBINATION", "INVALID_CHUNK_LENGTH", "INVALID_COMPRESSION_METHOD", "INVALID_FILTER_METHOD", "INVALID_INTERLACE_METHOD", "INVALID_IEND_CHUNK_SIZE", "DECODING_ERROR"};

typedef struct FileData {
    unsigned int length;
    unsigned char* data;
    FileType file_type;
} FileData;

typedef struct Image {
    unsigned int width;
    unsigned int height;
    unsigned char* decoded_data;
    unsigned int size;
    unsigned char components;
    ImageError error;
} Image;

#endif //_USE_IMAGE_LIBRARY_

Image decode_image(const char* file_path);
bool create_ppm_image(Image image, const char* filename);
void flip_image_horizontally(Image image);
void flip_image_vertically(Image image);
void deallocate_image(Image image);

#ifdef _NO_LIBRARY_
#define _IMAGE_IO_IMPLEMENTATION_
#endif //_NO_LIBRARY_

#ifdef _IMAGE_IO_IMPLEMENTATION_

#define CHECK_JPEG(data) ((data)[0] == 0xFF && (data)[1] == 0xD8)

static const unsigned char png_magic_numbers[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

static bool is_file_png(unsigned char* image_file_data) {
    bool is_png = TRUE;
    for (unsigned char i = 0; i < 8; ++i) {
        if ((image_file_data)[i] != png_magic_numbers[i]) {
            is_png = FALSE;
            break;
        }
    }
    return is_png;
}

static bool is_file_ppm(unsigned char* image_file_data) {
    return is_str_equal((unsigned char*) "P6", image_file_data, 2);
}

static bool check_image_file(FileData* image_file) {
    if (CHECK_JPEG(image_file -> data)) {
        image_file -> file_type = JPEG;
        return TRUE;
    } else if (is_file_png(image_file -> data)) {
        image_file -> file_type = PNG;
        return TRUE;
    } else if (is_file_ppm(image_file -> data)) {
        image_file -> file_type = PPM;
        return TRUE;
    }

    return FALSE;
}

static void deallocate_file_data(FileData* image_file, bool deallocate_data) {
    debug_print(BLUE, "deallocating file data...\n");
    if (deallocate_data) free(image_file -> data);
    free(image_file);
    return;
}

static bool read_image_file(FileData* image_file, const char* filename) {
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
    
    if (!check_image_file(image_file)) {
        error_print("invalid type of file!\n");
        return INVALID_FILE_TYPE;
    }

    debug_print(GREEN, "The current image file is valid %s image!\n\n", file_types[image_file -> file_type]);

    return NO_ERROR;
}

#endif //_IMAGE_IO_IMPLEMENTATION_

#ifdef _NO_LIBRARY_

Image decode_image(const char* file_path) {
    Image image = {0};

    // Read the given file
    bool status = 0;
    FileData* image_file = (FileData*) calloc(1, sizeof(FileData));
    if ((status = read_image_file(image_file, file_path))) {
        image.error = status;
        deallocate_file_data(image_file, TRUE);
        return image;
    }

    if (image_file -> file_type == JPEG) {
        image = decode_jpeg(image_file);
    } else if (image_file -> file_type == PNG) {
        image = decode_png(image_file);
    } else if (image_file -> file_type == PPM) {
        image = decode_ppm(image_file);
    }

    deallocate_file_data(image_file, FALSE);

    return image;
}

bool create_ppm_image(Image image, const char* filename) {
    if (image.size == 0) {
        error_print("the image size is zero!\n");
        return INVALID_IMAGE_SIZE;
    }

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
    unsigned int info_len = snprintf(size_info, 35, "%d %d\n255\n", image.width, image.height);
    fwrite(size_info, sizeof(char), info_len, file);

    // Check for errors
    if (ferror(file)) {
        error_print("an error occured while writing the file!\n");
        return FILE_ERROR;
    }

    unsigned int b_w = fwrite(image.decoded_data, 1, image.size, file);

    if (b_w != image.size) {
        debug_print(PURPLE, "Diff in size: %u - %u\n", b_w, image.size);
    }

    // Check for errors
    if (ferror(file)) {
        error_print("an error occured while writing the file!\n");
        return FILE_ERROR;
    }

    fclose(file);

    debug_print(GREEN, "file %s successfully created!\n", filename);
    debug_print(GREEN, "copied %u bytes out of the expected %u\n\n", b_w, image.components * image.width * image.height);

    return NO_ERROR;
}

void flip_image_horizontally(Image image) {
    unsigned int half_image_size = (unsigned int) floorl(image.size / 2.0L);
    for (unsigned int i = 0; i < half_image_size; ++i) {
        unsigned char temp = image.decoded_data[i];
        image.decoded_data[i] = image.decoded_data[image.size - i];
        image.decoded_data[image.size - i] = temp;
    }
    return;
}

void flip_image_vertically(Image image) {
    for (unsigned int h = 0; h < image.height; ++h) {
        unsigned int half_width_size = (unsigned int) floorl(image.width / 2.0L);
        for (unsigned int w = 0; w < half_width_size; ++w) {
            unsigned int first = ((h * image.width) + w) * 3;
            unsigned int second = (((h + 1) * image.width) - (w + 1)) * 3;
            for (unsigned char i = 0; i < image.components; ++i) {
                unsigned char temp = image.decoded_data[first + i];
                image.decoded_data[first + i] = image.decoded_data[second + i];
                image.decoded_data[second + i] = temp;
            }
        }
    }
    return;
}

void deallocate_image(Image image) {
    debug_print(BLUE, "deallocating image...\n");
    free(image.decoded_data);
    return;
}

#endif //_NO_LIBRARY

#endif //_IMAGE_IO_H_