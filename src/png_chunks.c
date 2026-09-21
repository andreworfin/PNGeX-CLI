#include "png_chunks.h"
#include "util.h"
#include <stdlib.h>
#include <string.h> //I added this for strncmp as allowed 

/* Parse IHDR data from chunk */
int png_parse_ihdr(const png_chunk_t *chunk, png_ihdr_t *out)
{
    if((!chunk) || (!out) || (strncmp(chunk->type, "IHDR\0", 5)) || (chunk->length != 13)){
        return -1;
    }
    png_ihdr_t ihdrChunk = {read_u32_be(chunk->data), read_u32_be((chunk->data) + 4), 
        *(chunk->data + 8), *(chunk->data + 9), *(chunk->data + 10), *(chunk->data + 11), *(chunk->data + 12)};
    *out = ihdrChunk;
    return 0;
}

/* Parse PLTE data from chunk into an allocated array of colors */
int png_parse_plte(const png_chunk_t *chunk, png_color_t **out_colors, size_t *out_count)
{
    if((!chunk) || (!out_colors) || (!out_count) || (strncmp(chunk->type, "PLTE\0", 5)) || (chunk->length % 3)){
        return -1;
    }
    size_t length = (size_t)chunk->length;
    length /= 3;

    if(1 > length || length > 256){
        return -1;
    }
    
    png_color_t *colors = malloc(length * sizeof(png_color_t));
    if (!colors) {
        return -1;
    }
    
    for(int i = 0; i < length; i++){
        (colors + i)->r = *(chunk->data + (i * 3));
        (colors + i)->g = *(chunk->data + (i * 3) + 1);
        (colors + i)->b = *(chunk->data + (i * 3) + 2);
    }
    *out_colors = colors;
    *out_count = length;
    return 0;
}