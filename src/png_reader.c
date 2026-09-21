#include "png_reader.h"
#include "png_crc.h"
#include "util.h"
#include "global.h"
#include <stdlib.h>
#include <string.h>

#define SIGNATURE_SIZE 8
#define PNG_LENGTH_SIZE 4
#define PNG_TYPE_SIZE 4
#define PNG_CRC_SIZE 4

/* Opens a PNG file and validates signature */
FILE *png_open(const char *path)
{

    FILE *fp = fopen(path, "rb");
    if(!fp){
        return NULL;
    }

    uint8_t signature[SIGNATURE_SIZE];
    if(read_exact(fp, signature, SIGNATURE_SIZE)){
        fclose(fp);
        return NULL;
    }

    if(memcmp(signature, (const uint8_t[]) {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}, SIGNATURE_SIZE)){
        fclose(fp);
        return NULL;
    }

    return (fp); //fread() in read_exact() should move the file pointer by amount read
}

/* Reads the next chunk from the file */
int png_read_chunk(FILE *fp, png_chunk_t *out)
{
    if((!fp) || (!out)){
        return -1;
    }
    out->data = NULL;

    uint8_t length_array[PNG_LENGTH_SIZE];
    if(read_exact(fp, length_array, PNG_LENGTH_SIZE)){
        return -1;
    }
    out->length = read_u32_be(length_array);

    if(read_exact(fp, (uint8_t *)(out->type), PNG_TYPE_SIZE)){
        return -1;
    }
    out->type[4] = '\0';


    out->data = malloc(out->length);
    if (out->data == NULL) {
        return -1;
    }

    if(read_exact(fp, out->data, out->length)){
        free(out->data); 
        out->data = NULL;
        return -1;
    }

    uint8_t crc_array[PNG_CRC_SIZE];
    if(read_exact(fp, crc_array, PNG_CRC_SIZE)){
        free(out->data); 
        out->data = NULL;
        return -1;
    }
    uint32_t stored_crc = read_u32_be(crc_array);

    size_t buf_len = PNG_TYPE_SIZE + out->length;
    uint8_t buf_array[buf_len];
    memcpy(buf_array, out->type, PNG_TYPE_SIZE);
    memcpy(buf_array + PNG_TYPE_SIZE, out->data, out->length); 
    uint32_t computed_crc = png_crc(buf_array, buf_len);

    out->crc = stored_crc;

    if (computed_crc != stored_crc) {
        out->crc = 0;
    }

    return 0;
}

/* Frees memory allocated inside png_chunk_t */
void png_free_chunk(png_chunk_t *chunk)
{
    if(!chunk){
        return;
    }
    if(chunk->data){
        free(chunk->data);
        chunk->data = NULL;
    }
}

int png_extract_ihdr(FILE *fp, png_ihdr_t *out)
{
    if((!fp) || (!out)){
        return -1;
    }
    png_chunk_t chunk = {0};

    if(png_read_chunk(fp, &chunk)){
        png_free_chunk(&chunk);
        return -1;
    }

    if(png_parse_ihdr(&chunk, out)){
        png_free_chunk(&chunk);
        return -1;
    }

    png_free_chunk(&chunk);
    return 0;
}

int png_extract_plte(FILE *fp, png_color_t **out_colors, size_t *out_count)
{
    if((!fp) || (!out_colors) || (!out_count)){
        return -1;
    }
    
    png_chunk_t chunk = {0};

    //Read chunks sequentially until PLTE chunk found
    char type[5];
    do{
        png_free_chunk(&chunk);
        if(png_read_chunk(fp, &chunk)){
            png_free_chunk(&chunk);
            return -1;
        }
        strncpy(type, chunk.type, 5);
        if(!strcmp(type, "IDAT\0") || !strcmp(type, "IEND\0")){
            png_free_chunk(&chunk);
            return -1;
        } 
    }while(strncmp(type,"PLTE\0", 5));

    if(png_parse_plte(&chunk, out_colors, out_count)){
        png_free_chunk(&chunk);
        return -1;
    }
    png_free_chunk(&chunk);
    return 0;
}

int png_summary(const char *filename, png_chunk_t **out_summary)
{
    if((!filename) | (!out_summary)){
        return -1;
    }
    
    FILE *fp = png_open(filename);
    if (!fp) {
        return -1;
    }

    size_t capacity = 16;
    int index = 0;
    png_chunk_t *summary = malloc(16 * sizeof(png_chunk_t));
    if (!summary) {
        fclose(fp);
        return -1;
    }

    //Read reads chunks until IEND is encountered, storing only the chunk type, length, and CRC validity status (not the actual chunk data).
    while(1){
        //Realloc array if capacity reached
        if(index >= capacity){
            capacity *= 2;
            png_chunk_t *new_summary = realloc(summary, capacity * sizeof(png_chunk_t));
            if(!new_summary){
                *out_summary = summary;
                fclose(fp);
                return -1;
            }
            summary = new_summary;
        }

        if(png_read_chunk(fp, summary + index)){
            *out_summary = summary;
            png_free_chunk(summary + index);
            fclose(fp);
            return -1;
        }
        png_free_chunk(summary + index);
        if(!strncmp((summary + index)->type, "IEND\0", 5)){
        index++;
        break;
        }

        index++;
    }
    
    *out_summary = summary;
    fclose(fp);
    return 0;
}