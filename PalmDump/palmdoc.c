/*
The format is that of a standard Palm Database Format file. The header of that format includes the name of the database
(usually the book title and sometimes a portion of the authors name) which is up to 31 bytes of data.
This string of characters is terminated with a 0 in the C style. The files are identified as a Type of TEXt and a Creator ID of REAd.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "palmdoc.h"
#include "lz77.h"

PalmDOCHeader *readPalmDOCHeader(PDBRecordEntryData *rec)
{
	PalmDOCHeader *pdh = malloc(sizeof(PalmDOCHeader));

	assert(rec && rec->data);
	assert(rec->length == 14 || rec->length == sizeof(PalmDOCHeader));

	memcpy(pdh, rec->data, min(rec->length, sizeof(PalmDOCHeader)));

	pdh->compression = _byteswap_ushort(pdh->compression);
	pdh->unused = _byteswap_ushort(pdh->unused);
	pdh->uncompressed_length = _byteswap_ulong(pdh->uncompressed_length);
	pdh->record_count = _byteswap_ushort(pdh->record_count);
	pdh->record_size = _byteswap_ushort(pdh->record_size);
	pdh->current_pos = _byteswap_ulong(pdh->current_pos);

	if (rec->length == 14)
	{
		pdh->current_pos = ((pdh->current_pos & 0xFFFF0000) >> 16);
	}

	return pdh;
}

void writePalmDOCText(FILE *f, PalmDOCHeader *pdh, PDBRecordEntryData **arec)
{
	unsigned char *text = NULL;
	unsigned long  text_len = 0;
	unsigned long  uncompressed_len = 0;

	assert(pdh);
	assert(arec);

	switch (pdh->compression)
	{
	case kPalmDocCompressionUsed:
		for (unsigned short i = 1; i < pdh->record_count + 1; i++) {
			if (arec[i] != NULL) {
				text_len = pdh->record_size;
				text = decodeLz77(arec[i]->data, arec[i]->length, &text_len);
				assert(text_len <= (unsigned long)pdh->record_size);
				fwrite(text, text_len, 1, f);
				uncompressed_len += text_len;
				free(text);
			}
		}
		assert(uncompressed_len == pdh->uncompressed_length);
		break;
	case kPalmDocCompressionNone:
		for (unsigned short i = 1; i < pdh->record_count + 1; i++) {
			if (arec[i] != NULL) {
				fwrite(arec[i]->data, arec[i]->length, 1, f);
			}
		}
		break;
	}
}

void disposePalmDOCData(PalmDOCHeader **pdh)
{
	if (pdh && *pdh)
		free(*pdh);

	*pdh = NULL;
}
