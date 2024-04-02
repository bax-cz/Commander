/*
 *  tga.h - Libtga header
 *
 *  Copyright (C) 2001-2002, Matthias Brueckner
 *  This file is part of the TGA library (libtga).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef __TGA_H
#define __TGA_H 1

#include "tgaconfig.h"	/* include our config header before anything else */
#include <stdio.h>

/* Tell C++ that we have C types and declarations. */
#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

/* __FILE__ and __LINE__ are gcc specific */
#ifndef __FILE__
# define __FILE__ "unknown"
#endif
#ifndef __LINE__
# define __LINE__ 0
#endif

/* sections */
#define TGA_IMAGE_ID	0x01
#define TGA_IMAGE_INFO	0x02
#define TGA_IMAGE_DATA	0x04
#define TGA_COLOR_MAP	0x08
/* RLE */
#define TGA_RLE_ENCODE  0x10

/* color format */
#define TGA_RGB		0x20
#define TGA_BGR		0x40

/* orientation */
#define TGA_BOTTOM	0x0
#define TGA_TOP		0x1
#define	TGA_LEFT	0x0
#define	TGA_RIGHT	0x1

/* version info */
#define LIBTGA_VER_MAJOR  	1
#define LIBTGA_VER_MINOR  	0
#define LIBTGA_VER_PATCH	1
#define LIBTGA_VER_STRING	"1.0.1"

/* error codes */
enum {	TGA_OK = 0,				/* success */
		TGA_ERROR,
		TGA_OOM,				/* out of memory */
		TGA_OPEN_FAIL,
		TGA_SEEK_FAIL,
		TGA_READ_FAIL,
		TGA_WRITE_FAIL,
		TGA_UNKNOWN_SUB_FORMAT	/* invalid bit depth */
};

#define TGA_ERRORS 8  /* total number of error codes */

/* text strings corresponding to the error codes */
static const char*
tga_error_strings[] = {
	"Success",
	"Error",
	"Out of memory",
	"Failed to open file",
	"Seek failed",
	"Read failed",
	"Write failed",
	"Unknown sub-format"
};

typedef unsigned char  tbyte;
typedef unsigned char  tuint8;
typedef unsigned short tuint16;
typedef unsigned int   tuint32;

typedef struct _TGAHeader TGAHeader;
typedef struct _TGAData   TGAData;
typedef struct _TGA       TGA;


typedef void (*TGAErrorProc)(TGA*, int);


/* TGA image header */
struct _TGAHeader {
	tbyte	id_len;		/* image id length */
	tbyte	map_t;		/* color map type */
	tbyte	img_t;		/* image type */
	tuint16	map_first;	/* index of first map entry */
	tuint16	map_len;	/* number of entries in color map */
	tbyte	map_entry;	/* bit-depth of a cmap entry */
	tuint16	x;			/* x-coordinate */
	tuint16	y;			/* y-coordinate */
	tuint16	width;		/* width of image */
	tuint16	height;		/* height of image */
	tbyte	depth;		/* pixel-depth of image */
	tbyte   alpha;		/* alpha bits */
	tbyte	horz;		/* horizontal orientation */
	tbyte	vert;		/* vertical orientation */
};

/* TGA image data */
struct _TGAData {  
	tbyte	*img_id;	/* image id */
	tbyte	*cmap;		/* color map */
	tbyte	*img_data;	/* image data */
	tuint32  flags;
};

/* TGA image handle */
struct _TGA {
	FILE*			fd;		/* file stream */
	size_t			off;	/* current offset in file*/
	int				last;	/* last error code */
	TGAHeader		hdr;	/* image header */
	TGAErrorProc 	error;	/* user-defined error proc */
};


__BEGIN_DECLS

TGA *TGAOpen (const wchar_t *name, const wchar_t *mode);
TGA *TGAOpenFd (FILE *fd);
int TGAReadImage (TGA *tga, TGAData *data);
int TGAWriteImage (TGA *tga, TGAData *data);
void TGAFreeData (TGAData *data);
void TGAClose (TGA *tga);

const char *TGAStrError (tuint8 code);

int TGAReadHeader (TGA *tga);
int TGAReadImageId (TGA *tga, tbyte **id);
int TGAReadColorMap (TGA *tga, tbyte **cmap, tuint32 flags);
size_t TGAReadScanlines (TGA *tga, tbyte *buf, size_t sln, size_t n, tuint32 flags);
int TGAWriteHeader (TGA *tga);
int TGAWriteImageId (TGA *tga, const tbyte *id);
int TGAWriteColorMap (TGA *tga, tbyte *cmap, tuint32 flags);
size_t TGAWriteScanlines (TGA *tga, tbyte *buf, size_t sln, size_t n, tuint32 flags);

size_t __TGASeek (TGA *tga, size_t off, int whence);
void __TGAbgr2rgb (tbyte *data, size_t size, size_t bytes);

__END_DECLS


#define TGA_HEADER_SIZE         18
#define TGA_CMAP_SIZE(tga)      ((tga)->hdr.map_len * (tga)->hdr.map_entry / 8)
#define TGA_CMAP_OFF(tga)       (TGA_HEADER_SIZE + (tga)->hdr.id_len)
#define TGA_IMG_DATA_OFF(tga)   (TGA_HEADER_SIZE + (tga)->hdr.id_len + TGA_CMAP_SIZE(tga))
#define TGA_IMG_DATA_SIZE(tga)  ((tga)->hdr.width * (tga)->hdr.height * (tga)->hdr.depth / 8)
#define TGA_SCANLINE_SIZE(tga)  ((tga)->hdr.width * (tga)->hdr.depth / 8)
#define TGA_CAN_SWAP(depth)     (depth == 24 || depth == 32)

#define TGA_IS_MAPPED(tga)      ((tga)->hdr.map_t == 1)
#define TGA_IS_ENCODED(tga)     ((tga)->hdr.img_t > 8 && (tga)->hdr.img_t < 12)

#define TGA_ERROR(tga, code) \
if((tga) && (tga)->error) (tga)->error(tga, code);\
fprintf(stderr, "Libtga:%s:%d: %s\n", __FILE__, __LINE__, TGAStrError(code));\
if(tga) (tga)->last = code\


#endif /* __TGA_H */
