// Program that decodes a PNG file (only 8 bit true color RGBA and no interlacing, at the time).

// In the future i want to make more compatible:
// 1) Add a parser of the following chunks: PLTE, tEXt, zTXt, gAMA, cHRM, sRGB, tIME 
// 2) Add compatibility with the following color types: Grayscale, RGB, Indexed Color, Grayscale with Alpha
// 3) Add compatibility with the following bit depths: 1, 2, 4, 16 bits
// 4) Checking the IDAT chunks concatenation
// 5) Checking the "Average" filter
// 6) Add the "Adam7" interlacing
// 7) Add more tests about PNG integrity

#include <png_decoder.h>

uint32_t swap_endianess(const uint32_t bytes)
{
    return bytes >> 24 & 0xFF | bytes >> 8 & 0xFF00 | bytes << 8 & 0xFF0000 | bytes << 24 & 0xFF000000;
}

chunk_t parse_chunk(unsigned char* png_buffer, size_t* current_offset)
{
    chunk_t chunk;

    uint32_t chunk_length = *(uint32_t*)&png_buffer[*current_offset];
    chunk_length = swap_endianess(chunk_length);
    *current_offset += chunk_length_offset;
    chunk_data_offset = chunk_length;

    chunk.data_length = chunk_length;

    char* chunk_type = malloc(chunk_type_offset); // Always 4 bytes.
    memcpy(chunk_type, &png_buffer[*current_offset], chunk_type_offset);
    chunk_type[chunk_type_offset] = '\0'; 
    *current_offset += chunk_type_offset;

    chunk.type = chunk_type;

    uint8_t* chunk_data = malloc(chunk_data_offset);
    memcpy(chunk_data, &png_buffer[*current_offset], chunk_data_offset);
    *current_offset += chunk_data_offset;

    chunk.data = chunk_data;

    uint32_t chunk_crc = *(uint32_t*)&png_buffer[*current_offset];
    chunk_crc = swap_endianess(chunk_crc);
    *current_offset += chunk_crc_offset;

    uint8_t* crc_fields = malloc(chunk_type_offset + chunk_data_offset);
    memcpy(crc_fields, chunk_type, chunk_type_offset);
    memcpy(&crc_fields[chunk_type_offset], chunk_data, chunk_data_offset);

    uint32_t checksum = crc32(0, crc_fields, chunk_type_offset + chunk_data_offset);
    if(checksum != chunk_crc)
    {
        fprintf(stderr, "Warning: The chunk type %s has been corrupted!\n", chunk_type);
    }

    free(crc_fields);
    return chunk;
}

int parse_chunk_IHDR(png_props_t* png_props, const chunk_t* chunk)
{
    chunk_IHDR_t chunk_IHDR;
    size_t current_IHDR_offset = 0;
    
    chunk_IHDR.width = swap_endianess(*(uint32_t*)&chunk->data[current_IHDR_offset]);
    current_IHDR_offset += 4; // uint32_t
    chunk_IHDR.height = swap_endianess(*(uint32_t*)&chunk->data[current_IHDR_offset]);
    current_IHDR_offset += 4; // uint32_t
    chunk_IHDR.bit_depth = chunk->data[current_IHDR_offset];

    if(chunk_IHDR.bit_depth != BIT_DEPTH)
    {
        perror("Error: this PNG decoder supports only 8 bit depth!");
        return EXIT_FAILURE;
    }

    current_IHDR_offset += 1; // uint8_t
    chunk_IHDR.color_type = chunk->data[current_IHDR_offset];

    if(chunk_IHDR.color_type != COLOR_TYPE)
    {
        perror("Error: this PNG decoder supports only \"Truecolor with Alpha (RGBA)\" color type!");
        return EXIT_FAILURE;
    }

    current_IHDR_offset += 1; // uint8_t
    chunk_IHDR.compression_method = chunk->data[current_IHDR_offset];

    if(chunk_IHDR.compression_method != 0)
    {
        perror("Error: this PNG decoder supports only 0 as compression method (Deflate/Inflate)!");
        return EXIT_FAILURE;
    }

    current_IHDR_offset += 1; // uint8_t
    chunk_IHDR.filter_method = chunk->data[current_IHDR_offset];

    if(chunk_IHDR.filter_method != 0)
    {
        perror("Error: this PNG decoder supports only 0 as filter method!");
        return EXIT_FAILURE;
    }

    current_IHDR_offset += 1; // uint8_t
    chunk_IHDR.interlace_method = chunk->data[current_IHDR_offset];

    if(chunk_IHDR.interlace_method != 0)
    {
        perror("Error: this PNG decoder supports only 0 as interlace method! (no interlace)");
        return EXIT_FAILURE;
    }

    current_IHDR_offset += 1; // uint8_t

    png_props->IHDR = chunk_IHDR;

    return 0;
}

uint8_t recon_a(const uint8_t* pixels_data, const size_t x, const size_t y, const size_t scanline)
{
    if(x <= BYTES_PER_PIXEL) return 0;
    return pixels_data[x + y * scanline - BYTES_PER_PIXEL];
}

uint8_t recon_b(const uint8_t* pixels_data, const size_t x, const size_t y, const size_t scanline)
{
    if(y <= 0) return 0;
    return pixels_data[x + (y - 1) * scanline];
}

uint8_t recon_c(const uint8_t* pixels_data, const size_t x, const size_t y, const size_t scanline)
{
    if(x <= BYTES_PER_PIXEL || y <= 0) return 0;
    return pixels_data[(x - BYTES_PER_PIXEL + (y - 1) * scanline)];
}

uint8_t paeth_predictor(const uint8_t recon_a, const uint8_t recon_b, const uint8_t recon_c)
{
    int32_t paeth = recon_a + recon_b - recon_c;
    int32_t paeth_a = abs(paeth - recon_a);
    int32_t paeth_b = abs(paeth - recon_b);
    int32_t paeth_c = abs(paeth - recon_c);

    if(paeth_a <= paeth_b && paeth_a <= paeth_c) return recon_a;
    if(paeth_b <= paeth_c) return recon_b;
    return recon_c;
}

void print_info(const chunk_IHDR_t* png_info)
{
    printf("PNG INFO\n");

    printf("Width: %d\n", png_info->width);
    printf("Height: %d\n", png_info->height);
    printf("Bit Depth: %u\n", png_info->bit_depth);

    printf("Color Type: ");
    switch(png_info->color_type)
    {
        case 0:
            printf("Grayscale\n");
            break;
        case 2:
            printf("Truecolor (RGB)\n");
            break;
        case 3:
            printf("Indexed Color\n");
            break;
        case 4:
            printf("Grayscale with Alpha\n");
            break;
        case 6:
            printf("Truecolor with Alpha (RGBA)\n");
            break;
        default:
            perror("Warning: invalid color type!\n");
            break;
    }

    printf("Compression Method: %u\n", png_info->compression_method); // Only 0
    printf("Filter Method: %u\n", png_info->filter_method); // Only 0

    printf("Interlace Method: ");
    switch(png_info->interlace_method)
    {
        case 0:
            printf("No Interlace\n");
            break;
        case 1:
            printf("Adam7 Interlace\n");
            break;
        default:
            perror("Warning: invalid interlace method!\n");
            break;
    }
}

int main(int argc, char** argv)
{
    // PNG File Checking ------------------------------------------------------------------------------------------

    if(argc != 2)
    {
        perror("Error: number of arguments is wrong!");
        return EXIT_FAILURE;
    }

    FILE* png_file = NULL;
    if(fopen_s(&png_file, argv[1], "rb") != 0)
    {
        perror("Error: opening file failed!");
        return EXIT_FAILURE;
    }

    // PNG File Reading -------------------------------------------------------------------------------------------

    if(fseek(png_file, 0, SEEK_END))
    {
        perror("Error: moving cursor at the end failed!");
        return EXIT_FAILURE;
    }

    const long png_size = ftell(png_file);
    unsigned char* png_buffer = malloc(png_size);

    if(!png_buffer)
    {
        perror("Error: allocation unsuccessful!");
        return EXIT_FAILURE;
    }

    if(fseek(png_file, 0, SEEK_SET))
    {
        perror("Error: moving cursor at the start failed!");
        return EXIT_FAILURE;
    }

    const int png_size_read = fread_s(png_buffer, png_size, 1, png_size, png_file);
    if(png_size_read != png_size)
    {
        fprintf(stderr, "Error: unable to read data over %d", png_size_read);
        free(png_buffer);
        fclose(png_file);
        return EXIT_FAILURE;
    }
    
    fclose(png_file);

    // PNG Header Matching ----------------------------------------------------------------------------------------

    png_props_t png_props;
    png_props.pixels_data = NULL;

    size_t current_offset = 0;
    const uint8_t header_offset = 8;
    const char* png_signature = "\x89PNG\x0D\x0A\x1A\x0A"; // b"\x89PNG\r\n\x1a\n"
    if(memcmp(png_buffer, png_signature, header_offset))
    {
        perror("Error: your file doesn't match the PNG signature. Are you sure that your file is a PNG?");
        return EXIT_FAILURE;
    }

    current_offset += header_offset;
    
    // PNG Chunk Reading ------------------------------------------------------------------------------------------

    chunk_t* chunks = NULL;
    size_t number_of_chunks = 0;

    uint8_t* chunk_IDAT_data = NULL;
    size_t chunk_IDAT_data_length = 0;

    while(TRUE)
    {
        chunk_t chunk = parse_chunk(png_buffer, &current_offset);

        chunks = realloc(chunks, sizeof(chunk_t) * (number_of_chunks + 1));
        memcpy(&chunks[number_of_chunks], &chunk, sizeof(chunk_t));
        number_of_chunks++;

        if(!strcmp(chunk.type, "IEND")) break;
    }

    // PNG Chunk Parsing ------------------------------------------------------------------------------------------

    // Avoid gAMA type of chunk.
    for (size_t i = 0; i < number_of_chunks; i++)
    {
        if(!strcmp(chunks[i].type, chunk_IHDR_type))
        {
            if(parse_chunk_IHDR(&png_props, &chunks[i])) return EXIT_FAILURE;
        }
        else if(!strcmp(chunks[i].type, chunk_gAMA_type))
        {
            continue; // "gAMA" chunk ignored at the time.
        }
        else if(!strcmp(chunks[i].type, chunk_IDAT_type))
        {
            chunk_IDAT_data = realloc(chunk_IDAT_data, chunk_IDAT_data_length + chunks[i].data_length);
            memcpy(&chunk_IDAT_data[chunk_IDAT_data_length], chunks[i].data, chunks[i].data_length);

            chunk_IDAT_data_length += chunks[i].data_length;
        }
        else if(!strcmp(chunks[i].type, chunk_IEND_type))
        {
            break;
        }
        else
        {
            perror("Error: found a non valid chunk type!");
            
            free(png_buffer);
            free(chunks);
            free(chunk_IDAT_data);

            return EXIT_FAILURE;
        }
    }
   
    // PNG Data Decompressing -------------------------------------------------------------------------------------

    size_t scanline = png_props.IHDR.width * BYTES_PER_PIXEL + 1;
    size_t stride = png_props.IHDR.width * BYTES_PER_PIXEL;

    unsigned long pixels_data_length = scanline * png_props.IHDR.height;
    size_t pixels_filtered_data_length = stride * png_props.IHDR.height;

    uint8_t* pixels_data_with_filters = malloc(pixels_data_length);
    png_props.pixels_data = malloc(pixels_filtered_data_length);

    if(uncompress(pixels_data_with_filters, &pixels_data_length, chunk_IDAT_data, chunk_IDAT_data_length) != Z_OK)
    {
        perror("Error: unexpected size of uncompressed data!");
        return EXIT_FAILURE;
    } 

    // PNG Data Reconstruction ------------------------------------------------------------------------------------

    for (size_t i = 0; i < png_props.IHDR.height; i++)
    {
        uint8_t filter_type = pixels_data_with_filters[i * scanline];

        for (size_t j = 1; j < scanline; j += BYTES_PER_PIXEL)
        {
            size_t current_index = i * scanline + j;

            // Reconstruction functions.
            switch (filter_type)
            {
                case 0: break; // None
                case 1: // Sub
                    pixels_data_with_filters[current_index] += recon_a(pixels_data_with_filters, j, i, scanline);
                    pixels_data_with_filters[current_index + 1] += recon_a(pixels_data_with_filters, j + 1, i, scanline);
                    pixels_data_with_filters[current_index + 2] += recon_a(pixels_data_with_filters, j + 2, i, scanline);
                    pixels_data_with_filters[current_index + 3] += recon_a(pixels_data_with_filters, j + 3, i, scanline);
                    break;
                case 2: // Up
                    pixels_data_with_filters[current_index] += recon_b(pixels_data_with_filters, j, i, scanline);
                    pixels_data_with_filters[current_index + 1] += recon_b(pixels_data_with_filters, j + 1, i, scanline);
                    pixels_data_with_filters[current_index + 2] += recon_b(pixels_data_with_filters, j + 2, i, scanline);
                    pixels_data_with_filters[current_index + 3] += recon_b(pixels_data_with_filters, j + 3, i, scanline);
                    break;
                case 3: // Average
                    pixels_data_with_filters[current_index] += ((recon_a(pixels_data_with_filters, j, i, scanline) + recon_b(pixels_data_with_filters, j, i, scanline)) * 0.5f);
                    pixels_data_with_filters[current_index + 1] += ((recon_a(pixels_data_with_filters, j + 1, i, scanline) + recon_b(pixels_data_with_filters, j + 1, i, scanline)) * 0.5f);
                    pixels_data_with_filters[current_index + 2] += ((recon_a(pixels_data_with_filters, j + 2, i, scanline) + recon_b(pixels_data_with_filters, j + 2, i, scanline)) * 0.5f);
                    pixels_data_with_filters[current_index + 3] += ((recon_a(pixels_data_with_filters, j + 3, i, scanline) + recon_b(pixels_data_with_filters, j + 3, i, scanline)) * 0.5f);
                    break;
                case 4: // Paeth
                    pixels_data_with_filters[current_index] += paeth_predictor(recon_a(pixels_data_with_filters, j, i, scanline), recon_b(pixels_data_with_filters, j, i, scanline), recon_c(pixels_data_with_filters, j, i, scanline));
                    pixels_data_with_filters[current_index + 1] += paeth_predictor(recon_a(pixels_data_with_filters, j + 1, i, scanline), recon_b(pixels_data_with_filters, j + 1, i, scanline), recon_c(pixels_data_with_filters, j + 1, i, scanline));
                    pixels_data_with_filters[current_index + 2] += paeth_predictor(recon_a(pixels_data_with_filters, j + 2, i, scanline), recon_b(pixels_data_with_filters, j + 2, i, scanline), recon_c(pixels_data_with_filters, j + 2, i, scanline));
                    pixels_data_with_filters[current_index + 3] += paeth_predictor(recon_a(pixels_data_with_filters, j + 3, i, scanline), recon_b(pixels_data_with_filters, j + 3, i, scanline), recon_c(pixels_data_with_filters, j + 3, i, scanline));
                    break;
                default:
                    fprintf(stderr, "Warning: filter %u not found!", filter_type);
                    break;
            }
        }
    }

    for (size_t i = 0; i < png_props.IHDR.height; i++)
    {
        size_t current_index = i * scanline + 1;
        memcpy(&png_props.pixels_data[i * stride], &pixels_data_with_filters[current_index], stride);
    }

    // Print IHDR Fields ------------------------------------------------------------------------------------------

    print_info(&png_props.IHDR);

    // SDL Initialization and Blitting ----------------------------------------------------------------------------
    
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow("PNG Decoder", 100, 100, 512, 512, 0);
    if (!window)
    {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        goto quit;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        goto quit;
    }

    SDL_Texture* texture_png = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, png_props.IHDR.width, png_props.IHDR.height);
    SDL_UpdateTexture(texture_png, NULL, png_props.pixels_data, stride);
    SDL_SetTextureBlendMode(texture_png, SDL_BLENDMODE_BLEND);

    SDL_RenderClear(renderer);

    SDL_RenderCopy(renderer, texture_png, NULL, NULL);
    SDL_RenderPresent(renderer);

    int running = TRUE;
    while (running)
    {
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = FALSE;
            }
        }
    }

quit:
    SDL_Quit();

    free(png_buffer);
    free(chunks);
    free(chunk_IDAT_data);
    free(pixels_data_with_filters);
    free(png_props.pixels_data);

    return EXIT_SUCCESS;
}