#include <stdint.h>
#include "file_utils.h"

int
parse_file(file_t *file, char* filename)
{
  FILE* fp;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    return 1;
  }

  file->size = fread(file->contents,
                      sizeof(uint8_t),
                      sizeof(file->contents) / sizeof(uint8_t),
                      fp);

  fclose(fp);
  return 0;
}

int
output_ari(ari_t *ari)
{
    free(ari->cumulative);
    free(ari->symbols);

    return 0;
}