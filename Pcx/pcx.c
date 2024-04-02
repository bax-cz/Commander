#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pcx.h"

/* Decode a PCX scan line */
int readScanLineRle(PCXContext *ctx, unsigned char **pbuf, unsigned char *line, int lineLen, int *total);

/* Calculate image size */
unsigned int calcImageSize(PCXContext *ctx);

PCXContext *PCXOpen(const wchar_t *fname)
{
	/* Open pcx file */
	FILE *f = _wfopen(fname, L"rb");

	PCXContext *ctx = NULL;

	if (f)
	{
		ctx = (PCXContext*)malloc(sizeof(PCXContext));
		memset(ctx, 0, sizeof(PCXContext));

		/* Read pcx header */
		size_t bytesRead = fread(&ctx->pcxHdr, sizeof(char), sizeof(ctx->pcxHdr), f);

		/* Get data file length */
		long offset = ftell(f);
		fseek(f, 0, SEEK_END);
		ctx->bufferLen = ftell(f) - offset;
		fseek(f, offset, SEEK_SET);

		/* Validate header data */
		if (bytesRead == sizeof(ctx->pcxHdr) && ctx->bufferLen != 0 &&
			ctx->pcxHdr.Identifier == 0x0A &&
			ctx->pcxHdr.BytesPerLine != 0 && (
			ctx->pcxHdr.Encoding == 0 ||
			ctx->pcxHdr.Encoding == 1) && (
			ctx->pcxHdr.BitsPerPixel == 1 ||
			ctx->pcxHdr.BitsPerPixel == 2 ||
			ctx->pcxHdr.BitsPerPixel == 4 ||
			ctx->pcxHdr.BitsPerPixel == 8) && (
			ctx->pcxHdr.NumBitPlanes == 1 ||
			ctx->pcxHdr.NumBitPlanes == 2 ||
			ctx->pcxHdr.NumBitPlanes == 3 ||
			ctx->pcxHdr.NumBitPlanes == 4) && (
			ctx->pcxHdr.PaletteType == 0 || ctx->pcxHdr.PaletteType > 2 || /* Some files use 0 and other values, so we ignore it */
			ctx->pcxHdr.PaletteType == 1 ||
			ctx->pcxHdr.PaletteType == 2) && (
			ctx->pcxHdr.Version == PCX_PAINTBRUSHVER_25_EGA ||
			ctx->pcxHdr.Version == PCX_PAINTBRUSHVER_28_PALETTE ||
			ctx->pcxHdr.Version == PCX_PAINTBRUSHVER_28_NOPALETTE ||
			ctx->pcxHdr.Version == PCX_PAINTBRUSHVER_WINDOWS ||
			ctx->pcxHdr.Version == PCX_PAINTBRUSHVER_30))
		{
			/* Allocate buffer and read the data */
			ctx->buffer = (unsigned char*)malloc(ctx->bufferLen);
			bytesRead = fread(ctx->buffer, sizeof(char), ctx->bufferLen, f);

			assert(bytesRead == ctx->bufferLen);

			ctx->maxNumberOfColors = (1L << (ctx->pcxHdr.BitsPerPixel * ctx->pcxHdr.NumBitPlanes));
			ctx->scanLineLength = (ctx->pcxHdr.BytesPerLine * ctx->pcxHdr.NumBitPlanes);

			ctx->imageWidth = ctx->pcxHdr.XEnd - ctx->pcxHdr.XStart + 1; /* Width of image in pixels */
			ctx->imageHeight = ctx->pcxHdr.YEnd - ctx->pcxHdr.YStart + 1; /* Length of image in scan lines */

			ctx->linePaddingSize = (ctx->pcxHdr.BitsPerPixel != 8 ?
				(ctx->scanLineLength * (8 / ctx->pcxHdr.BitsPerPixel) - ctx->imageWidth) : 0);

			/* Set default EGA palette */
			if(ctx->pcxHdr.Version == PCX_PAINTBRUSHVER_28_NOPALETTE)
			{
				const unsigned char EGA_DEFAULT_PALETTE[] = {
					0x00, 0x00, 0x00, /* black */
					0x00, 0x00, 0xAA, /* blue */
					0x00, 0xAA, 0x00, /* green */
					0x00, 0xAA, 0xAA, /* cyan */
					0xAA, 0x00, 0x00, /* red */
					0xAA, 0x00, 0xAA, /* magenta */
					0xAA, 0x55, 0x00, /* brown */
					0xAA, 0xAA, 0xAA, /* light gray */
					0x55, 0x55, 0x55, /* gray */
					0x55, 0x55, 0xFF, /* light blue */
					0x55, 0xFF, 0x55, /* light green */
					0x55, 0xFF, 0xFF, /* light cyan */
					0xFF, 0x55, 0x55, /* light red */
					0xFF, 0x55, 0xFF, /* light magenta */
					0xFF, 0xFF, 0x55, /* yellow */
					0xFF, 0xFF, 0xFF  /* white */
				};

				memcpy(ctx->pcxHdr.Palette, EGA_DEFAULT_PALETTE, sizeof(EGA_DEFAULT_PALETTE));
			}
		}
		else
		{
			/* Invalid header - close file */
			PCXClose(ctx);
			ctx = NULL;
		}

		/* Close pcx file */
		fclose(f);
	}

	return ctx;
}

unsigned char *PCXReadImage(PCXContext *ctx)
{
	assert(ctx);

	ctx->imageLen = ctx->scanLineLength * ctx->imageHeight;
	ctx->image = (unsigned char*)malloc(ctx->imageLen);

	unsigned char *p = ctx->buffer;
	int total = 0;

	if (ctx->pcxHdr.Encoding)
	{
		for (int i = 0; i < ctx->imageHeight; ++i)
		{
			/* RLE decode scan line */
			if (readScanLineRle(ctx, &p, ctx->image + i * ctx->scanLineLength, ctx->scanLineLength, &total)
				&& i + 1 != ctx->imageHeight)
			{
				PCXFreeData(ctx);
				return NULL;
			}
		}
	}
	else
	{
		/* Copy uncompressed data */
		if (ctx->imageLen <= ctx->bufferLen)
		{
			memcpy(&ctx->image, p, ctx->imageLen);
			p += ctx->imageLen;
			total = ctx->imageLen;
		}
		else
		{
			PCXFreeData(ctx);
			return NULL;
		}
	}

	/* Check boundaries */
	if (ctx->imageLen < calcImageSize(ctx))
	{
		PCXFreeData(ctx);
		return NULL;
	}

	/* Zero-out unused space (possibly corrupt image) */
	if (total < (int)ctx->imageLen)
	{
		memset(ctx->image + total, 0, ctx->imageLen - total);
	}

	/* Check whether there aren't another 768 bytes of color VGA palette before eof */
	if (ctx->bufferLen - (p - ctx->buffer) == PCX_VGAPALETTELENGTH + 1)
	{
		if (*p++ == PCX_VGAPALETTEMAGIC)
		{
			/* Read VGA palette */
			ctx->paletteVga = (unsigned char*)malloc(PCX_VGAPALETTELENGTH);
			memcpy(ctx->paletteVga, p, PCX_VGAPALETTELENGTH);
		}
	}

	return ctx->image;
}

void PCXFreeData(PCXContext *ctx)
{
	assert(ctx);

	if (ctx->paletteVga)
	{
		free(ctx->paletteVga);
		ctx->paletteVga = NULL;
	}

	if (ctx->image)
	{
		free(ctx->image);
		ctx->image = NULL;
		ctx->imageLen = 0;
	}

	if (ctx->buffer)
	{
		free(ctx->buffer);
		ctx->buffer = NULL;
		ctx->bufferLen = 0;
	}
}

void PCXClose(PCXContext *ctx)
{
	assert(ctx);

	PCXFreeData(ctx);
	free(ctx);
}

unsigned int calcImageSize(PCXContext *ctx)
{
	unsigned int imgSize = ctx->imageWidth * ctx->imageHeight;

	if (ctx->pcxHdr.BitsPerPixel >= 8)
		imgSize *= (ctx->pcxHdr.BitsPerPixel / 8);
	else
		imgSize /= (8 / ctx->pcxHdr.BitsPerPixel);

	imgSize *= ctx->pcxHdr.NumBitPlanes;

	return imgSize;
}

int readScanLineRle(PCXContext *ctx, unsigned char **pbuf, unsigned char *line, int len, int *total)
{
	int cnt = 0, val = 0, i = 0;

	do
	{
		if (*pbuf - ctx->buffer >= ctx->bufferLen)
			return 1;

		int byte = *(*pbuf)++;

		if ((byte & 0xC0) == 0xC0) /* 2-byte code */
		{
			if (*pbuf - ctx->buffer >= ctx->bufferLen)
				return 1;

			cnt = byte & 0x3F;
			val = *(*pbuf)++;
		}
		else /* 1-byte code */
		{
			cnt = 1;
			val = byte;
		}

		if (*total + cnt > (int)ctx->imageLen)
			return 1;

		/* Write the pixel run to image data buffer */
		for (*total += cnt; cnt && i < len; cnt--, i++)
			line[i] = val;

	} while (i < len);

	return 0;
}
