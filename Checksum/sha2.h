/*
 * SHA2 implementation for PuTTY. Written directly from the spec by
 * Simon Tatham.
 */

#ifndef __SHA2_H
#define __SHA2_H

#include <stdint.h>

#define SHA512_DIGEST_SIZE ( 512 / 8)
#define SHA512_BLOCK_SIZE  (1024 / 8)

#ifdef __cplusplus
extern "C" {
#endif

/*typedef struct {
	uint64_t h[8];
	unsigned char block[SHA512_BLOCK_SIZE];
	int blkused;
	uint64_t lenhi, lenlo;
	//BinarySink_IMPLEMENTATION;
} Sha512_State;

void Sha512_Init(Sha512_State *s);
void Sha512_Update(Sha512_State *s, unsigned char *buf, unsigned int len);
void Sha512_Final(Sha512_State *s, unsigned char *digest);*/

/* Structure to save state of computation between the single steps.  */
typedef struct {
	uint64_t state[8];
	uint64_t total[2];
	size_t buflen;  /* >= 0, <= 256 */
	uint64_t buffer[32]; /* 256 bytes; the first buflen bytes are in use */
} sha512_ctx;

/* Initialize structure containing state of computation. */
void sha512_init_ctx (sha512_ctx *ctx);

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is NOT required that LEN is a multiple of 128.  */
void sha512_process_bytes (const void *buffer, size_t len, sha512_ctx *ctx);

/* Process the remaining bytes in the buffer and put result from CTX
   in first 64 (48) bytes following RESBUF.  The result is always in little
   endian byte order, so that a byte-wise output yields to the wanted
   ASCII representation of the message digest.  */
void *sha512_finish_ctx (sha512_ctx *ctx, void *resbuf);

#ifdef __cplusplus
}
#endif

#endif /* __SHA2_H */
