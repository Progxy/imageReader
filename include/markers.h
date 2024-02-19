#ifndef _MARKERS_H_
#define _MARKERS_H_

#include <stdio.h>
#include "./debug_print.h"
#include "./types.h"
#include "./bitstream.h"

static const char* markers_types[256] = {[0xC0] = "SOF0", "SOF1", "SOF2", "SOF3", "DHT", "SOF5", "SOF6", "SOF7", "JPG", "SOF9", "SOF10", "SOF11", "DAC", "SOF13", "SOF14", "SOF15", "RSTm0", "RSTm1", "RSTm2", "RSTm3", "RSTm4", "RSTm5", "RSTm6", "RSTm7", "SOI", "EOI", "SOS", "DQT", "DNL", "DRI", "DHP", "EXP", "APP0", "APP1", "APP2", "APP3", "APP4", "APP5", "APP6", "APP7", "APP8", "APP9", "APPA", "APPB", "APPC", "APPD", "APPE", "APPF", "JPG0", "JPG1", "JPG2", "JPG3", "JPG4", "JPG5", "JPG6", "JPG7", "JPG8", "JPG9", "JPGA", "JPGB", "JPGC", "JPGD", "COM"};
static const unsigned char markers_codes[] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xDA, 0xDB, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xFE};

bool is_marker(unsigned char data) {
    for (unsigned char i = 0; i < 35; ++i) {
        if (markers_codes[i] == data) {
            return TRUE;
        }
    }
    
    return FALSE;
}

unsigned short int get_marker_len(JPEGImage* image) {
    unsigned short int length = (get_next_byte_uc(image -> bit_stream) << 8) | get_next_byte_uc(image -> bit_stream); 

    // Check that the length is valid
    if (length < 2) {
        error_print("Invalid marker length!\n");
        return INVALID_MARKER_LENGTH | 0x8000;
    }

    debug_print(YELLOW, "Length: %d\n", length);

    return length;
}

MarkersTable get_markers(FileData* image_file) {
    // Init marker table
    MarkersTable markers_table;
    markers_table.count = 1;
    markers_table.marker_type = (unsigned char*) calloc(1, 1);
    markers_table.positions = (unsigned int*) calloc(1, sizeof(unsigned int));
    
    unsigned char* data = image_file -> data;
    unsigned int len = image_file -> length;

    unsigned char previous_data = 0;
    for (unsigned int i = 1; i < len; ++i) {
        previous_data = data[i - 1];
        unsigned char current_data = data[i];
        
        if ((current_data < 0xC0 || current_data == 0xFF) || (previous_data != 0xFF)) {
            continue;
        }

        // Store the marker data
        markers_table.marker_type[markers_table.count - 1] = current_data;
        markers_table.positions[markers_table.count - 1] = i + ((current_data == 0xD9) ? 0 : 1);
        markers_table.count++;
        markers_table.marker_type = (unsigned char*) realloc(markers_table.marker_type, markers_table.count);
        markers_table.positions = (unsigned int*) realloc(markers_table.positions, markers_table.count * sizeof(unsigned int));

        // Skip the length of the marker
        if (is_marker(current_data) && (previous_data == 0xFF)) {
            debug_print(YELLOW, "skipping marker: %s\n", markers_types[current_data]);
            i += ((unsigned short int) (data[i + 1] << 8) | data[i + 2]) - 3;
            continue;
        }
    }

    return markers_table;
}

#endif //_MARKERS_H_