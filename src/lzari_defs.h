#ifndef LZARI_DEFS_H
#define LZARI_DEFS_H

#include <stdint.h>
#include <stdio.h>

/* Parameter definitions */
#define LITERALS_BITS 8

#define LITERALS_AMT (1UL << LITERALS_BITS)
#define EOF_SYMBOL   256   // luckily this symbol will never be used

#define SYMBOL(x)  (x + LITERALS_AMT)
#define LITERAL(x) (x - LITERALS_AMT)
#define MIN(x, y)  ((y < x) ? y : x)
#define MAX(x, y)  ((y > x) ? y : x)

/* String definitions */
#define HELP_MSG                                                               \
    "Please provide flag (-c for compress, -d for decompress), as well as a "  \
    "path to your file.\n"
#define INVALID_FILE_MSG "Problem accessing given file. Operation failed.\n"
#define WRITE_ERROR_MSG  "Problem outputting the compressed/decompressed file.\n"

/* Struct definitions */
typedef struct {
    size_t symbols_set_size;
    size_t unique_dist_size;
    uint16_t *order;
    uint16_t *inverse_order;
    uint16_t *unique;
    uint16_t *amt;
} dist_table_t;

typedef struct {
    size_t size;
    size_t capacity;
    uint8_t *contents;
} block_t;

typedef struct {
    size_t size;
    size_t symbols_amt;
    size_t symbol_bits;
    uint16_t *contents;
    uint16_t *distributions;
    size_t idx;
    dist_table_t distributions_table;
} lz_t;

typedef struct {
    uint8_t offset;
    uint8_t last_byte;
} ari_t;

#endif   // LZARI_DEFS_H
