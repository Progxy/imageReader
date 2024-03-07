#ifndef _TYPES_H_
#define _TYPES_H_

typedef enum ImageError {NO_ERROR, FILE_NOT_FOUND, INVALID_FILE_TYPE, FILE_ERROR, INVALID_MARKER_LENGTH, INVALID_QUANTIZATION_TABLE_NUM, INVALID_HUFFMAN_TABLE_NUM, INVALID_IMAGE_SIZE, EXCEEDED_LENGTH, UNSUPPORTED_JPEG_TYPE, INVALID_DEPTH_COLOR_COMBINATION, INVALID_CHUNK_LENGTH, INVALID_COMPRESSION_METHOD, INVALID_FILTER_METHOD, INVALID_INTERLACE_METHOD, INVALID_IEND_CHUNK_SIZE, DECODING_ERROR} ImageError; 
typedef enum Colors {RED = 31, GREEN, YELLOW, BLUE, PURPLE, CYAN, WHITE} Colors;
typedef enum JPEGType {BASELINE, SEQUENTIAL_EXTENDED_HUFFMAN, PROGRESSIVE_HUFFMAN, LOSSLESS_HUFFMAN, DIFFERENTIAL_SEQUENTIAL_EXTENDED_HUFFMAN = 5, DIFFERENTIAL_PROGRESSIVE_HUFFMAN, DIFFERENTIAL_LOSSLESS_HUFFMAN, SEQUENTIAL_EXTENDED_ARITHMETIC = 9, PROGRESSIVE_ARITHMETIC, LOSSLESS_ARITHMETIC, DIFFERENTIAL_SEQUENTIAL_EXTENDED_ARITHMETIC = 13, DIFFERENTIAL_PROGRESSIVE_ARITHMETIC, DIFFERENTIAL_LOSSLESS_ARITHMETIC} JPEGType;
typedef enum PNGType {GREYSCALE = 0, TRUECOLOR = 2, INDEXED_COLOR = 3, GREYSCALE_ALPHA = 4, TRUECOLOR_ALPHA = 6} PNGType;
typedef enum FileType {JPEG, PNG, PPM} FileType;
typedef unsigned char bool;

const char* err_codes[] = {"NO_ERROR", "FILE_NOT_FOUND", "INVALID_FILE_TYPE", "FILE_ERROR", "INVALID_MARKER_LENGTH", "INVALID_QUANTIZATION_TABLE_NUM", "INVALID_HUFFMAN_TABLE_NUM", "INVALID_IMAGE_SIZE", "EXCEEDED_LENGTH", "UNSUPPORTED_JPEG_TYPE", "INVALID_DEPTH_COLOR_COMBINATION", "INVALID_CHUNK_LENGTH", "INVALID_COMPRESSION_METHOD", "INVALID_FILTER_METHOD", "INVALID_INTERLACE_METHOD", "INVALID_IEND_CHUNK_SIZE", "DECODING_ERROR"};
const char* jpeg_types[] = {"BASELINE", "SEQUENTIAL_EXTENDED_HUFFMAN", "PROGRESSIVE_HUFFMAN", "LOSSLESS_HUFFMAN", "", "DIFFERENTIAL_SEQUENTIAL_EXTENDED_HUFFMAN", "DIFFERENTIAL_PROGRESSIVE_HUFFMAN", "DIFFERENTIAL_LOSSLESS_HUFFMAN", "", "SEQUENTIAL_EXTENDED_ARITHMETIC", "PROGRESSIVE_ARITHMETIC", "LOSSLESS_ARITHMETIC", "", "DIFFERENTIAL_SEQUENTIAL_EXTENDED_ARITHMETIC", "DIFFERENTIAL_PROGRESSIVE_ARITHMETIC", "DIFFERENTIAL_LOSSLESS_ARITHMETIC"};
const char* png_types[] = {"GREYSCALE", "", "TRUECOLOR/RGB_TRIPLE", "INDEXED_COLOR/PALETTE", "GREYSCALE_ALPHA", "", "TRUECOLOR_ALPHA/RGB_TRIPLE_ALPHA"};
const char* file_types[] = {"JPEG", "PNG", "PPM"};

const JPEGType type_supported[] = { BASELINE };
const unsigned char type_supported_count = 1;

typedef struct MCU {
    int** data_units;
    unsigned char components;
    unsigned char data_units_count; // Total number of data units
    unsigned char* comp_du_count; // Number of data units per component
    unsigned char max_du;
} MCU;

typedef struct HuffmanData {
    unsigned char last_k;
    unsigned char* hf_lengths;
    unsigned char* hf_values;
    short int* min_codes;
    short int* max_codes;
    short int* val_ptr;
    short int* huff_size;
    short int* huff_codes;
} HuffmanData;

typedef struct BitStream {
    unsigned char* stream;
    unsigned int byte;
    unsigned char bit;
    unsigned int size;
    unsigned char current_byte;
    ImageError error;
} BitStream;

typedef struct Component {
    unsigned char dc_table_id;
    unsigned char ac_table_id;
    unsigned char qt_id;
    unsigned char sampling_factor_v;
    unsigned char sampling_factor_h;
    unsigned char id;
    int pred;
} Component;

typedef struct QuantizationTable {
    unsigned char id;
    unsigned char* data;
} QuantizationTable;

typedef struct FileData {
    unsigned int length;
    unsigned char* data;
    FileType file_type;
} FileData;

typedef struct DataTables {
    HuffmanData* hf_dc;
    HuffmanData* hf_ac;
    unsigned char hf_dc_count;
    unsigned char hf_ac_count;
    QuantizationTable* qt_tables; // Vector of Quantization Tables
    unsigned char qt_count;
    Component* components;
    unsigned char components_count;
    unsigned char max_sf_h;
    unsigned char max_sf_v;
    unsigned char** sampling_factors;
    unsigned char sf_count;
    unsigned char* comp_du_count;
    unsigned char max_du;
} DataTables;

typedef struct MarkersTable {
    unsigned int* positions;
    unsigned char* marker_type;
    unsigned int count;
} MarkersTable;

typedef struct RGBA {
    unsigned char* R;
    unsigned char* G;
    unsigned char* B;
    unsigned char* A;
} RGBA;

typedef RGBA RGB;
typedef struct Image {
    unsigned int width;
    unsigned int height;
    unsigned char* decoded_data;
    unsigned int size;
    unsigned char components;
    ImageError error;
} Image;

typedef struct JPEGImage {
    Image image_data;
    BitStream* bit_stream;
    FileData image_file;
    MCU* mcus;
    unsigned int mcu_count;
    unsigned short int mcu_per_line;
    int is_exif;
    unsigned int mcu_x;
    unsigned int mcu_y;
    JPEGType jpeg_type;
} JPEGImage;

typedef struct Chunk {
    unsigned int pos;
    unsigned char chunk_type[5];
    unsigned int length;
} Chunk;

typedef struct Chunks {
    Chunk* chunks;
    unsigned int chunks_count;
    unsigned char invalid_chunks;
} Chunks;

typedef struct DynamicHF {
    unsigned short int* codes;
    unsigned char* lengths;
    unsigned short int size;
    unsigned char bit_length;
} DynamicHF; 

typedef struct PNGImage {
    Image image_data;
    BitStream* bit_stream;
    unsigned char bit_depth;
    PNGType color_type;
    unsigned char compression_method;
    unsigned char filter_method;
    unsigned char interlace_method;
    RGB palette;
    unsigned char filter_interval;
} PNGImage;

typedef struct PPMImage {
    Image image_data;
    BitStream* bit_stream;
} PPMImage;

#define SET_COLOR(color) printf("\033[%d;1m", color)
#define RESET_COLOR() printf("\033[0m")
#define FALSE 0
#define TRUE 1
#define DC 0
#define AC 1

#endif //_TYPES_H_