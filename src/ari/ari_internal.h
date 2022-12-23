#ifndef ARI_INTERNAL_H
#define ARI_INTERNAL_H

void ari_encode_init_global_encoder(void);

void ari_encode_init_dist_table(lz_t *lz);

void ari_encode_emit_dist_table(lz_t *lz, block_t *block);

void ari_encode_ending(lz_t *lz, block_t *block);

int ari_encode_next_symbol(lz_t *lz, block_t *block);

int ari_encode_edge_case(lz_t *lz, block_t *block);

void ari_encode_rewrite(block_t *block);

void ari_decode_init_global_decoder(void);

void ari_read_reset(block_t *block);

void ari_decode_allocate_dist_table(lz_t *lz, block_t *block);

void ari_decode_init_dist_table(lz_t *lz, block_t *block);

size_t ari_decode_main(lz_t *lz, block_t *block);

#endif   // ARI_INTERNAL_H
