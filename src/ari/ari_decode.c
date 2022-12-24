#include "../lzari_defs.h"
#include "ari_internal.h"
#include <stdlib.h>

struct {
    uint64_t low;
    uint64_t high;
    uint64_t binary_approx;
    uint8_t offset_bit;
    size_t block_idx;
    uint8_t started_decoding_contents;
    uint8_t block_finished;
} g_decoder = {0};

/* Readers */

static void
ari_read_offset(uint16_t *output, size_t num_bits, block_t *block) {
    uint8_t read_byte = block->contents[g_decoder.block_idx];
    uint8_t mask = ((1U << g_decoder.offset_bit) - 1) -
                   ((1U << (g_decoder.offset_bit - num_bits)) - 1);
    read_byte = (read_byte & mask) >> (g_decoder.offset_bit - num_bits);
    mask >>= (g_decoder.offset_bit - num_bits);
    *output = (*output & ~mask) | read_byte;
    g_decoder.offset_bit -= num_bits;
}

static int
ari_read_bits(uint16_t *output, size_t num_bits, block_t *block) {
    size_t unread_bits;
    size_t left_bits;

    unread_bits =
        g_decoder.offset_bit + ((block->size - g_decoder.block_idx) * 8);
    if ((unread_bits < num_bits) || (g_decoder.block_idx >= block->size)) {
        return 1;
    }

    left_bits = num_bits;
    if (num_bits < g_decoder.offset_bit) {
        ari_read_offset(output, num_bits, block);
        return 0;
    } else {
        left_bits -= g_decoder.offset_bit;
        ari_read_offset(output, g_decoder.offset_bit, block);
        g_decoder.block_idx++;
        g_decoder.offset_bit = 8;
    }

    while (left_bits >= 8) {
        left_bits -= 8;
        *output = *output << 8;
        *output |= block->contents[g_decoder.block_idx++];
    }

    if (left_bits == 0) {
        return 0;
    }

    *output = *output << left_bits;
    ari_read_offset(output, left_bits, block);

    return 0;
}

static int
ari_read_int(uint16_t *output, block_t *block) {
    uint16_t num_num_bits;
    uint16_t num_bits;
    uint16_t ones;

    num_num_bits = 0;
    num_bits = 0;
    do {
        ones = 0;
        ari_read_bits(&ones, 1, block);
        num_num_bits++;
    } while (ones == 1);
    num_num_bits--;
    ari_read_bits(&num_bits, num_num_bits, block);
    ari_read_bits(output, num_bits, block);
}

void
ari_decode_init_approx(block_t *block) {
    uint16_t bit_read;
    size_t precision = 1;

    while (precision <= PRECISION_BITS) {
        bit_read = 0;
        ari_read_bits(&bit_read, 1, block);
        if (bit_read == 1) {
            g_decoder.binary_approx += (1UL << (PRECISION_BITS - precision));
        }
        precision++;
    }
}

static void
ari_decode_ending(void) {
    size_t rewind_bits_amt;
    size_t left_bits;
    uint16_t bit_read;

    rewind_bits_amt = 0;
    while ((g_decoder.low < g_decoder.binary_approx) && (g_decoder.binary_approx  < g_decoder.high) &&
    (g_decoder.low < (g_decoder.binary_approx ^ 1)) && ((g_decoder.binary_approx ^ 1)  < g_decoder.high)) {
        g_decoder.low >>= 1;
        g_decoder.high >>= 1;
        g_decoder.binary_approx >>= 1;
        rewind_bits_amt++;
    }
    while (rewind_bits_amt > 0) {
        if (g_decoder.offset_bit == 8) {
            g_decoder.offset_bit = 0;
            g_decoder.block_idx--;
        }
        g_decoder.offset_bit++;
        rewind_bits_amt--;
    }

    if (g_decoder.offset_bit < 8) {
        g_decoder.offset_bit = 8;
        g_decoder.block_idx++;
    }
}

/* API */

void
ari_decode_init_global_decoder(void) {
    g_decoder.low = 0;
    g_decoder.high = WHOLE;
    g_decoder.binary_approx = 0;
    g_decoder.started_decoding_contents = 0;
}

void
ari_read_reset(block_t *block) {
    g_decoder.block_idx = 0;
    g_decoder.offset_bit = 8;
}

void
ari_decode_allocate_dist_table(lz_t *lz, block_t *block) {
    uint16_t symbol;
    uint16_t distribution;
    uint16_t amt;
    uint16_t total_amt;

    lz->distributions_table.symbols_set_size = 0;
    lz->distributions_table.unique_dist_size = 0;
    total_amt = 0;
    // Two loops are necessary to skip EOF symbol once, it is needed for the
    // distribution order and for done reading.
    do {
        symbol = 0;
        ari_read_bits(&symbol, SYMBOLS_BITS, block);
        lz->distributions_table.symbols_set_size++;
    } while (symbol != EOF_SYMBOL);
    do {
        symbol = 0;
        ari_read_bits(&symbol, SYMBOLS_BITS, block);
        lz->distributions_table.symbols_set_size++;
    } while (symbol != EOF_SYMBOL);
    lz->distributions_table.symbols_set_size--;

    lz->distributions_table.order =
        malloc(sizeof(uint16_t) * lz->distributions_table.symbols_set_size);

    while (total_amt != lz->distributions_table.symbols_set_size) {
        distribution = 0;
        amt = 0;
        ari_read_int(&distribution, block);
        ari_read_int(&amt, block);
        lz->distributions_table.unique_dist_size++;
        total_amt += amt;
    }

    lz->distributions_table.unique =
        malloc(sizeof(uint16_t) * lz->distributions_table.unique_dist_size);
    lz->distributions_table.amt =
        malloc(sizeof(uint16_t) * lz->distributions_table.unique_dist_size);

    ari_read_reset(block);
}

void
ari_decode_init_dist_table(lz_t *lz, block_t *block) {
    uint16_t symbol;
    uint16_t distribution;
    uint16_t amt;
    uint16_t cumulative_distribution;
    size_t total_amt;

    for (size_t i = 0; i < lz->distributions_table.symbols_set_size; i++) {
        symbol = 0;
        ari_read_bits(&symbol, SYMBOLS_BITS, block);
        lz->distributions_table.order[i] = symbol;
    }
    ari_read_bits(&symbol, SYMBOLS_BITS, block);   // read EOF
    lz->size = 0;
    total_amt = 0;
    cumulative_distribution = 0;
    for (size_t i = 0; i < lz->distributions_table.unique_dist_size; i++) {
        distribution = 0;
        amt = 0;
        ari_read_int(&distribution, block);
        ari_read_int(&amt, block);
        lz->distributions_table.unique[i] = distribution;
        lz->distributions_table.amt[i] = amt;
        for (size_t j = total_amt; j < total_amt + amt; j++) {
            cumulative_distribution += distribution;
            lz->distributions[lz->distributions_table.order[j]] =
                cumulative_distribution;
        }
        total_amt += amt;
        lz->size += (amt * distribution);
    }
    ari_read_bits(&symbol, g_decoder.offset_bit, block);
}

size_t
ari_decode_main(lz_t *lz, block_t *block) {
    uint64_t interval_size;
    uint16_t bit_read;
    uint64_t high;
    uint64_t low;
    uint16_t symbol;
    uint16_t symbol_high;
    uint16_t symbol_low;
    int status;

    if (!g_decoder.started_decoding_contents) {
        g_decoder.started_decoding_contents = 1;
        ari_decode_init_approx(block);
    }

    g_decoder.block_finished = 0;

    while (1) {
        for (size_t i = 0; i < lz->distributions_table.symbols_set_size; i++) {
            symbol = lz->distributions_table.order[i];
            symbol_high = lz->distributions[symbol];
            symbol_low = (i == 0) ? 0 : lz->distributions[lz->distributions_table.order[i - 1]];

            interval_size = g_decoder.high - g_decoder.low;
            high = g_decoder.low + ((interval_size * symbol_high) / lz->size);
            low = g_decoder.low + ((interval_size * symbol_low) / lz->size);

            if (low <= g_decoder.binary_approx &&
                g_decoder.binary_approx < high) {
                lz->contents[lz->idx++] = symbol;
                g_decoder.high = high;
                g_decoder.low = low;
                if (symbol == EOF_SYMBOL) {
                    if (!g_decoder.block_finished) {
                        ari_decode_ending();
                        
                    }
                    return g_decoder.block_idx;
                }
                break;
            }
        }

        while (g_decoder.high < HALF || g_decoder.low > HALF) {
            if (g_decoder.high < HALF) {
                g_decoder.low <<= 1;
                g_decoder.high <<= 1;
                g_decoder.binary_approx <<= 1;
            } else {
                g_decoder.low = (g_decoder.low - HALF) << 1;
                g_decoder.high = (g_decoder.high - HALF) << 1;
                g_decoder.binary_approx = (g_decoder.binary_approx - HALF) << 1;
            }
            bit_read = 0;
            status = ari_read_bits(&bit_read, 1, block);
            if (status != 0) {
                g_decoder.block_finished = 1;
                continue;
            }
            if (bit_read == 1) {
                g_decoder.binary_approx += 1;
            }
        }

        while ((g_decoder.low > QUARTER) && (g_decoder.high < (3 * QUARTER))) {
            g_decoder.low = (g_decoder.low - QUARTER) << 1;
            g_decoder.high = (g_decoder.high - QUARTER) << 1;
            g_decoder.binary_approx = (g_decoder.binary_approx - QUARTER) << 1;
            bit_read = 0;
            status = ari_read_bits(&bit_read, 1, block);

            if (status != 0) {
                g_decoder.block_finished = 1;
                continue;
            }
            if (bit_read == 1) {
                g_decoder.binary_approx += 1;
            }
        }
    }
    return g_decoder.block_idx;
}
