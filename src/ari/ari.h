#ifndef ARI_H
#define ARI_H

#include "../lzari_defs.h"

void ari_encode_init(lz_t *lz, block_t *block, uint8_t block_bits);

void ari_encode(lz_t *lz, block_t *block);

void ari_decode_init(lz_t *lz, block_t *block, uint8_t block_bits);

size_t ari_decode(lz_t *lz, block_t *block);

#endif   // ARI_H