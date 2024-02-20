#ifndef _DECODE_JPEG_H_
#define _DECODE_JPEG_H_

#include <stdlib.h>
#include <stdio.h>
#include "./types.h"
#include "./image_io.h"
#include "./markers.h"
#include "./debug_print.h"
#include "./bitstream.h"
#include "./mcu.h"
#include "./decode_huff.h"

#define MARKER_FLAG(data, pos) ((data)[(pos)] == 0xFF)
#define RESET_ERROR_FLAG(image) (((image)-> image_data).error = 0)
#define CHECK_ERROR_FLAG(image) if (((image)-> image_data).error) return image -> image_data
#define MARKER_PREFIX_CODE 0xFF
#define MARKER_BASE_CODE 0xC0

static const char* component_types[] = {"Y", "Cb", "Cr", "I", "Q"};

/* -------------------------------------------------------------------------------------- */

static Component* get_component_by_id(DataTables* data_tables, unsigned char component_id);
static bool jpeg_type_is_supported(JPEGType jpeg_type);
static void decode_com(JPEGImage* image);
static void decode_app(JPEGImage* image, unsigned char marker_code);
static void decode_dqt(JPEGImage* image, DataTables* data_tables);
static unsigned char max_val(unsigned char* vec, unsigned char len);
static void decode_sof(JPEGImage* image, DataTables* data_tables, unsigned char marker_code);
static void decode_sos(JPEGImage* image, DataTables* data_tables);
static void decode_dri(JPEGImage* image);
static void decode_rst(JPEGImage* image, unsigned char interval_count, unsigned int data_len, DataTables* data_tables);
static void decode_dht(JPEGImage* image, DataTables* data_tables);
static void deallocate_data_table(DataTables* data_tables);
static void decode_data(JPEGImage* image, DataTables* data_tables, unsigned char* image_data, unsigned int image_size);
static DataTables* init_data_tables();
Image decode_jpeg(FileData* image_file);

/* -------------------------------------------------------------------------------------- */

static Component* get_component_by_id(DataTables* data_tables, unsigned char component_id) {
    Component* components = data_tables -> components;

    for (unsigned int i = 0; i < data_tables -> components_count; ++i, ++components) {
        if (components -> id == component_id) {
            return components;
        }
    }
    
    return NULL;
}

static bool jpeg_type_is_supported(JPEGType jpeg_type) {
    for (unsigned char i = 0; i < type_supported_count; ++i) {
        if (jpeg_type == type_supported[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

static void decode_com(JPEGImage* image) {
    BitStream* bit_stream = image -> bit_stream;
    debug_print(PURPLE, "COM marker found at byte: %d: \n", bit_stream -> byte);

    // Get the length of the marker section
    unsigned short length = get_marker_len(image);

    // Check for errors
    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }

    char* comment = (char*) calloc(1, 1);

    for (int i = 0; i < length - 2; ++i) {
        comment[i] = get_next_byte_uc(bit_stream);
        comment = (char*) realloc(comment, i + 2);
        comment[i + 1] = '\0';
    }

    debug_print(YELLOW, "Comment: %s\n", comment);

    // Deallocate the comment data
    free(comment);
    
    // Print the marker section
    print_line(bit_stream -> stream, bit_stream -> byte - length, length);

    return;
}

static void decode_app(JPEGImage* image, unsigned char marker_code) {
    BitStream* bit_stream = image -> bit_stream;

    debug_print(PURPLE, "APP%d marker found at byte: %d: \n", marker_code - 0xE0, bit_stream -> byte);

    unsigned int start = bit_stream -> byte;

    // Get the length of the marker section
    unsigned short length = get_marker_len(image);

    // Check for errors
    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }

    // Get the end of the current marker section by adding the length and subtracting 2 (the bytes that represent the length of the marker are counted inside the length) 
    unsigned int end_of_marker = bit_stream -> byte + length - 2;

    // Get the identifier
    char* identifier = (char*) calloc(1, 1);
    for (int i = 0; (get_next_byte(bit_stream), bit_stream -> current_byte) != 0; identifier[i] = bit_stream -> current_byte, ++i) {
        identifier = (char*) realloc(identifier, i + 2);
    }

    debug_print(YELLOW, "Identifier: %s\n", identifier);

    image -> is_exif = !strcmp("Exif", identifier);

    free(identifier);

    if (image -> is_exif) {
        debug_print(YELLOW, "Is Exif format skipping the Exif header!\n");
        set_byte(bit_stream, start + length);
        return;
    }

    debug_print(YELLOW, "Version: %d.%d\n", get_next_byte_uc(bit_stream), get_next_byte_uc(bit_stream));

    unsigned char units = get_next_byte_uc(bit_stream);
    debug_print(YELLOW, "Units: %s\n", units == 0 ? "NO_UNITS" : units == 1 ? "PPI" : "PPCM");

    unsigned short densityx = get_next_byte_uc(bit_stream) << 8 | get_next_byte_uc(bit_stream);

    unsigned short densityy = get_next_byte_uc(bit_stream) << 8 | get_next_byte_uc(bit_stream);

    debug_print(YELLOW, "Density: %ux%u\n", densityx, densityy);

    debug_print(YELLOW, "Thumbnail: %ux%u\n", get_next_byte_uc(bit_stream), get_next_byte_uc(bit_stream));

    // Set the ptr index to the end of the marker
    set_byte(bit_stream, end_of_marker);

    // Print the marker section
    print_line(bit_stream -> stream, bit_stream -> byte - length, length);

    return;
}

static void decode_dqt(JPEGImage* image, DataTables* data_tables) {
    BitStream* bit_stream = image -> bit_stream;
    debug_print(PURPLE, "DQT marker found at byte: %d: \n", bit_stream -> byte);

    // Get the length of the marker section
    unsigned short length = get_marker_len(image);

    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }

    unsigned int data_len = bit_stream -> byte + length - 2;
    
    while (bit_stream -> byte < data_len) {
        unsigned char dqt_info = get_next_byte_uc(bit_stream);
        unsigned char precision = (dqt_info >> 4) & 15;
        unsigned char dqt_id = dqt_info & 15;

        debug_print(YELLOW, "Precision: %s\n", precision ? "16-BIT" : "8-BIT");
        debug_print(YELLOW, "Number of QT: %d\n", dqt_id);

        // Check that the number is in range 0..3
        if (dqt_id > 3) {
            error_print("Invalid Quantization Table Number!\n");
            (image -> image_data).error = INVALID_QUANTIZATION_TABLE_NUM;
            return;
        }

        unsigned char* table = get_next_n_byte_uc(bit_stream, 64);    
        QuantizationTable qt_table = {.data = table, .id = dqt_id};

        // Store the quantization table data
        if (dqt_id <= data_tables -> qt_count) {
            debug_print(YELLOW, "dqt_id: %u out of %u\n", dqt_id, data_tables -> qt_count);
            (data_tables -> qt_tables)[dqt_id] = qt_table;
        } else {
            data_tables -> qt_tables = (QuantizationTable*) realloc(data_tables -> qt_tables, sizeof(QuantizationTable) * (dqt_id + 1));
            (data_tables -> qt_tables)[dqt_id] = qt_table;
            (data_tables -> qt_count) = dqt_id;
        }

        debug_print(YELLOW, "Quantization Table:\n");
        print_table(YELLOW, (data_tables -> qt_tables)[dqt_id].data);
    }
    
    // Print the marker section
    print_line(bit_stream -> stream, bit_stream -> byte - length, length);

    return;
}

static unsigned char max_val(unsigned char* vec, unsigned char len) {
    unsigned char max = 0;
    for (unsigned char i = 0; i < len; ++i) {
        max = max < vec[i] ? vec[i] : max;
    }
    return max;
}

static void decode_sof(JPEGImage* image, DataTables* data_tables, unsigned char marker_code) {
    BitStream* bit_stream = image -> bit_stream;
    debug_print(PURPLE, "SOF%d marker found at byte: %d!\n", marker_code - 0xC0, bit_stream -> byte);

    // Get the length of the marker section
    unsigned short length = get_marker_len(image);

    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }

    debug_print(BLUE, "JPEG type: %s\n", jpeg_types[image -> jpeg_type]);

    unsigned char precision = get_next_byte_uc(bit_stream);
    debug_print(YELLOW, "Precision: %d(Bits/Samples)\n", precision);

    (image -> image_data).height = (get_next_byte_uc(bit_stream) << 8) | get_next_byte_uc(bit_stream);
    debug_print(YELLOW, "Height: %d\n", (image -> image_data).height);
    
    // Check that the height is valid
    if ((image -> image_data).height <= 0) {
        error_print("Invalid image height!\n");
        (image -> image_data).error = INVALID_IMAGE_SIZE;
        return;
    }

    (image -> image_data).width = (get_next_byte_uc(bit_stream) << 8) | get_next_byte_uc(bit_stream);

    debug_print(YELLOW, "Width: %d\n", (image -> image_data).width);

    // Check that the height is valid
    if ((image -> image_data).width <= 0) {
        error_print("Invalid image width!\n");
        (image -> image_data).error = INVALID_IMAGE_SIZE;
        return;
    }

    (image -> image_data).components = get_next_byte_uc(bit_stream);
    debug_print(YELLOW, "Components: %d, %s\n", (image -> image_data).components, (image -> image_data).components == 1 ? "GREY SCALED" : "YCBCR or YIQ");

    for (unsigned int i = 0; i < (image -> image_data).components; ++i) {
        Component component;
        component.pred = 0;
        component.id = get_next_byte_uc(bit_stream);
        debug_print(YELLOW, "Component_id: %d, %s\n", component.id, component_types[component.id]);
    
        component.sampling_factor_h = (get_next_byte_uc(bit_stream) >> 4) & 15;
        component.sampling_factor_v = (bit_stream -> current_byte) & 15;

        debug_print(YELLOW, "Sampling factor: %dx%d\n", component.sampling_factor_h, component.sampling_factor_v);

        component.qt_id = get_next_byte_uc(bit_stream);

        debug_print(YELLOW, "Quantization Table Number: %d\n", component.qt_id);

        // Store the component data
        data_tables -> components = (Component*) (!(data_tables -> components_count) ? calloc(1, sizeof(Component)) : realloc(data_tables -> components, sizeof(Component) * (data_tables -> components_count + 1)));
        data_tables -> components_count++; 
        (data_tables -> components)[data_tables -> components_count - 1] = component;
    }

    unsigned char** sampling_factors = (unsigned char**) calloc((image -> image_data).components, sizeof(unsigned char*));
    unsigned char* comp_du_count = (unsigned char*) calloc((image -> image_data).components, sizeof(unsigned char));
    unsigned char max_sf_h = 0;
    unsigned char max_sf_v = 0;
    unsigned char sf_count = 0;

    // Store the sampling factors
    for (unsigned char i = 0; i < (image -> image_data).components; ++i) {
        sampling_factors[i] = (unsigned char*) calloc(2, sizeof(unsigned char));
        sampling_factors[i][0] = (data_tables -> components[i]).sampling_factor_h;
        sampling_factors[i][1] = (data_tables -> components[i]).sampling_factor_v;
        if (max_sf_h < sampling_factors[i][0]) {
            max_sf_h = sampling_factors[i][0];
        }
        
        if (max_sf_v < sampling_factors[i][1]) {
            max_sf_v = sampling_factors[i][1];
        }

        comp_du_count[i] = sampling_factors[i][0] * sampling_factors[i][1];
        sf_count += comp_du_count[i];
    }

    data_tables -> sampling_factors = sampling_factors;
    data_tables -> comp_du_count = comp_du_count;
    data_tables -> max_du = max_val(comp_du_count, (image -> image_data).components);
    data_tables -> max_sf_h = max_sf_h;
    data_tables -> max_sf_v = max_sf_v;
    data_tables -> sf_count = sf_count;

    // Print the marker section
    print_line(bit_stream -> stream, bit_stream -> byte - length, length);

    return;
}

static void decode_sos(JPEGImage* image, DataTables* data_tables) {
    BitStream* bit_stream = image -> bit_stream;
    debug_print(PURPLE, "SOS marker found at byte: %d: \n", bit_stream -> byte);

    // Get the length of the marker section
    unsigned short length = get_marker_len(image);

    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }
    
    unsigned char components = get_next_byte_uc(bit_stream);

    debug_print(YELLOW, "Components: %d\n", components);

    for (int i = 0; i < components; ++i) { 
        unsigned char component_id = get_next_byte_uc(bit_stream);
        unsigned char dc_table = (get_next_byte_uc(bit_stream) >> 4) & 15;
        unsigned char ac_table = (bit_stream -> current_byte) & 15;

        Component* component = get_component_by_id(data_tables, component_id);
        
        if (component == NULL) {
            error_print("INVALID_COMPONENT: invalid component_id\n");
            continue;
        }

        component -> dc_table_id = dc_table;
        component -> ac_table_id = ac_table;

        debug_print(YELLOW, "Component id: %d, DC: %d, AC: %d\n", component_id, dc_table, ac_table);
    }

    unsigned char spectral_start = get_next_byte_uc(bit_stream);
    unsigned char spectral_end = get_next_byte_uc(bit_stream);
    debug_print(YELLOW, "Spectral selector %u..%u\n", spectral_start, spectral_end);
    debug_print(YELLOW, "Successive approx.: ");
    print_hex(YELLOW, (get_next_byte_uc(bit_stream) >> 4) & 15);
    debug_print(YELLOW, "-");
    debug_print(YELLOW, " ");
    print_hex(YELLOW, (bit_stream -> current_byte) & 15);
    debug_print(YELLOW, "\n");

    // Print the marker section (without the extra byte as it's not an FF of the next marker)
    print_line(bit_stream -> stream, bit_stream -> byte - length, length - 1);

    unsigned char* compressed_data = (unsigned char*) calloc(1, sizeof(unsigned char));
    unsigned int index = 0;

    for (unsigned char previous_data = 0, current_data = 0; (current_data < MARKER_BASE_CODE || current_data == MARKER_PREFIX_CODE) || (previous_data != MARKER_PREFIX_CODE);) {
        previous_data = current_data;
        current_data = get_next_byte_uc(bit_stream);
        compressed_data = (unsigned char*) realloc(compressed_data, sizeof(unsigned char) * (index + 1));
        compressed_data[index] = current_data;
        index++;
    }

    // Remove the 0xFFXX of the marker if not 0xFFDC
    if (compressed_data[index - 1] != 0xDC) {
        compressed_data = (unsigned char*) realloc(compressed_data, index - 1);
        index -= 2;
    }

    // Decode the unstuffed data
    decode_data(image, data_tables, compressed_data, index);

    return;
}

static void decode_dri(JPEGImage* image) {
    BitStream* bit_stream = image -> bit_stream;
    debug_print(PURPLE, " DRI marker found at byte: %d: \n", (bit_stream) -> byte);
    
    // Get the length of the marker section
    unsigned short length = get_marker_len(image); 
    
    // Get the number of mcu per line of sample
    image -> mcu_per_line = (get_next_byte_uc(bit_stream) << 8) + get_next_byte_uc(bit_stream);
    debug_print(YELLOW, "%u MCUs for each line of the sample\n", image -> mcu_per_line);
    
    // Print the marker section
    print_line(bit_stream -> stream, bit_stream -> byte - 8, 16);

    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }   

    return;
}

static void decode_rst(JPEGImage* image, unsigned char interval_count, unsigned int data_len, DataTables* data_tables) {
    BitStream* bit_stream = image -> bit_stream;

    debug_print(PURPLE, "RST%d marker found at byte: %d: \n", interval_count, bit_stream -> byte);
    debug_print(YELLOW, "Decoding %u mcus for each of the %u components\n", image -> mcu_per_line, (image -> image_data).components);

    // Reset the pred for each component
    for (unsigned char i = 0; i < (image -> image_data).components; ++i) {
        (data_tables -> components)[i].pred = 0;
    }

    // Retrieve data and decode it
    unsigned char* data_stream = get_next_n_byte_uc(bit_stream, data_len);
    decode_data(image, data_tables, data_stream, data_len);

    return;
}

static void decode_dht(JPEGImage* image, DataTables* data_tables) {
    BitStream* bit_stream = image -> bit_stream;
    debug_print(PURPLE, "DHT marker found at byte: %d: \n", bit_stream -> byte);

    unsigned int st = bit_stream -> byte;
    // Get the length of the marker section
    unsigned short length = get_marker_len(image);

    if (length & 0x8000) {
        (image -> image_data).error = length & 0x8008;
        return;
    }   

    unsigned int data_len = bit_stream -> byte + length - 2;

    while (bit_stream -> byte < data_len) {
        // Initialize the Huffman Data
        HuffmanData hf_data = {};

        // Store the info of the table
        unsigned char ht_info = get_next_byte_uc(bit_stream);
        unsigned char id = (ht_info) & 15;
        unsigned char hf_type = (ht_info >> 4) & 15;

        debug_print(YELLOW, "HT id: %d\n", id);
        debug_print(YELLOW, "HT type: %s\n", !(hf_type) ? "DC Table" : "AC Table");

        // Check that the number is in range 0..3
        if (id > 3) {
            error_print("Invalid Huffman Table!\n");
            (image -> image_data).error = INVALID_HUFFMAN_TABLE_NUM;
            return;
        }

        // Get the number of huffmans codes for each code length (1 bit to 16 bit long)
        hf_data.hf_lengths = get_next_n_byte_uc(bit_stream, 16);
        debug_print(YELLOW, "Lengths: ");
        for (unsigned int i = 0; i < 16; ++i) {
            print_hex(YELLOW, hf_data.hf_lengths[i]);
        }
        debug_print(YELLOW, "\n");

        // Get the sum of all the lengths
        unsigned char values_len = 0;
        for (unsigned char i = 0; i < 16; ++i) {
            values_len += hf_data.hf_lengths[i];
        }
        
        // Get the huffman values
        hf_data.hf_values = get_next_n_byte_uc(bit_stream, values_len);
        debug_print(YELLOW, "Codes: ");
        for (int i = 0; i < values_len; ++i) {
            print_hex(YELLOW, hf_data.hf_values[i]);
        }
        debug_print(YELLOW, "\n");

        debug_print(YELLOW, "%u values\n\n", values_len);
        hf_data.last_k = values_len;

        hf_data.min_codes = (short int*) calloc(16, sizeof(short int));
        hf_data.max_codes = (short int*) calloc(16, sizeof(short int));
        hf_data.val_ptr = (short int*) calloc(16, sizeof(short int));
        hf_data.huff_size = generate_huffsize(hf_data.hf_lengths);
        hf_data.huff_codes = generate_huffcode(hf_data.huff_size);

        decode_tables(hf_data);

        // Store the Huffman Data
        if (hf_type == DC) {
            if (id <= data_tables -> hf_dc_count) {
                (data_tables -> hf_dc)[id] = hf_data;
            } else {
                data_tables -> hf_dc = (HuffmanData*) realloc(data_tables -> hf_dc, sizeof(HuffmanData) * (id + 1));
                (data_tables -> hf_dc)[id] = hf_data;
                (data_tables -> hf_dc_count) = id;
            }
        } else {
            if (id <= data_tables -> hf_ac_count) {
                (data_tables -> hf_ac)[id] = hf_data;
            } else {
                data_tables -> hf_ac = (HuffmanData*) realloc(data_tables -> hf_ac, sizeof(HuffmanData) * (id + 1));
                (data_tables -> hf_ac)[id] = hf_data;
                (data_tables -> hf_ac_count) = id;
            }
        }
    }

    // Print the marker section
    print_line(bit_stream -> stream, st, length);

    return;
}

static void deallocate_data_table(DataTables* data_tables) {
    debug_print(BLUE, "deallocating data table...\n");

    // Deallocate DataTable
    free(data_tables -> components);
    free(data_tables -> qt_tables);
    free(data_tables -> comp_du_count);
    
    for (unsigned char i = 0; i < data_tables -> hf_ac_count; ++i) {
        free((data_tables -> hf_ac)[i].hf_lengths);
        free((data_tables -> hf_ac)[i].hf_values);
        free((data_tables -> hf_ac)[i].huff_codes);
        free((data_tables -> hf_ac)[i].huff_size);
        free((data_tables -> hf_ac)[i].max_codes);
        free((data_tables -> hf_ac)[i].min_codes);
        free((data_tables -> hf_ac)[i].val_ptr);
    }

    free(data_tables -> hf_ac);

    for (unsigned char i = 0; i < data_tables -> hf_dc_count; ++i) {
        free((data_tables -> hf_dc)[i].hf_lengths);
        free((data_tables -> hf_dc)[i].hf_values);
        free((data_tables -> hf_dc)[i].huff_codes);
        free((data_tables -> hf_dc)[i].huff_size);
        free((data_tables -> hf_dc)[i].max_codes);
        free((data_tables -> hf_dc)[i].min_codes);
        free((data_tables -> hf_dc)[i].val_ptr);
    }
    
    free(data_tables -> hf_dc);    
    
    // Deallocate sampling factors
    for (unsigned char i = 0; i < data_tables -> components_count; ++i) {
        free((data_tables -> sampling_factors)[i]);
    }
    free(data_tables -> sampling_factors);
    
    free(data_tables);

    return;
}

static void decode_data(JPEGImage* image, DataTables* data_tables, unsigned char* image_data, unsigned int image_size) {
    unsigned short int err = 0;
    unsigned char components = data_tables -> components_count;
    BitStream* bit_stream = allocate_bit_stream(image_data, image_size);

    if ((image -> image_data).components == 1) {
        unsigned int pixels_x = 8 * (data_tables -> max_sf_h / (data_tables -> components)[0].sampling_factor_h);
        unsigned int pixels_y = 8 * (data_tables -> max_sf_v / (data_tables -> components)[0].sampling_factor_v);
        image -> mcu_x = ((image -> image_data).width + pixels_x - 1) / pixels_x;
        image -> mcu_y = ((image -> image_data).height + pixels_y - 1) / pixels_y;
    } else {
        image -> mcu_x = ((image -> image_data).width + 8 * data_tables -> max_sf_h - 1) / (8 * data_tables -> max_sf_h); 
        image -> mcu_y = ((image -> image_data).height + 8 * data_tables -> max_sf_v - 1) / (8 * data_tables -> max_sf_v); 
    }

    debug_print(BLUE, "\n");
    debug_print(BLUE, "decoding data...\n");

    long double* m = generate_m();
    long double* t_m = generate_tm(m);
    unsigned int mcus_count = (image -> mcu_per_line) ? (image -> mcu_count + image -> mcu_per_line) : (image -> mcu_x * image -> mcu_y);

    // Decode all the MCUs inside the scan section 
    while (err != DNL_MARKER_DETECTED && (image -> mcu_count < mcus_count)) {
        image -> mcus = (MCU*) realloc(image -> mcus, sizeof(MCU) * (image -> mcu_count + 1));
        MCU mcu = generate_mcu(components, bit_stream, data_tables, &err);

        if (err == INVALID_BYTE_STUFFING) {
            error_print("Invalid byte stuffing at byte: %u\n", bit_stream -> byte);
            
            debug_print(YELLOW, "Near compressed data dump: ");
            for (unsigned int i = (bit_stream -> byte - 4); (i < bit_stream -> size) && (i < (bit_stream -> byte + 4)); ++i) {
                print_hex(YELLOW, (bit_stream -> stream)[i]);
            }
            
            debug_print(YELLOW, "\n");
            
            (image -> image_data).error = DECODING_ERROR;
            return;
        } else if (err == LENGTH_EXCEEDED) {
            decode_mcu(mcu, data_tables, t_m, m);
            (image -> mcus)[image -> mcu_count] = mcu;
            (image -> mcu_count)++;
            break;
        }

        decode_mcu(mcu, data_tables, t_m, m);
        (image -> mcus)[image -> mcu_count] = mcu;
        (image -> mcu_count)++;
    }

    debug_print(YELLOW, "Bitstream: byte: %u, bits: %u, out of %u\n", bit_stream -> byte, bit_stream -> bit, bit_stream -> size);
    deallocate_bit_stream(bit_stream);

    free(t_m);
    free(m);

    return;
}

static DataTables* init_data_tables() {
    DataTables* data_tables = (DataTables*) calloc(1, sizeof(DataTables));
    data_tables -> hf_dc = (HuffmanData*) calloc(1, sizeof(HuffmanData));
    data_tables -> hf_dc_count = 0;
    data_tables -> hf_ac = (HuffmanData*) calloc(1, sizeof(HuffmanData));
    data_tables -> hf_ac_count = 0;    
    data_tables -> qt_tables = (QuantizationTable*) calloc(1, sizeof(QuantizationTable));
    data_tables -> qt_count = 0;
    debug_print(BLUE, "init data table...\n");
    return data_tables;
}

Image decode_jpeg(FileData* image_file) {
    // Init image struct
    JPEGImage* image = (JPEGImage*) calloc(1, sizeof(JPEGImage));
    image -> image_file = *image_file;
    image -> mcu_count = 0;
    image -> mcus = (MCU*) calloc(1, sizeof(MCU));
    image -> bit_stream = allocate_bit_stream(image_file -> data, image_file -> length);
    image -> mcu_per_line = 0;
    
    // Init data tables
    DataTables* data_tables = init_data_tables();

    MarkersTable markers_table = get_markers(image_file);

    debug_print(BLUE, "File length: %u\n\n", image_file -> length);

    debug_print(BLUE, "Markers: \n");
    for (unsigned char i = 0; i < markers_table.count - 1; ++i) {
        debug_print(BLUE, "marker: ");
        print_hex(BLUE, markers_table.marker_type[i]);
        debug_print(BLUE, "- type: %s, position: %u\n", markers_types[markers_table.marker_type[i]], markers_table.positions[i]);
    }

    for (unsigned int i = 0; i < markers_table.count - 1; ++i) {
        if (image -> is_exif && (image -> bit_stream) -> byte > markers_table.positions[i] + 1) {
            continue;
        }

        if (!jpeg_type_is_supported(image -> jpeg_type)) {
            (image -> image_data).error = UNSUPPORTED_JPEG_TYPE;
            return image -> image_data;
        }

        // Set the position after the marker 
        set_byte(image -> bit_stream, markers_table.positions[i]);
        debug_print(YELLOW, "\n");

        unsigned char marker_type = markers_table.marker_type[i];

        switch (marker_type) {
            case 0xC0:   
            case 0xC1:   
            case 0xC2:
            case 0xC3: 
            case 0xC5:   
            case 0xC6:
            case 0xC7:
            case 0xC9:
            case 0xCA:
            case 0xCB:
            case 0xCD:
            case 0xCE:
            case 0xCF:
                image -> jpeg_type = marker_type - 0xC0;
                decode_sof(image, data_tables, marker_type);
                break;

            case 0xC4: 
                decode_dht(image, data_tables);
                break;

            case 0xDB: 
                decode_dqt(image, data_tables);
                break;
            
            case 0xDC:
                debug_print(PURPLE, "DNL marker found at byte: %d: \n", (image -> bit_stream) -> byte);
                debug_print(YELLOW, "length: %u\n", get_next_byte_uc(image -> bit_stream) | (get_next_byte_uc(image -> bit_stream) << 8));
                debug_print(YELLOW, "Height: %u\n", get_next_byte_uc(image -> bit_stream) | (get_next_byte_uc(image -> bit_stream) << 8));
                break;

            case 0xDD:
                decode_dri(image);
                break;

            case 0xDA: 
                decode_sos(image, data_tables);
                break;

            case 0xFE:
                decode_com(image);
                break;

            case 0xD9:
                debug_print(PURPLE, "EOI marker found at byte: %d!\n", (image -> bit_stream) -> byte);
                debug_print(YELLOW, "End of the image\n");
                break;

            default:
                if ((marker_type >= 0xD0) && (marker_type <= 0xD7)) {
                    decode_rst(image, marker_type - 0xD0, markers_table.positions[i + 1] - markers_table.positions[i] - 2, data_tables);
                } else if ((marker_type >= 0xE0) && (marker_type <= 0xEF)) {
                    decode_app(image, marker_type);
                }
                break;
        }
        
        CHECK_ERROR_FLAG(image);

    }

    debug_print(BLUE, "\n");
    debug_print(BLUE, "decoding image...\n");
    if (mcus_to_image(image, data_tables)) {
        (image -> image_data).error = 10;
        return image -> image_data;
    }

    debug_print(BLUE, "deallocating the mcus...\n");
    deallocate_mcus(image);
    deallocate_data_table(data_tables);
    deallocate_bit_stream(image -> bit_stream);

    debug_print(YELLOW, "\n");
    
    debug_print(BLUE, "size: %u\n", (image -> image_data).size);

    return (image -> image_data);
}

#endif //_DECODE_JPEG_H_