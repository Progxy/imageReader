# Image decoding library

Image file types supported on reading mode: 
- JPEG: baseline;
- PNG: all bit-depths (NO INTERLACING);
- PPM: P6 header.

Supports only PPM file type on writing mode.

NOTE: to understand how to use this library use as a reference the example in `image.c`, where is implemented a simple image viewer (for LINUX) using `gtk`.

NOTE: the library is OS independent.