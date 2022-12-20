#include "ari/ari.h"
#include "file_utils/file_utils.h"
#include "lz/lz.h"
#include "lzari_defs.h"
#include <stdint.h>
#include <stdio.h>
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
compress(char *filename) {
    int status;
    block_t block;
    lz_t lz;
    ari_t ari;
    FILE *fp;
    size_t file_size;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf(INVALID_FILE_MSG);
        return 1;
    }

    while (1) {
        block.size = fread(block.contents, sizeof(uint8_t),
                           sizeof(block.contents) / sizeof(uint8_t), fp);
        if (block.size == 0) {
            break;
        }

        
    }

    printf(DONE_MSG)

    status = parse_file(&file, filename);
    if (status != 0) {
        return status;
    }

    printf("Parsed file. Beginning LZSS compression.\n");

    lz = lzss_compress(&file);

    printf("Compressed. Beginning arithmetic encoding.\n");
    ari = ari_encode(&lz);

    printf("Encoded. Outputting file.\n");
    status = output_ari(&ari);
    if (status != 0) {
        printf(WRITE_ERROR_MSG);
        return status;
    }

    printf("Process completed. The file should be in your workarea.\n");
    printf("Thank you for choosing Roi's LZARI! Goodbye!\n");
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
        return compress(argv[2]);
    } else if (strcmp(argv[1], "-d") == 0) {
        return decompress(argv[2]);
    } else {
        printf(HELP_MSG);
        return 1;
    }
    return 0;
}