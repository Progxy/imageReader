# IDL: Image Decoding Library

Image file types supported on reading mode:
- JPEG: baseline;
- PNG: all bit-depths (NO INTERLACING);
- PPM: P6 header.

Supports only PPM file type on writing mode.

## General Notes
  - To understand how to use this library use as a reference the example in `image.c`, where is implemented a simple image viewer (for LINUX) using `gtk`.
  - You can also see the Python implementation in the `python` folder.
  - Remember to create the `out` directory before compiling.
  - The library is OS independent.

## Compile using the library as a shared library
Compile using the `idl` option with makefile

When compiling using `IDL` as a shared library remember to add the following flags to include it: `L. -lidl`

NOTE: remember to add the path to the library to the `LD_LIBRARY_PATH` variable on Linux

NOTE: remember to define `_USE_IMAGE_LIBRARY_` before including `image_io.h`, or you'll not be able to use the `idl` types
