#include "ari.h"
#include <stdint.h>

ari_t
ari_encode(lz_t *lz) {
    ari_t ari;
    size_t cumulative_value;

    ari.cumulative = calloc(sizeof(uint16_t) * (lz->symbols_set_size + 1));
    ari.symbols = malloc(sizeof(uint16_t) * lz->symbols_set_size);

    cumulative_value = 0;
    for (size_t i = 0, j = 0; i < lz->size; i++) {
        if (lz->distributions[i] != 0) {
            cumulative_value += lz->distributions[i];
            ari.cumulative[j + 1] = cumulative_value;
            ari.symbols[j] = i;
            j++;
        }
    }

    return ari;
}