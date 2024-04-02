#pragma once

#include "pdb.h"
#include "growbuff.h"

#define kPalmHuffDict1Len   256
#define kPalmHuffCodesLen    33

typedef struct PalmHuffDict {
	unsigned long codelen;
	unsigned long term;
	unsigned long maxcode;
} PalmHuffDict;

typedef struct PalmHuffDictData {
	PDBRecordEntryData rec;
	unsigned short     flag;
} PalmHuffDictData;

typedef struct PalmHuffDictCtx {
	PalmHuffDictData **dictionary;
	unsigned long      dictionary_len;
	PalmHuffDict       dict1[kPalmHuffDict1Len];
	unsigned long long mincodes[kPalmHuffCodesLen];
	unsigned long long maxcodes[kPalmHuffCodesLen];
	GrowBuff           buff;
} PalmHuffDictCtx;

PalmHuffDictCtx *loadHuff(const PDBRecordEntryData *huff_rec);
void loadCdic(PalmHuffDictCtx *ctx, const PDBRecordEntryData *cdic_rec);
unsigned char *decodeHuff(PalmHuffDictCtx *ctx, const PDBRecordEntryData *rec, unsigned int *uncompressed_len);
void disposeHuffCtx(PalmHuffDictCtx *ctx);
