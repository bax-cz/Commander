#pragma once

#include "pdb.h"

/* PalmDOC compression types */
#define kPalmDocCompressionNone      1  /* No compression */
#define kPalmDocCompressionUsed      2  /* PalmDOC compression */

/* Type of TEXt and a Creator ID of REAd */
#define kPalmDocType    1413830772ul /* TEXt */
#define kPalmDocCreator 1380270436ul /* REAd */

/*
The first record in the Palm Database Format gives more information about the PalmDOC file, and contains 16 bytes

bytes	content	comments
2	Compression	1 == no compression, 2 = PalmDOC compression
2	Unused	Always zero
4	text length	Uncompressed length of the entire text of the book
2	record count	Number of PDB records used for the text of the book.
2	record size	Maximum size of each record containing text, always 4096
4	Current Position	Current reading position, as an offset into the uncompressed text
*/

typedef struct PalmDOCHeader {
	unsigned short compression;
	unsigned short unused;
	unsigned long  uncompressed_length;
	unsigned short record_count;
	unsigned short record_size;
	unsigned long  current_pos;
} PalmDOCHeader;

extern PalmDOCHeader *readPalmDOCHeader(PDBRecordEntryData *rec);
extern void writePalmDOCText(FILE *f, PalmDOCHeader *pdh, PDBRecordEntryData **arec);
extern void disposePalmDOCData(PalmDOCHeader **pdh);
