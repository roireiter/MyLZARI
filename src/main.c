#include "ari/ari.h"
#include "lz/lz.h"
#include "lzari_defs.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Welcome to Roi Reiter's amateur LZARI compressor/decompressor!
 * This algorithm is based on Haruhiko Okumara's "History of Data Compression
 * in Japan". https://oku.edu.mie-u.ac.jp/~okumura/compression/history Okumara
 * presents lots of ideas, I took the most basic. The algorithm implements LZSS
 * with arithmetic encoding, which we learned about in class from Prof. Or
 * Ordentlich. Feel free to look around, or try out the program yourself!
 */

/* Compression & Decompression functions */
static int
compress(char *filename, char *output_filename, int dry_run, uint8_t block_bits,
         uint8_t symbol_bits) {
    block_t block = {0};
    lz_t lz = {0};
    FILE *fp;
    FILE *fp_out;
    int dry_run_result = 0;
    uint8_t header = 0;
    size_t written_bytes_amt;
    size_t tmp = 0;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf(INVALID_FILE_MSG);
        return 1;
    }

    fp_out = fopen(output_filename, "wb");

    block.capacity = 1UL << block_bits;
    block.contents = calloc((1UL << block_bits), sizeof(uint8_t));
    lz.contents = calloc((1UL << (block_bits + 1)), sizeof(uint16_t));
    lz.distributions = calloc((1UL << symbol_bits), sizeof(uint16_t));
    lz.distributions_table.inverse_order =
        calloc((1UL << symbol_bits), sizeof(uint16_t));
    lz.symbols_amt = 1UL << symbol_bits;
    lz.symbol_bits = symbol_bits;
    if (!dry_run) {
        header = block_bits - 9;
        header |= ((symbol_bits - 9) << 4);
        written_bytes_amt = fwrite(&header, sizeof(uint8_t), 1, fp_out);
        if (written_bytes_amt != 1) {
            printf(WRITE_ERROR_MSG);
            return 1;
        }
    }

    while (1) {
        block.size = fread(block.contents, sizeof(uint8_t), block.capacity, fp);
        if (block.size == 0) {
            break;
        }
        lzss_compress(&block, &lz);
        memset(block.contents, 0, block.capacity);
        block.size = 0;
        ari_encode_init(&lz, &block, block_bits);
        while (1) {
            ari_encode(&lz, &block);
            if (dry_run) {
                dry_run_result += block.size;
            } else {
                written_bytes_amt =
                    fwrite(block.contents, sizeof(uint8_t), block.size, fp_out);
                if (written_bytes_amt != block.size) {
                    printf(WRITE_ERROR_MSG);
                    return 1;
                }
            }
            if (lz.idx == lz.size) {
                break;
            }
            fseek(fp_out, -1, SEEK_CUR);
        }
        free(lz.distributions_table.order);
        free(lz.distributions_table.unique);
        free(lz.distributions_table.amt);
        memset(lz.contents, 0, sizeof(uint16_t) * (1UL << (block_bits + 1)));
        memset(lz.distributions, 0, sizeof(uint16_t) * (1UL << symbol_bits));
        memset(lz.distributions_table.inverse_order, 0,
               sizeof(uint16_t) * (1UL << symbol_bits));
        lz.size = 0;
        lz.idx = 0;
        lz.distributions_table.symbols_set_size = 0;
        lz.distributions_table.unique_dist_size = 0;
        memset(block.contents, 0, block.capacity);
        block.size = 0;
    }

    fclose(fp);
    fclose(fp_out);
    free(block.contents);
    free(lz.contents);
    free(lz.distributions);
    free(lz.distributions_table.inverse_order);

    if (dry_run) {
        return dry_run_result;
    }
    return 0;
}

static int
decompress(char *filename, char *output_filename) {
    block_t block = {0};
    lz_t lz = {0};
    FILE *fp;
    FILE *fp_out;
    size_t bytes_processed;
    uint8_t header;
    uint8_t block_bits;
    uint8_t symbol_bits;
    size_t written_bytes_amt;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf(INVALID_FILE_MSG);
        return 1;
    }

    fp_out = fopen(output_filename, "wb");

    fread(&header, sizeof(uint8_t), 1, fp);
    block_bits = (header & 0xF) + 9;
    symbol_bits = ((header >> 4) & 0xF) + 9;
    block.capacity = 1UL << block_bits;
    block.contents = calloc((1UL << block_bits), sizeof(uint8_t));
    lz.contents = calloc((1UL << (block_bits + 1)), sizeof(uint16_t));
    lz.distributions = calloc((1UL << symbol_bits), sizeof(uint16_t));
    lz.distributions_table.inverse_order =
        calloc((1UL << symbol_bits), sizeof(uint16_t));
    lz.symbols_amt = 1UL << symbol_bits;
    lz.symbol_bits = symbol_bits;

    while (1) {

        block.size = fread(block.contents, sizeof(uint8_t), block.capacity, fp);
        if (block.size == 0) {
            break;
        }
        ari_decode_init(&lz, &block, block_bits);

        while (1) {
            bytes_processed = ari_decode(&lz, &block);
            if (lz.contents[lz.idx - 1] == EOF_SYMBOL) {
                fseek(fp, (int)bytes_processed - block.size, SEEK_CUR);
                break;
            }
            block.size =
                fread(block.contents, sizeof(uint8_t), block.capacity, fp);
        }

        memset(block.contents, 0, block.capacity);
        block.size = 0;
        lzss_decompress(&block, &lz);
        written_bytes_amt =
            fwrite(block.contents, sizeof(uint8_t), block.size, fp_out);
        if (written_bytes_amt != block.size) {
            printf(WRITE_ERROR_MSG);
            return 1;
        }

        free(lz.distributions_table.order);
        free(lz.distributions_table.unique);
        free(lz.distributions_table.amt);
        memset(lz.contents, 0, sizeof(uint16_t) * (1UL << (block_bits + 1)));
        memset(lz.distributions, 0, sizeof(uint16_t) * (1UL << symbol_bits));
        memset(lz.distributions_table.inverse_order, 0,
               sizeof(uint16_t) * (1UL << symbol_bits));
        lz.size = 0;
        lz.idx = 0;
        lz.distributions_table.symbols_set_size = 0;
        lz.distributions_table.unique_dist_size = 0;
        memset(block.contents, 0, block.capacity);
        block.size = 0;
    }

    fclose(fp);
    fclose(fp_out);
    free(block.contents);
    free(lz.contents);
    free(lz.distributions);
    free(lz.distributions_table.inverse_order);

    return 0;
}

/* Main function */
int
main(int argc, char *argv[]) {
    int status;
    if (argc != 3 && argc != 5) {
        printf(HELP_MSG);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0) {
        uint8_t best_block_bits;
        uint8_t best_symbol_bits;
        int best_result = INT_MAX;
        int cur_result;

        for (uint8_t i = 9; i < 17; i++) {
            for (uint8_t j = 9; j < 13; j++) {
                cur_result =
                    compress(argv[2], "res/output/compressed.bin", 1, i, j);
                if (cur_result < best_result) {
                    best_result = cur_result;
                    best_block_bits = i;
                    best_symbol_bits = j;
                }
            }
        }
        status = compress(argv[2], "res/output/compressed.bin", 0,
                          best_block_bits, best_symbol_bits);
        printf("%s FINISHED\n", argv[2]);
        return status;
    } else if (strcmp(argv[1], "-d") == 0) {
        status = decompress(argv[2], "res/output/decompressed.bin");
        printf("%s FINISHED\n", argv[2]);
        return status;
    } else {
        printf(HELP_MSG);
        return 1;
    }
    return 0;
}