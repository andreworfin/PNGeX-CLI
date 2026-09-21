#include "png_steg.h"
#include "png_reader.h"
#include "png_chunks.h"
#include "png_crc.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include "helper.h"

#define MAX_COLORS 256

/* Encode secret string into LSBs of image data */
int png_encode_lsb(const char *input_path, const char *output_path, const char *secret)
{
    if( (!input_path) || (! output_path) || (! secret)){
        return -1;
    }
    if(!strcmp(input_path, output_path)){
        return -1;
    }

    FILE *fp = png_open(input_path);
    if(!fp){
        return -1;
    } 

    png_ihdr_t idhr = {0};
    if(png_extract_ihdr(fp, &idhr)){
        fclose(fp);
        return -1;
    }
    
    if(idhr.bit_depth != 8){
        fclose(fp);
        return -1;
    }

    size_t idat_data_size;
    uint8_t *idat_data;
    if(decompress(fp, &idat_data, &idat_data_size)){
        return -1;
    }

    int width = idhr.width;
    int height = idhr.height;
    int capacity = (int) (height * width);

    //Check if the message fits
    int secret_length = ((strlen(secret) + 1) * 8);
    if(capacity < secret_length){
        fclose(fp);
        free(idat_data);
        return -1;
    }

    int secret_bits[secret_length];
    int secret_size = secret_length / 8;
    for (int i = 0; i < secret_size; i++) {
        char ch = secret[i];
        for (int j = 0; j < 8; j++) {
            secret_bits[i * 8 + j] = (ch >> j) & 1;
        }
    }

    png_color_t *colors = NULL;
    size_t count = 0;


    //For palette images (color type 3): 
    int change_palette = 0;
    if(idhr.color_type == 3){
        if(png_extract_plte(fp, &colors, &count)){
            free(idat_data);
            fclose(fp);
            return -1;
        }

        //Create a mapping table: 
        int mapping[MAX_COLORS];
        for(int i = 0; i < MAX_COLORS; i++){
            mapping[i] = -1;
        }

        //pair[i] = j if colors at indices i and j are identical
        for(int i = 0; i < count; i++){
            if(mapping[i] != -1){
                continue;
            }

            for(int j = i + 1; j < count; j++){
                if( (mapping[j] == -1) && 
                    (colors[i].r == colors[j].r) &&
                    (colors[i].b == colors[j].b) &&
                    (colors[i].g == colors[j].g) ){
                        mapping[i] = j;
                        mapping[j] = i;
                        break;
                    }
            }
        }

        //Append addtional colors if not 256
        for(int i = 0; i < count && count < MAX_COLORS; i++){
            if(mapping[i] == -1){
                if(!change_palette){
                    png_color_t *colors_new = realloc(colors, 256 * sizeof(png_color_t));
                    if(!colors_new){
                        free(colors);
                        free(idat_data);
                        fclose(fp);
                        return -1;
                    }
                    colors = colors_new;
                    change_palette = 1;
                }
                colors[count] = colors[i];
                mapping[i] = count;
                mapping[count] = i;
                count++;
            }
        }

        //Encode the message: swap between identical-color pair indices (lower index = 0, higher = 1)
        int secret_index = 0;
        for (int y = 0; y < height && secret_index < secret_length; y++) {
            int scanline_start = y * (width + 1);
            for (int x = 0; x < width && secret_index < secret_length; x++) {
                int pixel_offset = scanline_start + 1 + x;
                int palette_index = idat_data[pixel_offset];
                int pair = mapping[palette_index];
                
                int lower = -1;
                int higher = -1;
                if(palette_index < pair){
                    lower = palette_index;
                    higher = pair;
                }
                else{
                    lower = pair;
                    higher = palette_index;
                }

                idat_data[pixel_offset] = secret_bits[secret_index] ? higher : lower;
                secret_index++;
            }
        }
    }

    //For non-palette images
    else{
        int channel_size = (int)number_channels(idhr.color_type);
        if(!channel_size){
            free(colors);
            free(idat_data);
            fclose(fp);
            return -1;
        }
        
        int scanline_size = width * channel_size + 1;
        int secret_index = 0;
        for(int y = 0; y < height && secret_index < secret_length; y++){
            int scanline_start = y * scanline_size;
            for(int x = 0; x < width && secret_index < secret_length; x++){
                int pixel_start = scanline_start + 1 + x * channel_size;

                if(secret_bits[secret_index]){
                    idat_data[pixel_start] |= 1;
                } else {
                    idat_data[pixel_start] &= ~1;
                }
                secret_index++;
                }
            }
    }

    //Re-compress the modified image data using PNG-compatible zlib settings
    uint8_t *recompressed_data;
    size_t recompressed_data_size;
    if(util_deflate_data_png(idat_data, idat_data_size, &recompressed_data, &recompressed_data_size)){
        free(idat_data);
        free(colors);
        fclose(fp);
        return -1;
    }
    free(idat_data);

    FILE *fp_out = open_png_out(output_path);
    if(!fp_out){
        fclose(fp);
        free(recompressed_data);
        free(colors);
        return -1;
    }

    if(write_out(fp, NULL, fp_out, recompressed_data, recompressed_data_size, &colors, change_palette, count)){
        fclose(fp);
        fclose(fp_out);
        free(recompressed_data);
        free(colors);
        return -1;
    }
    return 0;
}

/* Extract secret string from LSBs of image data */
int png_extract_lsb(const char *input_path, char *out, size_t max_len)
{
    if(!input_path || ! out || max_len <= 0){
        return -1;
    }

    FILE* fp = png_open(input_path);
    if(!fp){
        return -1;
    }

    png_ihdr_t ihdr = {0};
    if(png_extract_ihdr(fp, &ihdr)){
        fclose(fp);
        return -1;
    }

    if(ihdr.bit_depth != 8){
        fclose(fp);
        return -1;
    }

    size_t idat_data_size;
    uint8_t *idat_data;
    if(decompress(fp, &idat_data, &idat_data_size)){
        return -1;
    }

    int width = ihdr.width;
    int height = ihdr.height;
    int max_bits = max_len * 8;
    int secret_bits[max_bits];

    //For palette images: extract PLTE and build identical-color pair table
    png_color_t *colors = NULL;
    size_t count = 0;

    if(ihdr.color_type == 3){
        if(png_extract_plte(fp, &colors, &count)){
            free(idat_data);
            fclose(fp);
            return -1;
        }

        int mapping[MAX_COLORS];
        for(int i = 0; i < MAX_COLORS; i++){
        mapping[i] = -1;
        }

        for(int i = 0; i < count; i++){
            if(mapping[i] != -1){
                continue;
            }

            for(int j = i + 1; j < count; j++){
                if( (mapping[j] == -1) && 
                    (colors[i].r == colors[j].r) &&
                    (colors[i].b == colors[j].b) &&
                    (colors[i].g == colors[j].g) ){
                        mapping[i] = j;
                        mapping[j] = i;
                        break;
                }
             }
        }

        //For palette images: determine bit by checking if index is the "higher" of a pair
        int secret_index = 0;
        for (int y = 0; y < height && secret_index < max_bits; y++) {
            int scanline_start = y * (width + 1);
            for (int x = 0; x < width && secret_index < max_bits; x++) {
                int pixel_offset = scanline_start + 1 + x; 
                int palette_index = idat_data[pixel_offset];

                int pair = mapping[palette_index];
                if(pair != -1){
                    if(palette_index > pair){
                        secret_bits[secret_index] = 1;
                    }
                    else{
                        secret_bits[secret_index] = 0;
                    }

                    secret_index++;
                }
            }
        }

        free(colors);
    }
    
    //For non-palette images: read LSB from first channel of each pixel
    else{
        int channel_size = -1;
        if(ihdr.color_type == 0){
            channel_size = 1;
        }
        else if(ihdr.color_type == 2){
            channel_size = 3;
        }
        else if(ihdr.color_type == 4){
            channel_size = 2;
        }
        else if(ihdr.color_type == 6){
            channel_size = 4;
        }
        else{
            free(idat_data);
            fclose(fp);
            return -1;
        }

        int scanline_size = width * channel_size + 1;
        int secret_index = 0;
        for(int y = 0; y < height && secret_index < max_bits; y++){
            int scanline_start = y * scanline_size;
            for(int x = 0; x < width && secret_index < max_bits; x++){
                int pixel_start = scanline_start + 1 + x * channel_size;

                secret_bits[secret_index] = idat_data[pixel_start] & 1;
            
                secret_index++;
                }
            }

    }
    free(idat_data);
    fclose(fp);

    //Reconstruct bytes
    int length = 0;
    for(int i = 0; i < max_len; i++){
        char ch = 0;
        for(int j = 0; j < 8; j++){
            ch |= (secret_bits[i * 8 + j] & 1) << j;
        }
        out[i] = ch;

        if(ch == '\0'){
            break;
        }

        length++;
    }

    return length;
}

