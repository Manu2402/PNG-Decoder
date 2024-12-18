#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <zlib.h>
#include <SDL.h>

typedef struct chunk
{
    uint32_t data_length;
    char* type;
    uint8_t* data;
} chunk_t;

typedef struct chunk_IHDR
{
    uint32_t width, height;
    uint8_t bit_depth;
    uint8_t color_type; // 0: Grayscale, 2: Truecolor, 3: Indexed-Color, 4: Greyscale with Alpha, 6: Truecolor with Alpha
    uint8_t compression_method; // 0: Deflate/Inflate compression
    uint8_t filter_method; // 0: None, 1: Sub, 2: Up, 3: Average, 4: Paeth 
    uint8_t interlace_method; // 0: None, 1: Adam7
    // 3 bytes padding.
} chunk_IHDR_t;

typedef struct png_props
{
    uint8_t* pixels_data;
    chunk_IHDR_t IHDR;
} png_props_t;

#define TRUE 1
#define FALSE 0
#define chunk_IHDR_type "IHDR"
#define chunk_gAMA_type "gAMA"
#define chunk_IDAT_type "IDAT"
#define chunk_IEND_type "IEND"

#define BIT_DEPTH 8
#define COLOR_TYPE 6
#define BYTES_PER_PIXEL 4 // RGBA

const uint8_t chunk_length_offset = 4;
const uint8_t chunk_type_offset = 4;
uint32_t chunk_data_offset = 0; // Being compute anon.
const uint8_t chunk_crc_offset = 4;

uint32_t swap_endianess(const uint32_t bytes);

chunk_t parse_chunk(unsigned char* png_buffer, size_t* current_offset);
int parse_chunk_IHDR(png_props_t* png_props, const chunk_t* chunk);

uint8_t recon_a(const uint8_t* pixels_data, const size_t x, const size_t y, const size_t scanline);
uint8_t recon_b(const uint8_t* pixels_data, const size_t x, const size_t y, const size_t scanline);
uint8_t recon_c(const uint8_t* pixels_data, const size_t x, const size_t y, const size_t scanline);
uint8_t paeth_predictor(const uint8_t recon_a, const uint8_t recon_b, const uint8_t recon_c);

void print_info(const chunk_IHDR_t* png_props);