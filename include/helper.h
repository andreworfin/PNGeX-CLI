#ifndef HELPER_H
#define HELPER_H

#include "png_chunks.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>


/*Decompresses all IDAT chunks into a single buffer (moves file pointer)*/
int decompress(FILE *fp, uint8_t **out_data, size_t *out_size);

/*Opens output file*/
FILE *open_png_out(const char *output_path);

/*Writes to file in the correct edianess for uint32_t values*/
void write_u32(FILE *fp, uint32_t value);

/*Write output PNG file, preserving all chunks except IDAT (replaced) and PLTE (if modified)*/
/*Only frees if returns 0*/
int write_out(FILE *fp, FILE *fp_small, FILE *fp_out, uint8_t *idat_data, size_t idat_data_size, png_color_t **colors, int change_palette, size_t count);

/*Find the number of channels for the color_type*/
uint8_t number_channels(uint8_t color_type);

/*Unfilter scanlines to reconstruct actual pixel values (handles filter types 0-4)*/
int unfilter(uint8_t **orginal, uint8_t **unfiltered, uint32_t height, uint32_t width, uint8_t color_type);

#endif
