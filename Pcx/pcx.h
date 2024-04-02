#pragma once

/* PCX Versions */
#define PCX_PAINTBRUSHVER_25_EGA          0  /* Version 2.5 with fixed EGA palette information */
#define PCX_PAINTBRUSHVER_28_PALETTE      2  /* Version 2.8 with modifiable EGA palette information */
#define PCX_PAINTBRUSHVER_28_NOPALETTE    3  /* Version 2.8 without palette information */
#define PCX_PAINTBRUSHVER_WINDOWS         4  /* PC Paintbrush for Windows */
#define PCX_PAINTBRUSHVER_30              5  /* Version 3.0 of PC Paintbrush, PC Paintbrush Plus, PC Paintbrush Plus for Windows, Publisher's Paintbrush, and all 24-bit image files */

/* VGA palette definitions */
#define PCX_VGAPALETTEMAGIC          0x000C
#define PCX_VGAPALETTELENGTH         0x0300

/* PCX Header */
typedef struct _PcxHeader {
	unsigned char  Identifier;       /* PCX Id Number (Always 0x0A) */
	unsigned char  Version;          /* Version Number */
	unsigned char  Encoding;         /* Encoding Format */
	unsigned char  BitsPerPixel;     /* Bits per Pixel */
	unsigned short XStart;           /* Left of image */
	unsigned short YStart;           /* Top of Image */
	unsigned short XEnd;             /* Right of Image */
	unsigned short YEnd;             /* Bottom of image */
	unsigned short HorzRes;          /* Horizontal Resolution */
	unsigned short VertRes;          /* Vertical Resolution */
	unsigned char  Palette[48];      /* 16-Color EGA Palette */
	unsigned char  Reserved1;        /* Reserved (Always 0) */
	unsigned char  NumBitPlanes;     /* Number of Bit Planes */
	unsigned short BytesPerLine;     /* Bytes per Scan-line */
	unsigned short PaletteType;      /* Palette Type: 1 indicates color or monochrome information, 2 indicates gray-scale information */
	unsigned short HorzScreenSize;   /* Horizontal Screen Size */
	unsigned short VertScreenSize;   /* Vertical Screen Size */
	unsigned char  Reserved2[54];    /* Reserved (Always 0) */
} PCXHEADER;

/* PCX Context */
typedef struct _PcxContext {
	PCXHEADER      pcxHdr;    /* PCX header */
	unsigned char *buffer;    /* Temporary file data */
	unsigned int   bufferLen; /* Temporary data length */
	unsigned char *image;     /* Image data */
	unsigned int   imageLen;  /* Image data length */
	int            maxNumberOfColors;
	int            scanLineLength;
	int            imageWidth;
	int            imageHeight;
	int            linePaddingSize;
	unsigned char *paletteVga;
} PCXContext;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/* Open PCX file */
PCXContext *PCXOpen(const wchar_t *fname);

/* Read PCX image data */
unsigned char *PCXReadImage(PCXContext *ctx);

/* Free PCX image data */
void PCXFreeData(PCXContext *ctx);

/* Close PCX file */
void PCXClose(PCXContext *ctx);

#ifdef __cplusplus
}
#endif // __cplusplus
