/****************************************************************************
	This file is part of MD5/SHA1 checksum generator/checker plugin for
	Total Commander.
	Copyright (C) 2003  Stanislaw Y. Pusep

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	E-Mail:	stanis@linuxmail.org
	Site:	http://sysdlabs.hypermart.net/
****************************************************************************/


/*
 * MD5 implementation for PuTTY. Written directly from the spec by
 * Simon Tatham.
 */

#ifndef _MD5_H
#define _MD5_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

typedef struct {
	uint32 h[4];
} MD5_CoreState;

struct MD5_Context {
	MD5_CoreState core;
	unsigned char block[64];
	int blkused;
	uint32 lenhi, lenlo;
};

#define BLKSIZE 64

#define F(x,y,z) ( ((x) & (y)) | ((~(x)) & (z)) )
#define G(x,y,z) ( ((x) & (z)) | ((~(z)) & (y)) )
#define H(x,y,z) ( (x) ^ (y) ^ (z) )
#define I(x,y,z) ( (y) ^ ( (x) | ~(z) ) )

#define rol(x,y) ( ((x) << (y)) | (((uint32)x) >> (32-y)) )

#define subround(f,w,x,y,z,k,s,ti) w = x + rol(w + f(x,y,z) + block[k] + ti, s)

void MD5_Init(struct MD5_Context *context);
void MD5_Update(struct MD5_Context *context, unsigned char const *buf, unsigned len);
void MD5_Final(unsigned char digest[16], struct MD5_Context *context);

#ifdef __cplusplus
}
#endif

#endif
