#ifndef LZ_H
#define LZ_H

#include "../lzari_defs.h"

/**
 * @brief gathers information extracted through LZSS-encoding a file.
 * 
 * @param file the file content to compress
 * @return a struct holding all relevant information regarding the encoding.
 */
lz_t
lzss_compress(file_t *file);

#endif // LZ_H
