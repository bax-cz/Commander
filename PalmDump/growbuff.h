#pragma once

#define kAllocSize  4096

typedef struct GrowBuff {
	unsigned char *data;
	unsigned long  data_len;
	unsigned long  alloc_size;
} GrowBuff;

void initBuff(GrowBuff *buff);
void appendBuff(GrowBuff *buff, const unsigned char *data, unsigned long len);
void deinitBuff(GrowBuff *buff);
