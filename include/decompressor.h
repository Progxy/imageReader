#ifndef _DECOMPRESSOR_H_
#define _DECOMPRESSOR_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"

// void deflate(BitStream* bit_stream);

// static void update_adler_crc(unsigned char value, unsigned int* adler_register) {
//     const unsigned int PRIME = 65521L;
//     unsigned int low = (*adler_register) & 0X0000FFFFL;
//     unsigned int high = ((*adler_register) >> 16) & 0X0000FFFFL;
//     low = (low + value) % PRIME;
//     high = (low + high) % PRIME;
//     *adler_register = (high << 16) | low ;
//     return;
// }

// void deflate(BitStream* bit_stream) {
//     unsigned int adler_register = 1;
//     unsigned char* decompressed_data;
//     unsigned int decompressed_index;
//     unsigned char* sliding_window;

//     return;
// }

#endif //_DECOMPRESSOR_H_