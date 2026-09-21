#include "png_overlay.h"
#include "png_reader.h"
#include "png_chunks.h"
#include "png_crc.h"
#include "helper.h"
#include "util.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int png_overlay_paste(const char *large_path, const char *small_path,
                      const char *output_path, uint32_t x_offset, uint32_t y_offset)
{
    if(!large_path || !small_path || !output_path){
        return -1;
    }

    //Open both input files and validate PNG signatures
    FILE *fp_large = png_open(large_path);
    if(!fp_large) return -1;

    FILE *fp_small = png_open(small_path);
    if(!fp_small){
        fclose(fp_large);
        return -1;
    }

    //Read and parse IHDR chunks from both images
    png_ihdr_t ihdr_large;
    if(png_extract_ihdr(fp_large, &ihdr_large)){
        fclose(fp_small); 
        fclose(fp_large); 
        return -1;
    }

    png_ihdr_t ihdr_small;
    if(png_extract_ihdr(fp_small, &ihdr_small)){
        fclose(fp_small); 
        fclose(fp_large); 
        return -1;
    }

    //Validate compatibility: both images must have same bit depth (8) and color type
    if(ihdr_large.bit_depth != 8 || ihdr_small.bit_depth != 8 || ihdr_large.color_type != ihdr_small.color_type){
        fclose(fp_small); 
        fclose(fp_large); 
        return -1;
    }

    png_color_t *colors_large = NULL;
    size_t count_large = 0;
    png_color_t *colors_small = NULL;
    size_t count_small = 0;
    uint32_t small_width = ihdr_small.width;
    uint32_t small_height = ihdr_small.height;
    uint32_t large_width = ihdr_large.width;
    uint32_t large_height = ihdr_large.height;
    uint8_t bpp = number_channels(ihdr_large.color_type);
    if(!bpp){
        fclose(fp_small); 
        fclose(fp_large); 
        return -1;
    }
    uint32_t small_length = small_width * bpp + 1;
    uint32_t large_length = large_width * bpp + 1;
    size_t large_idat_size;
    uint8_t *idat_large;
    if(decompress(fp_large, &idat_large, &large_idat_size)){
        fclose(fp_small);
        return -1;
    }
    size_t small_idat_size;
    uint8_t *idat_small;
    if(decompress(fp_small, &idat_small, &small_idat_size)){
        free(idat_large);
        return -1;
    }

    //Unfilter scanlines to reconstruct actual pixel values (handles filter types 0-4)
    uint8_t *unfiltered_small = (uint8_t *) malloc(small_idat_size);
    if(!unfiltered_small){
        free(idat_large); 
        free(idat_small);
        fclose(fp_large); 
        fclose(fp_small);
        return -1;
    }
    if(unfilter(&idat_small, &unfiltered_small, small_height, small_width * bpp, bpp)){
        free(unfiltered_small); 
        free(idat_large); 
        free(idat_small);
        fclose(fp_large); 
        fclose(fp_small);
        return -1;
    }
    free(idat_small);

    uint8_t *unfiltered_large = (uint8_t *) malloc(large_idat_size);
    if(!unfiltered_large){
        free(unfiltered_small); 
        free(idat_large);
        fclose(fp_large); 
        fclose(fp_small);
        return -1;
    }
    if(unfilter(&idat_large, &unfiltered_large, large_height, large_width * bpp, bpp)){
        free(unfiltered_large); 
        free(unfiltered_small); 
        free(idat_large);
        fclose(fp_large); 
        fclose(fp_small);
        return -1;
    }
    free(idat_large);

    // Rewind for PLTE extraction
    fseek(fp_large, 8, SEEK_SET);
    fseek(fp_small, 8, SEEK_SET);

    //For palette images (color type 3):
    if(ihdr_large.color_type == 3){

        //Extract PLTE chunks from both images (rewind files to read from beginning)
        if(png_extract_plte(fp_large, &colors_large, &count_large)){
            free(unfiltered_small); 
            free(unfiltered_large);
            fclose(fp_small); 
            fclose(fp_large);
            return -1;
        }
        if(png_extract_plte(fp_small, &colors_small, &count_small)){
            free(colors_large); 
            free(unfiltered_small); 
            free(unfiltered_large);
            fclose(fp_small); 
            fclose(fp_large);
            return -1;
        }

        int mapping[256];
        for(int i = 0; i < 256; i++){
            mapping[i] = 0;
        }

        //Create index mapping: map small image indices to merged palette indices
        for(int i = 0; i < (int) count_small; i++){
            int found = 0;
            for(int j = 0; j < (int)count_large; j++){
                if(colors_large[j].r == colors_small[i].r &&
                   colors_large[j].g == colors_small[i].g &&
                   colors_large[j].b == colors_small[i].b){
                    mapping[i] = j;
                    found = 1;
                    break;
                }
            }
            // & Merge palettes: add colors from small image that don't exist in large image
            if(!found){
                if(count_large >= 256){
                    free(colors_large); 
                    free(colors_small);
                    free(unfiltered_small); 
                    free(unfiltered_large);
                    fclose(fp_small); 
                    fclose(fp_large);
                    return -1;
                }
                png_color_t *colors_new = realloc(colors_large, (count_large + 1) * sizeof(png_color_t));
                if(!colors_new){
                    free(colors_large); free(colors_small);
                    free(unfiltered_small); free(unfiltered_large);
                    fclose(fp_small); fclose(fp_large);
                    return -1;
                }
                colors_large = colors_new;
                colors_large[count_large] = colors_small[i];
                mapping[i] = count_large;
                count_large++;
            }
        }
        free(colors_small);

        //Remap pixel indices in small image data to use merged palette
        for(uint32_t y = 0; y < small_height; y++){
            uint32_t scanline = y * small_length;
            for(uint32_t x = 1; x < small_length; x++){
                unfiltered_small[x + scanline] = mapping[unfiltered_small[x + scanline]];
            }
        }
    }

    //Paste operation: replace pixels in large image with pixels from small image 
    for(uint32_t y = 0; y < small_height && (y + y_offset) < large_height; y++){
        uint8_t *large_scanline = unfiltered_large + (y + y_offset) * large_length;
        uint8_t *small_scanline = unfiltered_small + y * small_length;

        uint32_t copy_width = small_width;
        if(x_offset + copy_width > large_width){
            copy_width = large_width - x_offset;
        }
        memcpy(large_scanline + x_offset * bpp + 1, small_scanline + 1, copy_width * bpp);
    }
    free(unfiltered_small);

    for(uint32_t y = 0; y < large_height; y++){
        unfiltered_large[y * large_length] = 0;
    }

    uint8_t *idat;
    size_t idat_size;
    if(util_deflate_data_png(unfiltered_large, large_idat_size, &idat, &idat_size)){
        if(colors_large){
            free(colors_large);    
        } 
        free(unfiltered_large); 
        fclose(fp_large);
        return -1;
    }
    free(unfiltered_large);

    FILE *fp_out = open_png_out(output_path);
    if(!fp_out){
        if(colors_large){
            free(colors_large);
        }
        free(idat); 
        fclose(fp_large);
        return -1;
    }

    if(write_out(fp_large, fp_small, fp_out, idat, idat_size, &colors_large, (ihdr_large.color_type == 3), count_large)){
        if(colors_large){ 
            free(colors_large);
        }
        free(idat); 
        fclose(fp_large);
        fclose(fp_small); 
        fclose(fp_out);
        return -1;
    }
    
    return 0;
}