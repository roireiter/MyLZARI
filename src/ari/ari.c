#include "ari.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: DELETE THIS
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                   \
    (byte & 0x80 ? '1' : '0'), (byte & 0x40 ? '1' : '0'),                      \
        (byte & 0x20 ? '1' : '0'), (byte & 0x10 ? '1' : '0'),                  \
        (byte & 0x08 ? '1' : '0'), (byte & 0x04 ? '1' : '0'),                  \
        (byte & 0x02 ? '1' : '0'), (byte & 0x01 ? '1' : '0')

struct {
    size_t forwarding;
    uint64_t low;
    uint64_t high;
    uint8_t offset_bit;
    uint8_t edge_case;   // boolean for if block was filled and contents encoded
                         // at the same time
    uint8_t rewrite;
} g_encoder = {0};

static size_t
ari_num_bits(size_t integer) {
    size_t num_bits;
    for (num_bits = 0; integer != 0; integer >>= 1, num_bits++) {
    }
    return num_bits;
}

static uint64_t
ari_find_symbol_low(lz_t *lz, uint16_t symbol) {
    uint64_t symbol_low = 0;
    int idx = (int)symbol;

    while (idx-- >= 0) {
        if (lz->distributions[idx] != 0) {
            symbol_low = lz->distributions[idx];
            break;
        }
    }

    return symbol_low;
}

/* Emit functions */

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
        g_encoder.offset_bit + ((sizeof(block->contents) - block->size) * 8);
    if (free_bits < num_bits) {
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

static void
ari_encode_ending(lz_t *lz, block_t *block) {
    int status;

    if (g_encoder.low <= QUARTER) {
        status = ari_emit_bits((1UL << g_encoder.forwarding),
                               g_encoder.forwarding + 1, block);
        if (status != 0) {
            g_encoder.edge_case = 1;
            lz->encoding_idx--;
        }
    } else {
        status = ari_emit_bits((1UL << g_encoder.forwarding) - 1,
                               g_encoder.forwarding + 1, block);
        if (status != 0) {
            g_encoder.edge_case = 1;
            lz->encoding_idx--;
        }
    }
}

/* Init functions */

static void
ari_init_global_encoder(void) {
    g_encoder.forwarding = 0;
    g_encoder.low = 0;
    g_encoder.high = WHOLE;
    g_encoder.offset_bit = 8;
    g_encoder.edge_case = 0;
    g_encoder.rewrite = 0;
}

static void
ari_init_dist_table(lz_t *lz) {
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
        cumulative_distribution += lz->distributions[lz->distributions_table.order[i]];
        lz->distributions[lz->distributions_table.order[i]] = cumulative_distribution;
    }
    return;
    lz->distributions_table.unique[unique_idx] = cur_distribution;
    lz->distributions_table.amt[unique_idx] = amt;
}

static void
ari_emit_dist_table(lz_t *lz, block_t *block) {
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
}

void
ari_init(lz_t *lz, block_t *block) {
    ari_init_global_encoder();

    ari_init_dist_table(lz);
    ari_emit_dist_table(lz, block);

}

void
ari_encode(lz_t *lz, block_t *block) {
    uint64_t interval_size;
    uint16_t symbol;
    uint64_t symbol_high;
    uint64_t symbol_low;
    size_t emit;
    int status;

    if (g_encoder.edge_case) {
        lz->encoding_idx++;
        ari_encode_ending(lz, block);
        return;
    }

    // re-writing last written byte for bit accuracy
    if (g_encoder.rewrite) {
        block->contents[0] = block->contents[block->size - 1];
        block->size = 1;
        g_encoder.rewrite = 0;
    }

    while (lz->encoding_idx < lz->size) {
        symbol = lz->contents[lz->encoding_idx];
        symbol_high = lz->distributions[symbol];
        symbol_low =
            (lz->distributions_table.inverse_order[symbol] == 0)
                ? 0
                : lz->distributions
                      [lz->distributions_table.order
                           [lz->distributions_table.inverse_order[symbol] - 1]];

        interval_size = g_encoder.high - g_encoder.low;
        g_encoder.high =
            g_encoder.low + ((interval_size * symbol_high) / lz->size);
        g_encoder.low =
            g_encoder.low + ((interval_size * symbol_low) / lz->size);
        while (g_encoder.high < HALF || g_encoder.low > HALF) {
            if (g_encoder.high < HALF) {
                emit = (1UL << g_encoder.forwarding);
                status = ari_emit_bits(emit,
                                       g_encoder.forwarding + 1, block);
                if (status != 0) {
                    g_encoder.rewrite = 1;
                    break;
                }
                g_encoder.low <<= 1;
                g_encoder.high <<= 1;
                g_encoder.forwarding = 0;
            } else {
                emit = (1UL << g_encoder.forwarding) - 1;
                status = ari_emit_bits(emit,
                                       g_encoder.forwarding + 1, block);
                if (status != 0) {
                    g_encoder.rewrite = 1;
                    break;
                }
                g_encoder.low = (g_encoder.low - HALF) << 1;
                g_encoder.high = (g_encoder.high - HALF) << 1;
                g_encoder.forwarding = 0;
            }
        }

        while ((g_encoder.low > QUARTER) && (g_encoder.high < (3 * QUARTER))) {
            g_encoder.low = (g_encoder.low - QUARTER) << 1;
            g_encoder.high = (g_encoder.high - QUARTER) << 1;
            g_encoder.forwarding++;
        }

        lz->encoding_idx++;
    }
    if (lz->encoding_idx == lz->size) {
        g_encoder.forwarding++;
        ari_encode_ending(lz, block);
        lz->encoding_idx++;
    }

    // for (int i = 0; i < block->size; i++) {
    //     printf(BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(block->contents[i]));
    // }
    printf("BLOCK SIZE = %u\n", block->size);
}