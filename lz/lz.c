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
find_lz_pair(file_t *file, window_t window) {
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
               (file->contents[cur_match_checking_idx] ==
                file->contents[candidate_match_checking_idx])) {
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
add_pair(lz_t *lz, lz_pair_t lz_pair) {
    lz->contents[lz->size++] = SYMBOL(lz_pair.distance);
    lz->contents[lz->size++] = SYMBOL(lz_pair.length);
    if (lz->distributions[SYMBOL(lz_pair.distance)] == 0) {
        lz->symbols_set_size++;
    }
    if (lz->distributions[SYMBOL(lz_pair.length)] == 0) {
        lz->symbols_set_size++;
    }
    lz->distributions[SYMBOL(lz_pair.distance)]++;
    lz->distributions[SYMBOL(lz_pair.length)]++;
}

static void
add_literal(lz_t *lz, uint8_t literal) {
    lz->contents[lz->size++] = literal;
    lz->distributions[literal]++;
}

static void
add_symbol(lz_t *lz, uint16_t symbol) {
    lz->contents[lz->size++] = symbol;
    if (lz->distributions[symbol] == 0) {
        lz->symbols_set_size++;
    }
    lz->distributions[symbol]++;
}

static void
advance_window(window_t *window, uint16_t length, size_t limit_idx) {
    window->cur_idx += length;
    if (window->cur_idx - window->start_idx > (WINDOW_SIZE / 2)) {
        window->start_idx += length;
        if (window->end_idx < limit_idx) {
            window->end_idx = MIN((window->end_idx + length), limit_idx);
        }
    }
}

/* API */

lz_t
lzss_compress(file_t *file) {
    lz_t lz;
    lz_pair_t lz_pair;
    window_t window = {
        .start_idx = 0, .cur_idx = 0, .end_idx = MIN(WINDOW_SIZE, file->size)};

    while (window.cur_idx < window.end_idx) {
        lz_pair = find_lz_pair(file, window);
        if (lz_pair.distance > 0) {
            add_symbol(&lz, SYMBOL(lz_pair.distance));
            add_symbol(&lz, SYMBOL(lz_pair.length));
        } else {
            add_symbol(&lz, file->contents[window.cur_idx]);
        }
        advance_window(&window, lz_pair.length, file->size);
    }
    return lz;
}