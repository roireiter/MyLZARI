#ifndef ARI_H
#define ARI_H

#include "../lzari_defs.h"

void ari_init(lz_t *lz, block_t *block);

void ari_encode(lz_t *lz, block_t *block);

#endif   // ARI_H