#ifndef LZARI_DEFS_H
#define LZARI_DEFS_H

#include <stdint.h>
#include <stdio.h>

/* Parameter definitions */
#define BLOCK_BITS     17
#define SYMBOLS_BITS   10
#define LITERALS_BITS  8
#define PRECISION_BITS (64 - BLOCK_BITS - 1)   // using uint64_t

#define BLOCK_SIZE   (1UL << BLOCK_BITS)
#define SYMBOLS_AMT  (1UL << SYMBOLS_BITS)
#define LITERALS_AMT (1UL << LITERALS_BITS)
#define WINDOW_SIZE  ((SYMBOLS_AMT - LITERALS_AMT) * 2)
#define EOF_SYMBOL   256   // luckily this symbol will never be used

#define WHOLE   (1UL << PRECISION_BITS)
#define HALF    (1UL << (PRECISION_BITS - 1))
#define QUARTER (1UL << (PRECISION_BITS - 2))

#define SYMBOL(x) (x + LITERALS_AMT)
#define MIN(x, y) ((y < x) ? y : x)
#define MAX(x, y) ((y > x) ? y : x)

/* String definitions */
#define HELP_MSG                                                               \
    "Please provide flag (-c for compress, -d for decompress), as well as a "  \
    "path to your file.\n"
#define INVALID_FILE_MSG "Problem accessing given file. Operation failed.\n"
#define WRITE_ERROR_MSG  "Problem outputting the compressed/decompressed file.\n"
#define DONE_MSG                                                               \
    "Process completed! Your file is located at output directory.\n"
#define THANK_YOU_MSG "Thank you for choosing Roi's LZARI! Goodbye!\n"

/* Struct definitions */
typedef struct {
    size_t symbols_set_size;
    size_t unique_dist_size;
    uint16_t *order;
    uint16_t inverse_order[SYMBOLS_AMT];
    uint16_t *unique;
    uint16_t *amt;
} dist_table_t;

typedef struct {
    size_t size;
    uint8_t contents[BLOCK_SIZE];
} block_t;

typedef struct {
    size_t size;
    uint16_t contents[2 * BLOCK_SIZE];
    uint16_t distributions[SYMBOLS_AMT];
    size_t encoding_idx;
    dist_table_t distributions_table;
} lz_t;

typedef struct {
    uint8_t offset;
    uint8_t last_byte;
} ari_t;

#endif   // LZARI_DEFS_H
