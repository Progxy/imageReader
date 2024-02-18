#ifndef _DCT_H_
#define _DCT_H_

#include <stdlib.h>

void mul_mat(int** mat, long double* mat_b, long double* mat_c, unsigned int size) {
    int* temp = (int*) calloc(size * size, sizeof(int));
    
    for (unsigned int i = 0; i < size; ++i) {
        for (unsigned int r = 0; r < size; ++r) {
            long double sum = 0.0L;
            for (unsigned char c = 0; c < size; ++c) {
                sum += mat_b[i * size + c] * (*mat)[c * size + r];
            }
            
            for (unsigned int s = 0; s < size; ++s) {
                temp[i * size + s] += sum * mat_c[r * size + s];
            }
        }
    }
    
    // Deallocate the original matrix
    free(*mat);
    *mat = temp;

    return;
}

#endif //_DCT_H_