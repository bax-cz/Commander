#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "huffcdic.h"

/*
	Decompress MOBI files compressed with the HUFF/CDIC algorithm.
*/

void dict1Unpack(PalmHuffDict *dict, unsigned long num)
{
	dict->codelen = num & 0x1f;
	dict->term = num & 0x80;
	dict->maxcode = num >> 8;

	assert(dict->codelen != 0);

	if (dict->codelen <= 8) {
		assert(dict->term);
	}

	dict->maxcode = ((dict->maxcode + 1) << (32 - dict->codelen)) - 1;
}

PalmHuffDictCtx *loadHuff(const PDBRecordEntryData *huff_rec)
{
	unsigned char *p = huff_rec->data;

	if( memcmp( p, "HUFF\x00\x00\x00\x18", 8 ) )
		return NULL; // Invalid HUFF header

	PalmHuffDictCtx *ctx = malloc(sizeof(PalmHuffDictCtx));
	memset(ctx, 0, sizeof(PalmHuffDictCtx));

	initBuff(&ctx->buff);

	p += 8;

	unsigned long off1, off2;
	off1 = _byteswap_ulong(*(unsigned long*)p);
	p += sizeof(unsigned long);

	off2 = _byteswap_ulong(*(unsigned long*)p);
	p += sizeof(unsigned long);

	unsigned long *pval = (unsigned long*)(huff_rec->data + off1);
	for (size_t i = 0; i < kPalmHuffDict1Len; i++) {
		dict1Unpack(&ctx->dict1[i], _byteswap_ulong(*(pval + i)));
	}

	// load dict2 min and max codes
	unsigned long long mincode = 0ull;
	unsigned long long maxcode = 0ull;

	ctx->mincodes[0] = (mincode << 32);
	ctx->maxcodes[0] = (((maxcode + 1) << 32) - 1);

	pval = (unsigned long*)(huff_rec->data + off2);

	for (size_t i = 1, j = 1; i < kPalmHuffCodesLen; i++, j += 2) {
		mincode = _byteswap_ulong(*(pval + j - 1));
		maxcode = _byteswap_ulong(*(pval + j));
		ctx->mincodes[i] = (mincode << (32 - i));
		ctx->maxcodes[i] = (((maxcode + 1) << (32 - i)) - 1);
	}

	return ctx;
}

void loadCdic(PalmHuffDictCtx *ctx, const PDBRecordEntryData *cdic_rec)
{
	unsigned char *p = cdic_rec->data;

	if (memcmp(p, "CDIC\x00\x00\x00\x10", 8))
		return /*NULL*/; // Invalid CDIC header

	p += 8;

	unsigned long phrases, bits;
	phrases = _byteswap_ulong(*(unsigned long*)p);
	p += sizeof(unsigned long);

	bits = _byteswap_ulong(*(unsigned long*)p);
	p += sizeof(unsigned long);

	if (ctx->dictionary == NULL)
		ctx->dictionary = malloc(phrases * sizeof(PalmHuffDictData*));

	unsigned long n = min((unsigned long)(1 << bits), phrases - ctx->dictionary_len);
	unsigned short off, blen, *ps = (unsigned short*)p;

	for (unsigned long i = 0; i < n; i++, ps++) {
		off = _byteswap_ushort(*ps);
		blen = _byteswap_ushort(*(unsigned short*)(p + off));
		ctx->dictionary[ctx->dictionary_len + i] = malloc(sizeof(PalmHuffDictData));
		ctx->dictionary[ctx->dictionary_len + i]->rec.data = malloc((blen & 0x7fff));
		ctx->dictionary[ctx->dictionary_len + i]->rec.length = (blen & 0x7fff);
		ctx->dictionary[ctx->dictionary_len + i]->flag = (blen & 0x8000);
		memcpy(ctx->dictionary[ctx->dictionary_len + i]->rec.data, p + off + 2, (blen & 0x7fff));
	}

	ctx->dictionary_len += n;
}

unsigned char *decodeHuff(PalmHuffDictCtx *ctx, const PDBRecordEntryData *rec, unsigned int *uncompressed_len)
{
	unsigned long codelen, term, maxcode;
	unsigned long slice_len, r, code, off = 0;
	unsigned char *slice = NULL;

	unsigned long long x = _byteswap_uint64(*(unsigned long long*)rec->data);
	int bitsleft = rec->length * 8, n = 32;

	for (;;) {
		if (n <= 0) {
			off += 4;
			x = _byteswap_uint64(*(unsigned long long*)(rec->data + off));
			n += 32;
		}

		code = (unsigned long)((x >> n) & ((1ull << 32) - 1));

		codelen = ctx->dict1[code >> 24].codelen;
		term    = ctx->dict1[code >> 24].term;
		maxcode = ctx->dict1[code >> 24].maxcode;

		if (term == 0) {
			while (code < ctx->mincodes[codelen])
				codelen += 1;
			maxcode = (unsigned long)ctx->maxcodes[codelen];
		}

		n -= codelen;
		bitsleft -= codelen;

		if (bitsleft < 0)
			break;

		r = (maxcode - code) >> (32 - codelen);
		slice = ctx->dictionary[r]->rec.data;
		slice_len = ctx->dictionary[r]->rec.length;

		if (ctx->dictionary[r]->flag == 0) {
			unsigned long pos = ctx->buff.data_len;

			// recursive call
			decodeHuff(ctx, &ctx->dictionary[r]->rec, uncompressed_len);

			// update dictionary record
			if (ctx->dictionary[r]->rec.length < ctx->buff.data_len - pos) {
				unsigned char *tmp = realloc(ctx->dictionary[r]->rec.data, ctx->buff.data_len - pos);
				if (tmp)
					ctx->dictionary[r]->rec.data = tmp;
				else {
					assert("realloc failed" && 1 != 1);
					break;
				}
			}
			memcpy(ctx->dictionary[r]->rec.data, ctx->buff.data + pos, ctx->buff.data_len - pos);
			ctx->dictionary[r]->rec.length = ctx->buff.data_len - pos;
			ctx->dictionary[r]->flag = 1;
		}
		else
			appendBuff(&ctx->buff, slice, slice_len);
	}

	*uncompressed_len = ctx->buff.data_len;

	return ctx->buff.data;
}

void disposeHuffCtx(PalmHuffDictCtx *ctx)
{
	if (ctx->dictionary) {
		for (unsigned long i = 0; i < ctx->dictionary_len; i++) {
			free(ctx->dictionary[i]->rec.data);
			free(ctx->dictionary[i]);
		}
		free(ctx->dictionary);
	}
	deinitBuff(&ctx->buff);
	free(ctx);
}
