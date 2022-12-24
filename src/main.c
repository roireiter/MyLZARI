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
compress(char *filename, char *output_filename, int dry_run,
         uint8_t block_bits) {
    block_t block = {0};
    lz_t lz = {0};
    FILE *fp;
    FILE *fp_out;
    int dry_run_result = 0;
    size_t written_bytes_amt;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf(INVALID_FILE_MSG);
        return 1;
    }

    fp_out = fopen(output_filename, "wb");

    block.capacity = 1UL << block_bits;
    block.contents = malloc(sizeof(uint8_t) * (1UL << block_bits));
    lz.contents = malloc(sizeof(uint16_t) * (1UL << (block_bits + 1)));
    if (!dry_run) {
        written_bytes_amt = fwrite(&block_bits, sizeof(uint8_t), 1, fp_out);
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
        memset(lz.distributions, 0, sizeof(lz.distributions));
        memset(&lz.distributions_table, 0, sizeof(lz.distributions_table));
        lz.size = 0;
        lz.idx = 0;
        memset(block.contents, 0, block.capacity);
        block.size = 0;
    }

    fclose(fp);
    fclose(fp_out);
    free(block.contents);
    free(lz.contents);

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
    uint8_t block_bits;
    size_t written_bytes_amt;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf(INVALID_FILE_MSG);
        return 1;
    }

    fp_out = fopen(output_filename, "wb");

    fread(&block_bits, sizeof(uint8_t), 1, fp);
    block.capacity = 1UL << block_bits;
    block.contents = malloc(sizeof(uint8_t) * (1UL << block_bits));
    lz.contents = malloc(sizeof(uint16_t) * (1UL << (block_bits + 1)));

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
        memset(lz.distributions, 0, sizeof(lz.distributions));
        memset(&lz.distributions_table, 0, sizeof(lz.distributions_table));
        lz.size = 0;
        lz.idx = 0;
        memset(block.contents, 0, block.capacity);
        block.size = 0;
    }

    fclose(fp);
    fclose(fp_out);
    free(block.contents);
    free(lz.contents);

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
        int best_result = INT_MAX;
        int cur_result;

        for (uint8_t i = 9; i < 17; i++) {
            cur_result = compress(argv[2], "res/output/compressed.bin", 1, i);
            if (cur_result < best_result) {
                best_result = cur_result;
                best_block_bits = i;
            }
        }
        status =
            compress(argv[2], "res/output/compressed.bin", 0, best_block_bits);
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