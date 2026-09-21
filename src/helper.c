#include "helper.h"
#include "util.h"
#include "png_reader.h"
#include "png_chunks.h"
#include "png_crc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int decompress(FILE *fp, uint8_t **out_data, size_t *out_size){
    if(!fp || !out_data || !out_size){
        return -1;
    }

    size_t compressed_buffer_size = 0;
    uint8_t *compressed_buffer = NULL;
    while(1){
        png_chunk_t idat_chunk = {0};
        if(png_read_chunk(fp, &idat_chunk)){
            free(compressed_buffer);
            fclose(fp);
            return -1;
        }

        if(!strncmp(idat_chunk.type, "IEND\0", 5)){
            png_free_chunk(&idat_chunk);
            break;
        }

        if (!strncmp(idat_chunk.type, "IDAT", 4)) { 
            uint8_t *new_compressed_buffer = realloc(compressed_buffer, compressed_buffer_size + (size_t) idat_chunk.length);
            if(!new_compressed_buffer){
                free(compressed_buffer);
                fclose(fp);
                return -1;
            }
            compressed_buffer = new_compressed_buffer;
            memcpy(compressed_buffer + compressed_buffer_size, idat_chunk.data, idat_chunk.length);
            compressed_buffer_size += (size_t) idat_chunk.length;
        }
        png_free_chunk(&idat_chunk);
    }

    if(util_inflate_data(compressed_buffer, compressed_buffer_size, out_data, out_size)){
        free(compressed_buffer);
        fclose(fp);
        return -1;
    }
    free(compressed_buffer);
    fseek(fp, 8, SEEK_SET);
    return 0;
}

FILE *open_png_out(const char *output_path){
    if(!output_path){
        return NULL;
    }
    if(strlen(output_path) < 4 || strcmp(output_path + strlen(output_path) - 4, ".png")){
    return NULL;
    } 
    FILE *fp = fopen(output_path, "wb");
    return fp;
}

void write_u32(FILE *fp, uint32_t value){
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    fwrite(bytes, sizeof(uint8_t), 4, fp);
}

int write_out(FILE *fp, FILE *fp_small, FILE *fp_out, uint8_t *idat_data, size_t idat_data_size, png_color_t **colors, int change_palette, size_t count){
    if(!fp || !fp_out || ! idat_data || !colors){
        return -1;
    }

    rewind(fp);
    for(int i = 0; i < 8; i++) {
        fputc(fgetc(fp), fp_out);
    }

    int idat_written = 0;
    while(1){
        png_chunk_t chunk = {0};
        if(png_read_chunk(fp, &chunk)){
            png_free_chunk(&chunk);
            return -1;
        }

        if (!strncmp(chunk.type, "PLTE\0", 5)){
            if (change_palette) {
                if (chunk.data && colors){

                    //Rellaocate if Array too small
                    if(count == -1){
                        png_free_chunk(&chunk);
                        return -1;
                    }
                    if (chunk.length < count * 3){
                        uint8_t *new_data = realloc(chunk.data, count * 3);
                        if (!new_data) {
                            png_free_chunk(&chunk);
                            return -1;
                        }
                        chunk.data = new_data;
                    }

                    //Update colors
                    for (int i = 0; i < count; i++){
                        chunk.data[i * 3] = (*colors + i)->r;
                        chunk.data[i * 3 + 1] = (*colors + i)->g;
                        chunk.data[i * 3 + 2] = (*colors + i)->b;
                    }

                    chunk.length = count * 3;

                    // Update CRC
                    uint8_t *buf_array = malloc(4 + chunk.length);
                    if (!buf_array){
                        png_free_chunk(&chunk);
                        return -1;
                    }
                    memcpy(buf_array, chunk.type, 4);
                    memcpy(buf_array + 4, chunk.data, chunk.length);
                    chunk.crc = png_crc(buf_array, 4 + chunk.length);
                    free(buf_array);
                }
            }
        }
        
        if(!strncmp(chunk.type, "IDAT\0", 5)){
            if(!idat_written){
                //Copy ancillary chunks from small file (pHYs)
                if(fp_small){
                    fseek(fp_small, 8, SEEK_SET); //skip signature
                    while(1){
                        png_chunk_t small_chunk = {0};
                        if(png_read_chunk(fp_small, &small_chunk)){
                            png_free_chunk(&small_chunk);
                            break;
                        }

                        //only copy ancillary chunks
                        if(!strncmp(small_chunk.type, "IHDR", 4) || !strncmp(small_chunk.type, "PLTE", 4) || !strncmp(small_chunk.type, "IDAT", 4) || !strncmp(small_chunk.type, "IEND", 4)){
                            if(!strncmp(small_chunk.type, "IEND", 4)){
                                png_free_chunk(&small_chunk);
                                break;
                            }
                            png_free_chunk(&small_chunk);
                            continue;
                        }

                        //Write ancillary chunk from small file
                        write_u32(fp_out, small_chunk.length);
                        fwrite(small_chunk.type, sizeof(uint8_t), 4, fp_out);
                        fwrite(small_chunk.data, sizeof(uint8_t), small_chunk.length, fp_out);
                        write_u32(fp_out, small_chunk.crc);
                        png_free_chunk(&small_chunk);
                    }
                }

                //Write IDAT
                write_u32(fp_out, idat_data_size);
                fwrite("IDAT", sizeof(uint8_t), 4, fp_out);
                fwrite(idat_data, sizeof(uint8_t), idat_data_size, fp_out);

                //Update CRC
                uint8_t *buf_array = malloc(4 + idat_data_size);
                if (!buf_array){
                    png_free_chunk(&chunk);
                    return -1;
                }
                memcpy(buf_array, "IDAT", 4);
                memcpy(buf_array + 4, idat_data, idat_data_size);
                uint32_t crc = png_crc(buf_array, 4 + idat_data_size);
                write_u32(fp_out, crc);
                free(buf_array);
                idat_written = 1;
            }
            png_free_chunk(&chunk);
            continue;
        }

        if(!strncmp(chunk.type, "IEND\0", 5)){
            //Now write IEND
            write_u32(fp_out, chunk.length);
            fwrite(chunk.type, sizeof(uint8_t), 4, fp_out);
            fwrite(chunk.data, sizeof(uint8_t), chunk.length, fp_out);
            write_u32(fp_out, chunk.crc);
            png_free_chunk(&chunk);
            break;
        }
        
        //write fields to fp_out
        write_u32(fp_out, chunk.length);

        fwrite(chunk.type, sizeof(uint8_t), 4, fp_out);

        fwrite(chunk.data, sizeof(uint8_t), chunk.length, fp_out);

        write_u32(fp_out, chunk.crc);

        png_free_chunk(&chunk);
    }
    free(idat_data);
    free(*colors);
    fclose(fp);
    if(fp_small){
        fclose(fp_small);
    }
    fclose(fp_out);

    return 0;
}

uint8_t number_channels(uint8_t color_type){
    switch(color_type){
        case 0:
            return 1;
        case 2:
            return 3;
        case 3:
            return 1;
        case 4:
            return 2;
        case 6:
            return 4;
        default:
            return 0;
    }
}

int unfilter(uint8_t **orginal, uint8_t **unfiltered, uint32_t height, uint32_t width, uint8_t bpp){
    if(!orginal || !unfiltered){
        return -1;
    }

    for(uint32_t y = 0; y < height; y++){
        uint8_t *orginal_scanline = *orginal + y * (width + 1);
        uint8_t *unfiltered_scanline = *unfiltered + y * (width + 1);
        uint8_t *prev_scanline = (y == 0) ? NULL : *unfiltered + (y - 1) * (width + 1);

        unfiltered_scanline[0] = orginal_scanline[0];

        switch(unfiltered_scanline[0]){
            case 0: //None
                for(int i = 1; i <= width; i++){
                    unfiltered_scanline[i] = orginal_scanline[i];
                }
                break;
            case 1: //Sub byte unfiltering
                for(int i = 1; i <= bpp; i++){
                    unfiltered_scanline[i] = orginal_scanline[i];
                }   
                for(int i = bpp + 1; i <= width; i++){
                    unfiltered_scanline[i] = (orginal_scanline[i] + unfiltered_scanline[i - bpp]) & 0xFF;
                }
                break;
            case 2: //Up
                if(y == 0){
                    for(int i = 1; i <= width; i++){
                        unfiltered_scanline[i] = orginal_scanline[i];
                    }
                }
                else{
                    for(int i = 1; i <= width; i++){
                        unfiltered_scanline[i] = (orginal_scanline[i] + prev_scanline[i]) & 0xFF ;
                    }
                }
                break;
            case 3: //Average
                for(int i = 1; i <= width; i++){
                    uint8_t left = (i > bpp) ? unfiltered_scanline[i - bpp] : 0;
                    uint8_t above = (y > 0) ? prev_scanline[i] : 0;

                    uint8_t prediction = (left + above) / 2;  
                    unfiltered_scanline[i] = (orginal_scanline[i] + prediction) & 0xFF;
                }
                break;
            case 4: //Paeth
            for(int i = 1; i <= width; i++){
                uint8_t left = (i > bpp) ? unfiltered_scanline[i - bpp] : 0;
                uint8_t above = (y > 0) ? prev_scanline[i] : 0;
                uint8_t upper_left = (i > bpp && y > 0) ? prev_scanline[i - bpp] : 0;
                int p = (int)left + (int)above - (int)upper_left;
                int pa = abs(p - ((int)left));
                int pb = abs(p - ((int)above));
                int pc = abs(p - ((int)upper_left));

                int chosen_neighbor;
                if(pa <= pb && pa <= pc){
                    chosen_neighbor = left;
                }
                else if(pb <= pc){
                    chosen_neighbor = above;
                } 
                else{
                    chosen_neighbor = upper_left;
                } 

                unfiltered_scanline[i] = (orginal_scanline[i] + chosen_neighbor) & 0xFF ;
            }
                break;
            default:
                return -1;
        }
    }

    return 0;
}