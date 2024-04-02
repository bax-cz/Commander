#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>

#define PATH_SEPARATOR "\\"

#define TRUE 1
#define FALSE 0

#define FLIPENDIAN(x) flipEndian((unsigned char *)(&(x)), sizeof(x))
#define FLIPENDIANLE(x) flipEndianLE((unsigned char *)(&(x)), sizeof(x))

#define TIME_OFFSET_FROM_UNIX 2082844800L
#define APPLE_TO_UNIX_TIME(x) ((x)-TIME_OFFSET_FROM_UNIX)
#define UNIX_TO_APPLE_TIME(x) ((x) + TIME_OFFSET_FROM_UNIX)

#define ASSERT(x, m)                                                           \
  if (!(x)) {                                                                  \
    fflush(stdout);                                                            \
    fprintf(stderr, "error: %s\n", m);                                         \
    perror("error");                                                           \
    fflush(stderr);                                                            \
    exit(1);                                                                   \
  }

static inline void flipEndian(unsigned char *x, int length) {
  int i;
  unsigned char tmp;
  /* flip when big endian */
  for (i = 0; i < (length / 2); i++) {
    tmp = x[i];
    x[i] = x[length - i - 1];
    x[length - i - 1] = tmp;
  }
}

static inline void flipEndianLE(unsigned char *x, int length) {
  /* nothing to do on windows */
  return;
}

static inline void hexToBytes(const char *hex, uint8_t **buffer,
                              size_t *bytes) {
  *bytes = strlen(hex) / 2;
  *buffer = (uint8_t *)malloc(*bytes);
  size_t i;
  for (i = 0; i < *bytes; i++) {
    uint32_t byte;
    sscanf(hex, "%2x", &byte);
    (*buffer)[i] = byte;
    hex += 2;
  }
}

static inline void hexToInts(const char *hex, unsigned int **buffer,
                             size_t *bytes) {
  *bytes = strlen(hex) / 2;
  *buffer = (unsigned int *)malloc((*bytes) * sizeof(int));
  size_t i;
  for (i = 0; i < *bytes; i++) {
    sscanf(hex, "%2x", &((*buffer)[i]));
    hex += 2;
  }
}

struct io_func_struct;

typedef int (*readFunc)(struct io_func_struct *io, size_t location, size_t size,
                        void *buffer);
typedef int (*writeFunc)(struct io_func_struct *io, size_t location, size_t size,
                         void *buffer);
typedef void (*closeFunc)(struct io_func_struct *io);

typedef struct io_func_struct {
  void *data;
  readFunc read;
  writeFunc write;
  closeFunc close;
} io_func;

struct AbstractFile;

unsigned char *decodeBase64(char *toDecode, size_t *dataLength);
void writeBase64(struct AbstractFile *file, unsigned char *data,
                 size_t dataLength, int tabLength, int width);
char *convertBase64(unsigned char *data, size_t dataLength, int tabLength,
                    int width);

#endif
