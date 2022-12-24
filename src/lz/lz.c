#include "lz.h"
#include <stdint.h>
#include <stdio.h>

#define BREAK_EVEN_LENGTH 2

typedef struct {
    uint16_t distance;
    uint16_t length;
} lz_pair_t;

typedef struct {
    size_t start_idx;
    size_t end_idx;
    size_t cur_idx;
} window_t;

/*
The distance is bound by `window.cur_idx` - `window.start_idx`.
The length is bound by `window.end_idx` - `window.cur_idx`.
*/
static lz_pair_t
lz_find_lz_pair(block_t *block, window_t window) {
    size_t candidate_idx;
    uint16_t candidate_length;
    uint16_t candidate_distance;
    size_t candidate_match_checking_idx;
    size_t cur_match_checking_idx;
    lz_pair_t lz_pair = {.distance = 0, .length = 1};

    for (candidate_idx = window.start_idx; candidate_idx < window.cur_idx;
         candidate_idx++) {
        candidate_match_checking_idx = candidate_idx;
        cur_match_checking_idx = window.cur_idx;

        while ((cur_match_checking_idx < window.end_idx) &&
               (block->contents[cur_match_checking_idx] ==
                block->contents[candidate_match_checking_idx])) {
            cur_match_checking_idx++;
            candidate_match_checking_idx++;
        }

        candidate_distance = window.cur_idx - candidate_idx;
        candidate_length = candidate_match_checking_idx - candidate_idx;

        if (candidate_length > MAX(lz_pair.length, BREAK_EVEN_LENGTH)) {
            lz_pair.distance = candidate_distance;
            lz_pair.length = candidate_length;
        }
    }
    return lz_pair;
}

static void
lz_add_symbol(lz_t *lz, uint16_t symbol) {
    lz->contents[lz->size++] = symbol;
    lz->distributions[symbol]++;
}

static void
lz_advance_window(window_t *window, uint16_t length, size_t limit_idx) {
    window->cur_idx += length;
    if (window->cur_idx - window->start_idx > (WINDOW_SIZE / 2)) {
        window->start_idx += length;
        if (window->end_idx < limit_idx) {
            window->end_idx = MIN((window->end_idx + length), limit_idx);
        }
    }
}

/* API */

void
lzss_compress(block_t *block, lz_t *lz) {
    lz_pair_t lz_pair;
    window_t window = {
        .start_idx = 0, .cur_idx = 0, .end_idx = MIN(WINDOW_SIZE, block->size)};

    while (window.cur_idx < window.end_idx) {
        lz_pair = lz_find_lz_pair(block, window);
        if (lz_pair.distance > 0) {
            lz_add_symbol(lz, SYMBOL(lz_pair.distance));
            lz_add_symbol(lz, SYMBOL(lz_pair.length));
        } else {
            lz_add_symbol(lz, block->contents[window.cur_idx]);
        }
        lz_advance_window(&window, lz_pair.length, block->size);
    }
    lz_add_symbol(lz, EOF_SYMBOL);
}

void
lzss_decompress(block_t *block, lz_t *lz) {
    uint16_t symbol;
    uint16_t distance;
    uint16_t length;
    size_t start_idx;
    size_t end_idx;
    
    lz->idx = 0;
    while (1) {
        symbol = lz->contents[lz->idx++];
        if (symbol == EOF_SYMBOL) {
            break;
        }
        
        if (symbol <= LITERALS_AMT) {
            block->contents[block->size++] = symbol;
            continue;
        }

        distance = LITERAL(symbol);
        symbol = lz->contents[lz->idx++];
        length = LITERAL(symbol);
        start_idx = block->size - distance;
        end_idx = start_idx + length;

        for (int i = start_idx; i < end_idx; i++) {
            block->contents[block->size++] = block->contents[i];
        }
    }
}