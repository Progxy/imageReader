#define _IMAGE_IO_IMPLEMENTATION_
#include "./image_io.h"

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

void flip_image_vertically(Image image) {
    unsigned int half_image_size = (unsigned int) floorl(image.size / 2.0L);
    for (unsigned int i = 0; i < half_image_size; ++i) {
        unsigned char temp = image.decoded_data[i];
        image.decoded_data[i] = image.decoded_data[image.size - i];
        image.decoded_data[image.size - i] = temp;
    }
    return;
}

void flip_image_horizontally(Image image) {
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
