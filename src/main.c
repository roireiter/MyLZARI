#include "ari/ari.h"
#include "lz/lz.h"
#include "lzari_defs.h"
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
compress(char *filename, char *output_filename) {
    int status;
    block_t block = {0};
    lz_t lz = {0};
    FILE *fp;
    FILE *fp_out;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf(INVALID_FILE_MSG);
        return 1;
    }

    fp_out = fopen(output_filename, "wb");

    while (1) {
        block.size = fread(block.contents, sizeof(uint8_t),
                           sizeof(block.contents) / sizeof(uint8_t), fp);
        if (block.size == 0) {
            break;
        }

        lzss_compress(&block, &lz);
        memset(&block, 0, sizeof(block));
        ari_init(&lz, &block);
        while (lz.encoding_idx < lz.size) {
            ari_encode(&lz, &block);
            fwrite(block.contents, sizeof(uint8_t),
                   block.size, fp_out);
            fseek(fp_out, -1, SEEK_CUR);
        }
        free(lz.distributions_table.order);
        free(lz.distributions_table.unique);
        free(lz.distributions_table.amt);
        memset(&lz, 0, sizeof(lz));
    }

    fclose(fp);
    fclose(fp_out);

    printf(DONE_MSG);
    printf(THANK_YOU_MSG);

    return 0;
}

static int
decompress(char *filename) {
    return 0;
}

/* Main function */
int
main(int argc, char *argv[]) {
    if (argc != 3) {
        printf(HELP_MSG);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0) {
        return compress(argv[2], "res/output/output.bin");
    } else if (strcmp(argv[1], "-d") == 0) {
        return decompress(argv[2]);
    } else {
        printf(HELP_MSG);
        return 1;
    }
    return 0;
}