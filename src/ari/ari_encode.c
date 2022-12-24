#include "../lzari_defs.h"
#include "ari_internal.h"
#include <stdlib.h>

struct {
    uint64_t whole;
    size_t forwarding;
    uint64_t low;
    uint64_t high;
    uint8_t offset_bit;
    uint8_t edge_case;   // boolean for if block was filled and contents encoded
                         // at the same time
    uint8_t rewrite;
} g_encoder = {0};

/* Helpers */

static size_t
ari_num_bits(size_t integer) {
    size_t num_bits;
    for (num_bits = 0; integer != 0; integer >>= 1, num_bits++) {
    }
    return num_bits;
}

/* Emitters */
static void
ari_emit_to_offset(uint8_t byte, size_t num_bits, block_t *block) {
    uint8_t write_byte = byte << (g_encoder.offset_bit - num_bits);
    uint8_t mask = (1UL << g_encoder.offset_bit) - 1;
    block->contents[block->size] =
        (block->contents[block->size] & ~mask) | write_byte;
    g_encoder.offset_bit -= num_bits;
}

static int
ari_emit_bits(size_t integer, size_t num_bits, block_t *block) {
    size_t free_bits;
    size_t left_bits;

    free_bits =
        g_encoder.offset_bit + ((block->capacity - block->size) * 8);
    if ((free_bits < num_bits) || (block->size >= block->capacity)) {
        return 1;
    }

    left_bits = num_bits;

    if (num_bits < g_encoder.offset_bit) {
        ari_emit_to_offset(integer, num_bits, block);
        return 0;
    } else {
        left_bits -= g_encoder.offset_bit;
        ari_emit_to_offset(integer >> (num_bits - g_encoder.offset_bit),
                           g_encoder.offset_bit, block);
        block->size++;
        g_encoder.offset_bit = 8;
    }

    while (left_bits >= 8) {
        left_bits -= 8;
        block->contents[block->size++] = (uint8_t)(integer >> left_bits);
    }

    if (left_bits == 0) {
        return 0;
    }
    ari_emit_to_offset(integer & ((1UL << left_bits) - 1), left_bits, block);
    return 0;
}

static void
ari_emit_int(size_t integer, block_t *block) {
    size_t num_bits = ari_num_bits(integer);
    size_t num_num_bits = ari_num_bits(num_bits);
    ari_emit_bits((1UL << num_num_bits) - 1, num_num_bits, block);
    ari_emit_bits(0, 1, block);
    ari_emit_bits(num_bits, num_num_bits, block);
    ari_emit_bits(integer, num_bits, block);
}

/* API */

void
ari_encode_init_global_encoder(uint8_t block_bits) {
    g_encoder.whole = (1UL << (64 - block_bits - 1));
    g_encoder.forwarding = 0;
    g_encoder.low = 0;
    g_encoder.high = g_encoder.whole;
    g_encoder.offset_bit = 8;
    g_encoder.edge_case = 0;
    g_encoder.rewrite = 0;
}

void
ari_encode_init_dist_table(lz_t *lz) {
    size_t order_idx;
    size_t unique_idx;
    uint16_t cur_min;
    uint16_t last_min;
    uint16_t cur_distribution;
    uint16_t amt;
    uint16_t cumulative_distribution;

    // allocate the order array
    lz->distributions_table.symbols_set_size = 0;
    for (size_t i = 0; i < SYMBOLS_AMT; i++) {
        if (lz->distributions[i] != 0) {
            lz->distributions_table.symbols_set_size++;
        }
    }
    lz->distributions_table.order =
        malloc(lz->distributions_table.symbols_set_size * sizeof(uint16_t));

    // populate the order array, allocate the unique & amt arrays
    last_min = 0;
    order_idx = 0;
    cumulative_distribution = 0;
    lz->distributions_table.unique_dist_size = 0;
    while (order_idx < lz->distributions_table.symbols_set_size) {
        cur_min = UINT16_MAX;
        for (int j = 0; j < SYMBOLS_AMT; j++) {
            if ((lz->distributions[j] > last_min) &&
                (lz->distributions[j] < cur_min)) {
                cur_min = lz->distributions[j];
            }
        }
        for (int j = 0; j < SYMBOLS_AMT; j++) {
            if (lz->distributions[j] == cur_min) {
                lz->distributions_table.order[order_idx] = j;
                lz->distributions_table.inverse_order[j] = order_idx;
                order_idx++;
            }
        }
        last_min = cur_min;
        lz->distributions_table.unique_dist_size++;
    }

    lz->distributions_table.unique =
        malloc(lz->distributions_table.unique_dist_size * sizeof(uint16_t));
    lz->distributions_table.amt =
        malloc(lz->distributions_table.unique_dist_size * sizeof(uint16_t));

    // populate the unique & amt arrays
    amt = 0;
    cur_distribution = lz->distributions[lz->distributions_table.order[0]];
    unique_idx = 0;
    for (size_t i = 0; i < lz->distributions_table.symbols_set_size; i++) {
        if (cur_distribution !=
            lz->distributions[lz->distributions_table.order[i]]) {
            lz->distributions_table.unique[unique_idx] = cur_distribution;
            lz->distributions_table.amt[unique_idx] = amt;
            unique_idx++;
            cur_distribution =
                lz->distributions[lz->distributions_table.order[i]];
            amt = 0;
        }
        amt++;
        cumulative_distribution +=
            lz->distributions[lz->distributions_table.order[i]];
        lz->distributions[lz->distributions_table.order[i]] =
            cumulative_distribution;
    }
    lz->distributions_table.unique[unique_idx] = cur_distribution;
    lz->distributions_table.amt[unique_idx] = amt;
}

void
ari_encode_emit_dist_table(lz_t *lz, block_t *block) {
    // emit symbols
    for (size_t i = 0; i < lz->distributions_table.symbols_set_size; i++) {
        ari_emit_bits(lz->distributions_table.order[i], SYMBOLS_BITS, block);
    }
    ari_emit_bits(EOF_SYMBOL, SYMBOLS_BITS, block);

    // emit distributions
    for (size_t i = 0; i < lz->distributions_table.unique_dist_size; i++) {
        ari_emit_int(lz->distributions_table.unique[i], block);
        ari_emit_int(lz->distributions_table.amt[i], block);
    }

    ari_emit_bits(0, g_encoder.offset_bit, block);
}

int
ari_encode_edge_case(lz_t *lz, block_t *block) {
    if (g_encoder.edge_case) {
        lz->idx++;
        ari_encode_ending(lz, block);
    }
    return g_encoder.edge_case;
}

void
ari_encode_rewrite(block_t *block) {
    if (g_encoder.rewrite) {
        block->contents[0] = block->contents[block->size - 1];
        block->size = 1;
        g_encoder.rewrite = 0;
    }
}

void
ari_encode_ending(lz_t *lz, block_t *block) {
    int status;

    g_encoder.forwarding++;
    if (g_encoder.low <= (g_encoder.whole >> 2)) {
        status = ari_emit_bits((1UL << g_encoder.forwarding) - 1,
                               g_encoder.forwarding + 1, block);
        if (status != 0) {
            g_encoder.edge_case = 1;
            lz->idx--;
            g_encoder.forwarding--;
        }
    } else {
        status = ari_emit_bits((1UL << g_encoder.forwarding),
                               g_encoder.forwarding + 1, block);
        if (status != 0) {
            g_encoder.edge_case = 1;
            lz->idx--;
            g_encoder.forwarding--;
        }
    }

    if (status == 0 && g_encoder.offset_bit < 8) {
        ari_emit_bits(0, g_encoder.offset_bit, block);
    }
}

int
ari_encode_next_symbol(lz_t *lz, block_t *block) {
    uint64_t interval_size;
    uint16_t symbol;
    uint64_t symbol_high;
    uint64_t symbol_low;
    int status;

    symbol = lz->contents[lz->idx];
    symbol_high = lz->distributions[symbol];
    symbol_low =
        (lz->distributions_table.inverse_order[symbol] == 0)
            ? 0
            : lz->distributions
                  [lz->distributions_table.order
                       [lz->distributions_table.inverse_order[symbol] - 1]];

    interval_size = g_encoder.high - g_encoder.low;
    g_encoder.high = g_encoder.low + ((interval_size * symbol_high) / lz->size);
    g_encoder.low = g_encoder.low + ((interval_size * symbol_low) / lz->size);
    while ((g_encoder.high < (g_encoder.whole >> 1)) || (g_encoder.low > (g_encoder.whole >> 1))) {
        if (g_encoder.high < (g_encoder.whole >> 1)) {
            status = ari_emit_bits((1UL << g_encoder.forwarding) - 1, g_encoder.forwarding + 1, block);
            if (status != 0) {
                g_encoder.rewrite = 1;
                return status;
            }
            g_encoder.low <<= 1;
            g_encoder.high <<= 1;
            g_encoder.forwarding = 0;
        } else {
            status = ari_emit_bits(1UL << g_encoder.forwarding, g_encoder.forwarding + 1, block);
            if (status != 0) {
                g_encoder.rewrite = 1;
                return status;
            }
            g_encoder.low = (g_encoder.low - (g_encoder.whole >> 1)) << 1;
            g_encoder.high = (g_encoder.high - (g_encoder.whole >> 1)) << 1;
            g_encoder.forwarding = 0;
        }
    }

    while ((g_encoder.low > (g_encoder.whole >> 2)) && (g_encoder.high < (3 * (g_encoder.whole >> 2)))) {
        g_encoder.low = (g_encoder.low - (g_encoder.whole >> 2)) << 1;
        g_encoder.high = (g_encoder.high - (g_encoder.whole >> 2)) << 1;
        g_encoder.forwarding++;
    }

    lz->idx++;
    return 0;
}