#ifndef LZ_H
#define LZ_H

#include "../lzari_defs.h"

void lzss_compress(block_t *block, lz_t *lz);

void lzss_decompress(block_t *block, lz_t *lz);

#endif   // LZ_H
