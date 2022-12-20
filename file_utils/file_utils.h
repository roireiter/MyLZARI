#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "../lzari_defs.h"
#include <stdio.h>

/**
 * @brief parses a file into a file_t
 *
 * @param file the variable which will hold the data parsed from an actual file
 * @param filename the path to an actual file
 * @return 0 if success, 1 if failure
 */
int parse_file(file_t *file, char *filename);

/**
 * @brief outputs a file containing arithmetically encoded data.
 *
 * @param ari struct containing arithmetic encoding.
 * @return 0 on success, 1 on failure.
 */
int output_ari(ari_t *ari);

#endif   // FILE_UTILS_H
