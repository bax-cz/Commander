/*

			   P A L M R E C
			   =============

      Routines for parsing records in Palm .pdb and .prc files.

      The functions in this file are used in conjunction with those
      in palmread.c.  After a record or resource has been read with
      palmReadRecord or palmReadResource, a pointer to the item is
      kept in the palmreadContext structure.  The following functions
      permit extracting fields from the record in a byte-order-independent
      manner.

      All functions returning numbers return the unsigned variant;
      simply cast them to the corresponding signed type if that's
      what you want.

			    by John Walker
		       http://www.fourmilab.ch/
		 This program is in the public domain

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pdb.h"
#include "palmread.h"
#include "palmrec.h"

/*  palmRecParse  --  Set up to parse a new record.  You can call
			  this to parse an in-memory record other than
			  the last read with palmRead.  In fact, you can
					  use this with a palmreadContext which wasn't
			  initialised by palmReadHeader.  */

void palmRecParse(struct palmreadContext *p, void *buf, long bufl)
{
	p->lastrec = (unsigned char *)buf;
	p->lastlength = bufl;
	p->recptr = 0;
}

/*  palmRecByte  --  Return the next byte from the record.  */

unsigned char palmRecByte(struct palmreadContext *p)
{
	assert(p->lastrec != NULL);
	assert(p->recptr < p->lastlength);

	return p->lastrec[p->recptr++];
}

/*  palmRecShort  --  Return the next short from the record.  */

unsigned short palmRecShort(struct palmreadContext *p)
{
	unsigned short s;

	assert(p->lastrec != NULL);
	assert((p->recptr + 1) < p->lastlength);

	s = (p->lastrec[p->recptr] << 8) | p->lastrec[p->recptr + 1];
	p->recptr += 2;

	return s;
}

/*  palmRecLong  --  Return the next long from the record.  */

unsigned long palmRecLong(struct palmreadContext *p)
{
	unsigned long l;

	assert(p->lastrec != NULL);
	assert((p->recptr + 3) < p->lastlength);

	l = (p->lastrec[p->recptr] << 24) |
		(p->lastrec[p->recptr + 1] << 16) |
		(p->lastrec[p->recptr + 2] << 8) |
		p->lastrec[p->recptr + 3];
	p->recptr += 4;

	return l;
}

/*  palmRecBytes  --  Return a sequence of consecutive bytes
			  from the record.	*/

void palmRecBytes(struct palmreadContext *p, void *buf, int bufl)
{
	assert(p->lastrec != NULL);
	assert((p->recptr + (bufl - 1)) < p->lastlength);

	memcpy(buf, p->lastrec + p->recptr, bufl);
	p->recptr += bufl;
}

/*  palmRecString  --  Return a zero-terminated string from
			   the record.  */

void palmRecString(struct palmreadContext *p, char *buf, int bufl)
{
	int l = (int)strlen((char *)(p->lastrec + p->recptr)) + 1;

	assert(p->lastrec != NULL);
	assert((p->recptr + (l - 1)) < p->lastlength);

	if (bufl < l) {
		memcpy(buf, p->lastrec + p->recptr, bufl - 1);
		p->lastrec[p->recptr + (bufl - 1)] = 0;
	}
	else {
		strcpy(buf, (char *)(p->lastrec + p->recptr));
	}
	p->recptr += l;
}
