#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "png_reader.h"
#include "png_chunks.h"
#include "png_steg.h"
#include "png_overlay.h"

int main(int argc, char **argv)
{
    int fCheck = 0;
    char *filename = NULL;
    for(int i = 1; i < argc; i++){

        if(!strcmp(argv[i], "-h")){
            PRINT_USAGE(argv[0]);
            free(filename);
            return EXIT_SUCCESS;
        }

        if(!strcmp(argv[i], "-f")){
            if(fCheck){
                continue;
            }
            if(((i + 1) >= argc) || (argv[i  + 1][0] == '-')){
                fCheck = -1;
            }
            else{
                i++;
                filename = malloc(strlen(argv[i]) + 1);
                if(!filename){
                    return EXIT_FAILURE;
                }
                strcpy(filename, argv[i]);
                fCheck = 1;
            }
        }
    }

    if(!fCheck){
        PRINT_ERROR_MISSING_F_FLAG();
        return EXIT_FAILURE;
    }
    else if(fCheck == -1){
        PRINT_ERROR_F_REQUIRES_FILENAME();
        return EXIT_FAILURE;
    }
    
    int sCheck = 0, pCheck = 0, iCheck = 0, eCheck = 0, dCheck = 0, mCheck = 0;

    for(int i  = 1; i < argc; i++){
        if(!strcmp(argv[i], "-s")){
            if(sCheck){
                continue;
            }
            
            png_chunk_t *summary = NULL;
            if(png_summary(filename, &summary)){
                free(summary);
                free(filename);
                PRINT_ERROR_READ_CHUNKS();
                return EXIT_FAILURE;
            }

            PRINT_CHUNK_SUMMARY_HEADER(filename);
            int index = 0;
            while(1){
                PRINT_CHUNK_INFO(index, summary[index]);
                if(!strncmp(summary[index].type, "IEND\0", 5)){
                    break;
                }
                index++;
            }
            free(summary);
            sCheck++;
        }

        else if(!strcmp(argv[i], "-p")){
            if(pCheck){
                continue;
            }
            FILE *fp = png_open(filename);
            if(!fp){
                PRINT_ERROR_OPEN_FILE(filename);
                free(filename);
                return EXIT_FAILURE;
            }
            
            png_color_t *colors = NULL;
            size_t count = 0;

            if(png_extract_plte(fp, &colors, &count)){
                fclose(fp);
                free(filename);
                PRINT_ERROR_PLTE_NOT_FOUND(); 
                return EXIT_FAILURE;
            }

            PRINT_PALETTE_HEADER(filename);
            PRINT_PALETTE_COUNT(count);
            for(size_t i = 0; i < count; i++){
                PRINT_PALETTE_COLOR(i, colors[i].r, colors[i].g, colors[i].b);
            }
            free(colors);
            fclose(fp);

            pCheck++;
        }

        else if(!strcmp(argv[i], "-i")){
            if(iCheck){
                continue;
            }
            FILE *fp = png_open(filename);
            if(!fp){
                PRINT_ERROR_OPEN_FILE(filename);
                free(filename);
                return EXIT_FAILURE;
            }

            png_ihdr_t ihdr;
            if(png_extract_ihdr(fp, &ihdr)){
                fclose(fp);
                free(filename);
                PRINT_ERROR_READ_IHDR();
                return EXIT_FAILURE;
            } 
            PRINT_IHDR(filename, ihdr);
            fclose(fp);
            iCheck++;
        }

        else if(!strcmp(argv[i], "-e")){
            if(eCheck){
                continue;
            }
            int start = i + 1;
            int end = start;
            while(end < argc && strcmp(argv[end], "-o")){
                if(argv[end][0] == '-' && end != start){
                    break;
                }
                end++;
            }

            if(end == argc || end == start) {
                PRINT_ERROR_ENCODE_REQUIRES();
                free(filename);
                return EXIT_FAILURE;
            }

            if(end + 1 >= argc || argv[end + 1][0] == '-') {
                PRINT_ERROR_ENCODE_REQUIRES();
                free(filename);
                return EXIT_FAILURE;
            }

            int total_len = 0;
            for(int k = start; k < end; k++) {
                total_len += strlen(argv[k]);
                if (k < end - 1){ //space
                    total_len++;
                }
            }
            total_len++;
            char *message = malloc(total_len);
            if(!message) {
                PRINT_ERROR_ENCODE_FAILED();
                free(filename);
                return EXIT_FAILURE;
            }

            message[0] = '\0';
            for(int k = start; k < end; k++) {
                strcat(message, argv[k]);
                if(k < end - 1){
                    strcat(message, " ");
                }
            } 
            
            if(png_encode_lsb(filename, argv[end + 1], message)){
                free(message);
                PRINT_ERROR_ENCODE_FAILED();
                free(filename);
                return EXIT_FAILURE;
            }
            
            PRINT_ENCODE_SUCCESS(argv[end + 1]);
            free(message);
            i += end + 1;
            eCheck++;
        }

        else if(!strcmp(argv[i], "-d") ){
            if(dCheck){
                continue;
            }
            size_t length = 4096;
            char *message = (char *)malloc(length);
            if(!message){
                PRINT_ERROR_EXTRACT_FAILED();
                free(filename);
                return EXIT_FAILURE;
            }

            int actual_length = 0;
            if((actual_length = png_extract_lsb(filename, message, length)) == -1){
                PRINT_ERROR_EXTRACT_FAILED();
                free(message);
                free(filename);
                return EXIT_FAILURE;
            }

            if(actual_length >= length){
                message[length - 1] = '\0';
            }

            PRINT_HIDDEN_MESSAGE(message);
            free(message);
            dCheck++;
        }

        else if(!strcmp(argv[i], "-m")){
            if(mCheck){
                continue;
            }
            if((i + 3) >= argc || argv[i + 1][0] == '-' || strcmp(argv[i+2], "-o") || (argv[i + 3][0] == '-')){
                PRINT_ERROR_OVERLAY_REQUIRES();
                free(filename);
                return EXIT_FAILURE;
            }
            int width = 0, height = 0, widthCheck = 0, heightCheck = 0;
            int j = 4;
            while(i + j < argc){
                if(!strcmp(argv[i + j], "-w")){
                    if(widthCheck){
                        j += 2;
                        continue;
                    }
                    if((i + j + 1) >= argc || argv[i + j + 1][0] == '-'){
                        PRINT_ERROR_WIDTH_REQUIRES();
                        free(filename);
                        return EXIT_FAILURE;
                    }
                    width = (uint32_t) strtoul(argv[i + j + 1], NULL, 10);
                    widthCheck = 1;
                    j += 2;
                }
                else if(!strcmp(argv[i + j], "-g")){
                    if(heightCheck){
                        j += 2;
                        continue;
                    }
                    if((i + j + 1) >= argc || argv[i + j + 1][0] == '-'){
                        PRINT_ERROR_HEIGHT_REQUIRES();
                        free(filename);
                        return EXIT_FAILURE;
                    }
                    height = (uint32_t) strtoul(argv[i + j + 1], NULL, 10);
                    heightCheck = 1;
                    j += 2;
                }
                else{
                    break;
                }
            }
            
            if(png_overlay_paste(filename, argv[i + 1], argv[i+3], width, height)){
                PRINT_ERROR_OVERLAY_FAILED();
                free(filename);
                return EXIT_FAILURE;
            }
            PRINT_OVERLAY_SUCCESS(argv[i + 3]);
            i += j - 1;
            mCheck++;

        }

        else if(!strcmp(argv[i], "-f")){
            i++;
        }
        
        else{
            PRINT_ERROR_UNKNOWN_OPTION(argv[i]);
            free(filename);
            return EXIT_FAILURE;
        }
    }
    free(filename);
    return EXIT_SUCCESS;
}
