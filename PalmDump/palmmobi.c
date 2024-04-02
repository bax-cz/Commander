/*
The format is that of a standard Palm Database Format file. The header of that format includes the name of the database
(usually the book title and sometimes a portion of the authors name) which is up to 31 bytes of data.
This string of characters is terminated with a 0 in the C style. The files are identified as a Type of BOOK and a Creator ID of MOBI.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <direct.h>

#include "palmdoc.h"
#include "palmmobi.h"
#include "lz77.h"
#include "huffcdic.h"

#define lenof(x) ( (sizeof((x))) / (sizeof(*(x))))

PalmMOBIHeader *readPalmMOBIHeader(const PDBRecordEntryData *rec)
{
	assert(rec);
	assert(rec->length >= sizeof(PalmDOCHeader));

	PalmMOBIHeader *mh = malloc(sizeof(PalmMOBIHeader));
	unsigned char *p = rec->data;

	mh->pdh = malloc(sizeof(PalmDOCHeader));
	memcpy(mh->pdh, p, sizeof(PalmDOCHeader));

	/* PalmDOC "current_pos" entry is reused as two shorts for encryption info. */
	mh->pdh->compression = _byteswap_ushort(mh->pdh->compression);
	mh->pdh->unused = _byteswap_ushort(mh->pdh->unused);
	mh->pdh->uncompressed_length = _byteswap_ulong(mh->pdh->uncompressed_length);
	mh->pdh->record_count = _byteswap_ushort(mh->pdh->record_count);
	mh->pdh->record_size = _byteswap_ushort(mh->pdh->record_size);
	mh->pdh->current_pos = _byteswap_ulong(mh->pdh->current_pos);
	mh->pdh->current_pos = ((mh->pdh->current_pos & 0xFFFF0000) >> 16); // Encryption type  0 == no encryption, 1 = Old Mobipocket Encryption, 2 = Mobipocket Encryption

	p += sizeof(PalmDOCHeader);

	assert(rec->length >= sizeof(PalmDOCHeader) + kPalmMobiHeaderFixedLen);

	mh->pmh = malloc(sizeof(PalmMobiPocketHeader));
	memcpy(mh->pmh, p, kPalmMobiHeaderFixedLen); // read fixed part of the Mobi header
	memset((char*)mh->pmh + kPalmMobiHeaderFixedLen, 0, sizeof(PalmMobiPocketHeader) - kPalmMobiHeaderFixedLen);

	mh->pmh->identifier = _byteswap_ulong(mh->pmh->identifier);
	mh->pmh->header_length = _byteswap_ulong(mh->pmh->header_length);
	mh->pmh->mobi_type = _byteswap_ulong(mh->pmh->mobi_type);
	mh->pmh->text_encoding = _byteswap_ulong(mh->pmh->text_encoding);
	mh->pmh->unique_id = _byteswap_ulong(mh->pmh->unique_id);
	mh->pmh->generator_version = _byteswap_ulong(mh->pmh->generator_version);
	mh->pmh->reserved1 = _byteswap_ulong(mh->pmh->reserved1);
	mh->pmh->first_non_book_index = _byteswap_ulong(mh->pmh->first_non_book_index);
	mh->pmh->full_name_offset = _byteswap_ulong(mh->pmh->full_name_offset);
	mh->pmh->full_name_length = _byteswap_ulong(mh->pmh->full_name_length);
	mh->pmh->language = _byteswap_ulong(mh->pmh->language);
	mh->pmh->input_language = _byteswap_ulong(mh->pmh->input_language);
	mh->pmh->output_language = _byteswap_ulong(mh->pmh->output_language);
	mh->pmh->format_version = _byteswap_ulong(mh->pmh->format_version);
	mh->pmh->first_image_record = _byteswap_ulong(mh->pmh->first_image_record);
	mh->pmh->huff_record = _byteswap_ulong(mh->pmh->huff_record);
	mh->pmh->huff_count = _byteswap_ulong(mh->pmh->huff_count);
	mh->pmh->datp_record = _byteswap_ulong(mh->pmh->datp_record);
	mh->pmh->datp_count = _byteswap_ulong(mh->pmh->datp_count);
	mh->pmh->exth_flags = _byteswap_ulong(mh->pmh->exth_flags);

	assert(mh->pmh->identifier == kPalmMobiCreator);
	assert(rec->length >= sizeof(PalmDOCHeader) + mh->pmh->header_length);

	if (mh->pmh->header_length > kPalmMobiHeaderFixedLen) {
		// read the rest of Mobi header
		memcpy((char*)mh->pmh + kPalmMobiHeaderFixedLen, p + kPalmMobiHeaderFixedLen,
			min(mh->pmh->header_length, sizeof(PalmMobiPocketHeader)) - kPalmMobiHeaderFixedLen);
		mh->pmh->unknown2 = _byteswap_ulong(mh->pmh->unknown2);
		mh->pmh->drm_offset = _byteswap_ulong(mh->pmh->drm_offset);
		mh->pmh->drm_count = _byteswap_ulong(mh->pmh->drm_count);
		mh->pmh->drm_size = _byteswap_ulong(mh->pmh->drm_size);
		mh->pmh->drm_flags = _byteswap_ulong(mh->pmh->drm_flags);
		mh->pmh->last_image_record = _byteswap_ushort(mh->pmh->last_image_record);
		mh->pmh->unknown4 = _byteswap_ulong(mh->pmh->unknown4);
		mh->pmh->fcis_record = _byteswap_ulong(mh->pmh->fcis_record);
		mh->pmh->unknown5 = _byteswap_ulong(mh->pmh->unknown5);
		mh->pmh->flis_record = _byteswap_ulong(mh->pmh->flis_record);
		mh->pmh->extra_data_flags = _byteswap_ushort(
			(mh->pmh->header_length < 228) ? mh->pmh->extra_data_flags : (*(unsigned short*)(p + 226)));
	}

	mh->pxh = malloc(sizeof(PalmMobiExthHeader));

	// if bit 6 (0x40) is set, then there's an EXTH record
	if (mh->pmh->exth_flags & 0x00000040)
	{
		p += mh->pmh->header_length;

		memcpy(mh->pxh, p, 3 * sizeof(unsigned long));
		mh->pxh->identifier = _byteswap_ulong(mh->pxh->identifier);
		mh->pxh->header_length = _byteswap_ulong(mh->pxh->header_length);
		mh->pxh->record_count = _byteswap_ulong(mh->pxh->record_count);

		p += 3 * sizeof(unsigned long);

		assert(mh->pxh->identifier == kPalmMobiExth);

		mh->pxh->record_type = malloc(mh->pxh->record_count * sizeof(unsigned long));
		mh->pxh->record_length = malloc(mh->pxh->record_count * sizeof(unsigned long));
		mh->pxh->record_data = malloc(mh->pxh->record_count * sizeof(unsigned char*));

		for (unsigned long i = 0; i < mh->pxh->record_count; i++) {
			mh->pxh->record_type[i] = _byteswap_ulong(*(unsigned long*)p);
			p += sizeof(unsigned long);
			mh->pxh->record_length[i] = _byteswap_ulong(*(unsigned long*)p) - 8;
			p += sizeof(unsigned long);
			mh->pxh->record_data[i] = malloc(mh->pxh->record_length[i]);
			memcpy(mh->pxh->record_data[i], p, mh->pxh->record_length[i]);
			p += mh->pxh->record_length[i];
		}
	}

	return mh;
}

int getTrailingEntrySize(const PDBRecordEntryData *rec, unsigned long offset)
{
	assert(rec->length >= offset);

	unsigned long bitpos = 0, result = 0;
	unsigned char v;

	for (;;) {
		v = rec->data[offset - 1];
		result |= (v & 0x7F) << bitpos;
		bitpos += 7;
		offset--;

		if ((v & 0x80) != 0 || (bitpos >= 28) || (offset == 0))
			return result;
	}
}

int getTrailingEntriesSize(const PDBRecordEntryData *rec, unsigned short extra_data_flags)
{
	unsigned long  num = 0;
	unsigned short flags = extra_data_flags >> 1;

	while (flags) {
		if (flags & 1)
			num += getTrailingEntrySize(rec, rec->length - num);
		flags >>= 1;
	}

	if (extra_data_flags & 1) {
		unsigned long offset = rec->length - num - 1;
		num += (rec->data[offset] & 0x3) + 1;
	}

	return num;
}

void decodeMobiHuff(FILE *f, const PalmMOBIHeader *pmh, const PDBRecordEntryData **arec)
{
	assert(pmh->pmh->huff_record != 0);

	const PDBRecordEntryData *huff_rec = arec[pmh->pmh->huff_record];
	PalmHuffDictCtx *ctx = loadHuff(huff_rec);

	for (unsigned long i = 1; i < pmh->pmh->huff_count; i++) {
		loadCdic(ctx, arec[pmh->pmh->huff_record + i]);
	}

	unsigned char *text = NULL;
	unsigned long  text_len = 0;

	// decode Huff/Cdic data
	for (unsigned short i = 1; i < pmh->pdh->record_count + 1; i++) {
		text = decodeHuff(ctx, arec[i], &text_len);
	}

	fwrite(text, text_len, 1, f);

	disposeHuffCtx(ctx);
}

void decodeMobiLz77(FILE *f, const PalmDOCHeader *pdh, const PDBRecordEntryData **arec)
{
	unsigned char *text = NULL;
	unsigned long  text_len = 0;
	unsigned long  uncompressed_len = 0;

	for (unsigned short i = 1; i < pdh->record_count + 1; i++) {
		if (arec[i] != NULL) {
			text_len = max((unsigned long)pdh->record_size * 2, arec[i]->length * 8); // TODO: test max length, record_size * 2 should be enough
			text = decodeLz77(arec[i]->data, arec[i]->length, &text_len);
			assert(text_len <= (unsigned long)pdh->record_size * 2);
			fwrite(text, text_len, 1, f);
			uncompressed_len += text_len;
			free(text);
		}
	}
}

int isRecordImage(const PDBRecordEntryData *rec, wchar_t *ftype)
{
	char *non_img[] = { "FLIS", "FCIS", "SRCS", "\xE9\x8E\r\n", "RESC", "BOUN", "FDST", "DATP", "AUDI", "VIDE" };

	for (int i = 0; i < lenof(non_img); i++) {
		if (!memcmp(rec->data, non_img[i], 4))
			return 0;
	}

	struct ImageFormat {
		wchar_t *ext;
		char     *id;
	};

	struct ImageFormat img_fmt[4] = {
		{ L"jpg", "\xFF\xD8" },
		{ L"gif", "GIF" },
		{ L"png", "\x89PNG" },
		{ L"bmp", "BM6" }
	};

	for (int i = 0; i < lenof(img_fmt); i++) {
		if (!memcmp(rec->data, img_fmt[i].id, strlen(img_fmt[i].id))) {
			wcscpy(ftype, img_fmt[i].ext);
			return 1;
		}
	}

	return 0;
}

void writePalmMOBIImages(const PalmMOBIHeader *pmh, const PDBRecordEntryData **arec, const wchar_t *outdir, unsigned short num_records)
{
	if (pmh->pmh->first_image_record != 0xFFFFFFFF) {
		if (pmh->pmh->last_image_record == 0 ||
			pmh->pmh->last_image_record == 0xFFFF)
			pmh->pmh->last_image_record = num_records - 1;

		wchar_t imgdir[_MAX_PATH], fname[_MAX_FNAME], ftype[_MAX_EXT];

		// ensure that image directory exists by calling mkdir
		swprintf(imgdir, _MAX_PATH, L"%ls\\img\\", outdir);
		_wmkdir(imgdir);

		FILE *f = NULL;
		for (unsigned long i = pmh->pmh->first_image_record; i < pmh->pmh->last_image_record; i++) {
			if (isRecordImage(arec[i], ftype)) {
				swprintf(fname, _MAX_FNAME, L"%ls%05lu.%ls", imgdir, i - pmh->pmh->first_image_record + 1, ftype);

				f = _wfopen(fname, L"wb");

				if (f) {
					fwrite(arec[i]->data, arec[i]->length, 1, f);
					fclose(f);
				}
			}
		}
	}
}

void writePalmMOBIHtml(FILE *f, const PalmMOBIHeader *pmh, PDBRecordEntryData **arec)
{
	assert(pmh);
	assert(arec);

	unsigned long trail = 0;

	// update entries' record lengths
	for (unsigned short i = 1; i < pmh->pdh->record_count + 1; i++) {
		trail = getTrailingEntriesSize(arec[i], pmh->pmh->extra_data_flags);
		arec[i]->length -= trail; // trim entry record length
	}

	switch (pmh->pdh->compression)
	{
	case kPalmDocCompressionHuff:
		decodeMobiHuff(f, pmh, arec);
		break;
	case kPalmDocCompressionUsed:
		decodeMobiLz77(f, pmh->pdh, arec);
		break;
	case kPalmDocCompressionNone:
		for (unsigned short i = 1; i < pmh->pdh->record_count + 1; i++) {
			if (arec[i] != NULL) {
				fwrite(arec[i]->data, arec[i]->length, 1, f);
			}
		}
		break;
	default:
		assert("Invalid compression" && 1 != 1);
		break;
	}
}

void disposePalmMOBIData(PalmMOBIHeader **pmh)
{
	if (pmh && *pmh)
	{
		// if bit 6 (0x40) is set, then there's an EXTH record
		if ((*pmh)->pmh->exth_flags & 0x00000040)
		{
			free((*pmh)->pxh->record_type);
			free((*pmh)->pxh->record_length);

			for (unsigned long i = 0; i < (*pmh)->pxh->record_count; i++)
				free((*pmh)->pxh->record_data[i]);

			free((*pmh)->pxh->record_data);
		}

		free((*pmh)->pmh);
		free((*pmh)->pxh);
		free((*pmh)->pdh);
		free(*pmh);
	}

	*pmh = NULL;
}
