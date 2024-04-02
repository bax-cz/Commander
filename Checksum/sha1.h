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
 * SHA1 implementation for PuTTY. Written directly from the spec by
 * Simon Tatham.
 */

#ifndef _SHA1_H
#define _SHA1_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

typedef struct {
	uint32 h[5];
	unsigned char block[64];
	int blkused;
	uint32 lenhi, lenlo;
} SHA_State;

#define rol(x,y) ( ((x) << (y)) | (((uint32)x) >> (32-y)) )

	void SHA_Init(SHA_State * s);
	void SHA_Bytes(SHA_State * s, void *p, int len);
	void SHA_Final(SHA_State * s, unsigned char *output);

#ifdef __cplusplus
}
#endif

#endif
