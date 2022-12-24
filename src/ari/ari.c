#include "ari.h"
#include "ari_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void
ari_encode_init(lz_t *lz, block_t *block, uint8_t block_bits) {
    ari_encode_init_global_encoder(block_bits);

    ari_encode_init_dist_table(lz);
    ari_encode_emit_dist_table(lz, block);
}

void
ari_encode(lz_t *lz, block_t *block) {
    int status;

    status = ari_encode_edge_case(lz, block);
    if (status == 1) {
        return;
    }
    ari_encode_rewrite(block);

    while (lz->idx < lz->size) {
        status = ari_encode_next_symbol(lz, block);
        if (status == 1) {
            break;
        }
    }
    if (status == 0) {
        ari_encode_ending(lz, block);
    }
}

void
ari_decode_init(lz_t *lz, block_t *block, uint8_t block_bits) {
    ari_decode_init_global_decoder(block_bits);
    ari_read_reset(block);
    ari_decode_allocate_dist_table(lz, block);
    ari_decode_init_dist_table(lz, block);
}

size_t
ari_decode(lz_t *lz, block_t *block) {
    return ari_decode_main(lz, block);
}
