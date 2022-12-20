#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdfloat.h>

/*
 * Welcome to Roi Reiter's amateur LZARI compressor/decompressor!
 * This algorithm is based on Haruhiko Okumara's "History of Data Compression
 * in Japan". https://oku.edu.mie-u.ac.jp/~okumura/compression/history Okumara
 * presents lots of ideas, I took some. The algorithm implements LZSS with
 * arithmetic encoding (ANS). Feel free to look around, or try out the program
 * yourself!
 */

/* Parameter definitions */
#define MAX_FILE_SIZE (1 << 20)
#define SYMBOLS_AMT (1 << 12)
#define LITERALS_AMT (1 << 8)
#define WINDOW_SIZE ((SYMBOLS_AMT - LITERALS_AMT) * 2)
#define SYMBOL(x) (x + LITERALS_AMT)
#define BREAK_EVEN_LENGTH 2
#define MIN(x, y) ((y < x) ? y : x)
#define MAX(x, y) ((y > x) ? y : x)

uint8_t file_contents[MAX_FILE_SIZE];

/* String definitions */
#define HELP_MSG                                                               \
  "Please provide flag (-c for compress, -d for decompress), as well as a "    \
  "path to your file.\n"
#define INVALID_FILE_MSG "Problem accessing given file. Operation failed.\n"

/* Struct definitions */

typedef struct
{
  size_t size;
  uint8_t contents[MAX_FILE_SIZE];
} file_t;

typedef struct
{
    float128_t a;
    float128_t b;
} dyadic_t;

typedef struct
{
    dyadic_t distribution;
    uint16_t content;
} element_t;

typedef struct
{
  size_t size;
  uint16_t contents[2 * MAX_FILE_SIZE];
  uint16_t distributions[SYMBOLS_AMT];
  size_t symbols_set_size;
} lz_t;

typedef struct
{
  size_t encoding;
  size_t size;
  uint16_t* cumulative;
  uint16_t* symbols;
  element_t* contents;
} ari_t;

typedef struct
{
  size_t start_idx;
  size_t end_idx;
  size_t cur_idx;
} window_t;

/* Global variables */
file_t g_file = { 0 };
lz_t g_lz = { 0 };
ari_t g_ari = { 0 };

/* helper functions */

static int
parse_file(char* filename)
{
  FILE* fp;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    return 1;
  }

  g_file.size = fread(g_file.contents,
                      sizeof(uint8_t),
                      sizeof(g_file.contents) / sizeof(uint8_t),
                      fp);

  fclose(fp);
  return 0;
}

static int
lzss(void)
{
  window_t window = { .start_idx = 0,
                      .cur_idx = 0,
                      .end_idx = MIN(WINDOW_SIZE, g_file.size) };
  size_t candidate_idx;
  uint16_t best_length;
  uint16_t best_distance;
  uint16_t candidate_length;
  uint16_t candidate_distance;
  size_t candidate_match_checking_idx;
  size_t cur_match_checking_idx;

  while (window.cur_idx < window.end_idx) {
    best_length = 1;
    best_distance = 0;

    for (candidate_idx = window.start_idx; candidate_idx < window.cur_idx;
         candidate_idx++) {
      candidate_match_checking_idx = candidate_idx;
      cur_match_checking_idx = window.cur_idx;

      while ((cur_match_checking_idx < window.end_idx) &&
             (g_file.contents[cur_match_checking_idx] ==
              g_file.contents[candidate_match_checking_idx])) {
        cur_match_checking_idx++;
        candidate_match_checking_idx++;
      }

      candidate_distance = window.cur_idx - candidate_idx;
      candidate_length = candidate_match_checking_idx - candidate_idx;

      if (candidate_length > MAX(best_length, BREAK_EVEN_LENGTH)) {
        best_length = candidate_length;
        best_distance = candidate_distance;
      }
    }

    if (best_distance > 0) {
      g_lz.contents[g_lz.size++] = SYMBOL(best_distance);
      g_lz.contents[g_lz.size++] = SYMBOL(best_length);
            if (g_lz.distributions[SYMBOL(best_distance) == 0)
            {
              g_lz.symbols_set_size++;
            }
            if (g_lz.distributions[SYMBOL(best_length) == 0)
            {
              g_lz.symbols_set_size++;
            }
            g_lz.distributions[SYMBOL(best_distance)]++;
            g_lz.distributions[SYMBOL(best_length)]++;
    } else {
            g_lz.contents[g_lz.size++] = g_file.contents[window.cur_idx];
            g_lz.distributions[g_file.contents[window.cur_idx]]++;
    }

    window.cur_idx += best_length;
    if (window.cur_idx - window.start_idx > (WINDOW_SIZE / 2)) {
            window.start_idx += best_length;
            if (window.end_idx < g_file.size) {
              window.end_idx = MIN((window.end_idx + best_length), g_file.size);
            }
    }
  }
  printf("%d vs %d\n", g_lz.size, g_file.size);

  for (int i = 256; i < SYMBOLS_AMT; i++) {
    if (g_lz.distributions[i] == 0) {
            printf("%d ", i);
    }
  }
  return 0;
}

/**
 * @brief Arithmetically codes the LZSS-compressed content and outputs it to a
 * file with the given file name.
 *
 * @param filename The name to save the output file.
 * @return int int 0 on success, 1 otherwise.
 */
static int
lszz_output(char* filename)
{
  size_t cumulative_value;
  FILE* fp;

  g_ari.cumulative = calloc(sizeof(uint16_t) * (g_lz.symbols_set_size + 1));
  g_ari.symbols = malloc(sizeof(uint16_t) * g_lz.symbols_set_size);

  cumulative_value = 0;
  for (size_t i = 0, j = 0; i < g_lz.size; i++) {
    if (g_lz.distributions[i] != 0) {
            cumulative_value += g_lz.distributions[i];
            g_ari.cumulative[j + 1] = cumulative_value;
            g_ari.symbols[j] = i;
            j++;
    }
  }

  fp = fopen(filename, "wb");
  if (fp == NULL) {
    return 1;
  }

  output_table(fp);

  for (int i = 0; i < g_lz.size; i++) {
    output_dyadic(fp, g_lz.contents[i]);
  }

  free(g_dyadic.cumulative);
  free(g_dyadic.symbols);
}

/* Compression & Decompression functions */
static int
compress(char* filename)
{
  int status;

  status = parse_file(filename);
  if (status != 0) {
    printf(INVALID_FILE_MSG);
    return status;
  }

  printf("Parsed file. Beginning lzss.\n");

  lzss();

  return 0;
}

static int
decompress(char* filename)
{
  return 0;
}

/* Main function */
int
main(int argc, char* argv[])
{
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