#ifndef _MCU_H_
#define _MCU_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "./types.h"
#include "./dct.h"

#define COMPUTE_IDCT(data_unit, t_m, m) mul_mat(data_unit, t_m, m, 8)
#define PI 3.14159265358979323846L 
#define SQRT2 1.4142135623730951L

const unsigned char zigzag[64] = {
    0, 1, 5, 6, 14, 15, 27, 28,
    2, 4, 7, 13, 16, 26, 29, 42,
    3, 8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63,
};

void unzigzag_vec(int** data) {
    int* temp_vec = (int*) calloc(64, sizeof(int));

    // Unzigzag the matrix
    for (unsigned int i = 0; i < 64; ++i) {
        temp_vec[i] = (*data)[zigzag[i]];
    }

    int* temp = *data;
    *data = temp_vec;

    // Deallocate the vectors
    free(temp);

    return;
}

void dequantize_data_unit(int* data_unit, unsigned char* quantization_table) {
    for (unsigned char i = 0; i < 64; ++i) {
        data_unit[i] *= quantization_table[i];
    }
    return;
}

long double* generate_m() {
    long double* m = (long double*) calloc(64, sizeof(long double));
    const long double first_row_value = 1.0L / (2.0L * SQRT2);

    for (unsigned char v = 0; v < 8; ++v) {
        m[v] = first_row_value; 
    }

    for (unsigned char u = 1; u < 8; ++u) {
        for (unsigned char v = 0; v < 8; ++v) {
            m[u * 8 + v] = 0.5L * cosl(((2.0L * v + 1.0L) * PI * u) / 16.0L);
        }
    }

    return m;
}

long double* generate_tm(long double* m) {
    long double* t_m = (long double*) calloc(64, sizeof(long double));
    
    for (unsigned char i = 0; i < 8; ++i) {
        for (unsigned char l = 0; l < 8; ++l) {
            t_m[i * 8 + l] = m[l * 8 + i];
        }
    }
    
    return t_m;
}

long double round_colour(long double val) {
    return ceill(val + 0.5L);
}

void ycbcr_to_rgb(int* y, int* cb, int* cr, RGB* rgb) {
    // Convert the values from YCbCr to RGB
    // Clip the value between 0 and 255
    for (unsigned char i = 0; i < 64; ++i) {
        y[i] += 128;
        int R = round_colour(y[i] + (1.40200L * cr[i]));
        int G = round_colour(y[i] - (0.344136L * cb[i]) - (0.714136L * cr[i]));
        int B = round_colour(y[i] + (1.77200L * cb[i]));
        (rgb -> R)[i] = (unsigned char) CLAMP(R, 0, 255);
        (rgb -> G)[i] = (unsigned char) CLAMP(G, 0, 255);
        (rgb -> B)[i] = (unsigned char) CLAMP(B, 0, 255);
    }
    return;
}

void ycbcr_to_greyscale(int* y, RGB* rgb) {
    for (unsigned char j = 0; j < 64; ++j) {
        (rgb -> R)[j] = CLAMP(y[j] + 128, 0, 255);
        (rgb -> G)[j] = CLAMP(y[j] + 128, 0, 255);
        (rgb -> B)[j] = CLAMP(y[j] + 128, 0, 255);
    }
    return;
}

RGB* mcu_to_rgb(MCU mcu) {
    // Init RGB values
    RGB* rgb = (RGB*) calloc(mcu.max_du, sizeof(RGB));
    for (unsigned char i = 0; i < mcu.max_du; ++i) {
        rgb[i].R = (unsigned char*) calloc(64, sizeof(unsigned char));
        rgb[i].G = (unsigned char*) calloc(64, sizeof(unsigned char));
        rgb[i].B = (unsigned char*) calloc(64, sizeof(unsigned char));
    }

    // Convert YCbCr to RGB
    for (unsigned char i = 0; i < mcu.max_du; ++i) {
        if (mcu.components == 1) {
            ycbcr_to_greyscale(mcu.data_units[i % mcu.comp_du_count[0]], rgb + i);
            continue;
        }
        ycbcr_to_rgb(mcu.data_units[i % mcu.comp_du_count[0]], mcu.data_units[mcu.comp_du_count[0] + (i % mcu.comp_du_count[1])], mcu.data_units[mcu.comp_du_count[0] + mcu.comp_du_count[1] + (i % mcu.comp_du_count[2])], rgb + i);
    }

    return rgb;
}

void decode_mcu(MCU mcu, DataTables* data_table, long double* t_m, long double* m) {
    unsigned int mcu_count = 0;

    for (unsigned char comp_id = 0; comp_id < mcu.components; ++comp_id) {
        for (unsigned char j = 0; j < mcu.comp_du_count[comp_id]; ++j, ++mcu_count) {
            unsigned char qt_id = (data_table -> components)[comp_id].qt_id;
            unsigned char* quantization_table = (data_table -> qt_tables)[qt_id].data;
            
            // Dequantize the data units
            dequantize_data_unit(mcu.data_units[mcu_count], quantization_table);

            // Unzigzag data unit
            unzigzag_vec(mcu.data_units + mcu_count);
            
            // Calculate the IDCT for each data units
            COMPUTE_IDCT(mcu.data_units + mcu_count, t_m, m);
        }
    }

    return;
}

    // for (unsigned int i = 0; i < pixels -> mcu_y; ++i) {
    //     for (unsigned int j = 0; j < pixels -> mcu_x; ++j) {
    //         for (unsigned char du_index = 0; du_index < pixels -> du_count; ++du_index) {
    //             for (unsigned char x = 0; x < 8; ++x) { 
    //                 for (unsigned char y = 0; y < 8; ++y) {
    //                     unsigned int p_x = (j * 8 * pixels -> du_x) + (du_index % 2) * 8 + x + offset_x + j * 5; 
    //                     unsigned int p_y = (i * 8 * pixels -> du_y) + (du_index > 1) * 8 + y + offset_y + i * 5;
    //                     // Set the color (RGBA) for the pixel
    //                     cairo_set_source_rgba(cr, (pixels -> pixels_data)[p_index] / 255.0, (pixels -> pixels_data)[p_index + 1] / 255.0, (pixels -> pixels_data)[p_index + 2] / 255.0, 1.0);
    //                     // Draw a filled rectangle (pixel) at position (x, y) with a width and height of 1
    //                     cairo_rectangle(cr, p_x, p_y, 1.0, 1.0);
    //                     cairo_fill(cr);
    //                     p_index += 3;
    //                 }
    //             }
    //         }
    //     }
    // }

unsigned char mcus_to_image(Image* image, DataTables* data_table) {
    MCU* mcus = image -> mcus;
    image -> decoded_data = (unsigned char*) calloc(3 * (image -> width * image -> height + 1), sizeof(unsigned char));
    image -> size = 0;

    if (image -> mcu_x * image -> mcu_y > image -> mcu_count) {
        error_print("invalid mcu_count size: %u, expected: %u\n", image -> mcu_count, image -> mcu_x * image -> mcu_y);
        return TRUE;
    }

    RGB** rgbs = (RGB**) calloc(image -> mcu_count, sizeof(RGB*));
    for (unsigned int i  = 0; i < image -> mcu_count; ++i) {
        rgbs[i] = mcu_to_rgb(mcus[i]);
    }

    // Recalculate mcu_x
    unsigned int mcu_x = (image -> width - (image -> width % (8 * data_table -> max_sf_h))) / (8 * data_table -> max_sf_h); 

    for (unsigned int h = 0; h < image -> height; ++h) {
        unsigned int r = (h - (h % (8 * data_table -> max_sf_v))) / (8 * data_table -> max_sf_v);
        unsigned int h1 = h - (8 * r * data_table -> max_sf_v);
        unsigned int du_y = (h1 - (h1 % 8)) / 8;
        for (unsigned int w = 0; w < image -> width; ++w) {
            unsigned int c = (w - (w % (8 * data_table -> max_sf_h))) / (8 * data_table -> max_sf_h);
            unsigned int w1 = w - (8 * c * data_table -> max_sf_h);
            unsigned int du_x = (w1 - (w1 % 8)) / 8;
            (image -> decoded_data)[image -> size] = rgbs[r * mcu_x + c][du_y * data_table -> max_sf_h + du_x].R[8 * (h1 % 8) + (w1 % 8)];
            (image -> decoded_data)[image -> size + 1] = rgbs[r * mcu_x + c][du_y * data_table -> max_sf_h + du_x].G[8 * (h1 % 8) + (w1 % 8)];
            (image -> decoded_data)[image -> size + 2] = rgbs[r * mcu_x + c][du_y * data_table -> max_sf_h + du_x].B[8 * (h1 % 8) + (w1 % 8)];
            (image -> size) += 3;
        }
    }

    // Resize the decoded data
    image -> decoded_data = (unsigned char*) realloc(image -> decoded_data, image -> size);
    free(rgbs);

    return FALSE;
}

void deallocate_mcu(MCU mcu) {
    for (unsigned char j = 0; j < mcu.data_units_count; ++j) {
        free(mcu.data_units[j]);
    }
    free(mcu.data_units);
    return;
}

void deallocate_mcus(Image* image) {
    for (unsigned int i = 0; i < image -> mcu_count; ++i) {
        deallocate_mcu((image -> mcus)[i]);
    }
    free(image -> mcus);
    return;
}

#endif // _MCU_H_
