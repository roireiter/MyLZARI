#ifndef LZARI_DEFS_H
#define LZARI_DEFS_H

#include <stdint.h>
#include <stdio.h>

/* Parameter definitions */
#define BLOCK_SIZE    (1 << 15)
#define SYMBOLS_BITS  12
#define LITERALS_BITS 8
#define SYMBOLS_AMT   (1 << SYMBOLS_BITS)
#define LITERALS_AMT  (1 << LITERALS_BITS)
#define WINDOW_SIZE   ((SYMBOLS_AMT - LITERALS_AMT) * 2)
#define SYMBOL(x)     (x + LITERALS_AMT)
#define MIN(x, y)     ((y < x) ? y : x)
#define MAX(x, y)     ((y > x) ? y : x)

/* String definitions */
#define HELP_MSG                                                               \
    "Please provide flag (-c for compress, -d for decompress), as well as a "  \
    "path to your file.\n"
#define INVALID_FILE_MSG "Problem accessing given file. Operation failed.\n"
#define WRITE_ERROR_MSG  "Problem outputting the compressed/decompressed file.\n"
#define DONE_MSG "Process completed! Your file is located at output directory.\n"

/* Struct definitions */
typedef struct {
    size_t size;
    uint8_t contents[BLOCK_SIZE];
} block_t;

typedef struct {
    size_t size;
    uint16_t contents[2 * BLOCK_SIZE];
    uint16_t distributions[SYMBOLS_AMT];
    size_t symbols_set_size;
} lz_t;

typedef struct {
    size_t encoding;
    size_t size;
    uint16_t *cumulative;
    uint16_t *symbols;
} ari_t;

#endif   // LZARI_DEFS_H
