/*

			   Hex dump utility

			    by John Walker
	       WWW home page: http://www.fourmilab.ch/

		This program is in the public domain.

*/

#include <stdio.h>
#include <string.h>

#define EOS     '\0'

static char addrformat[80] = "%6X";
static char dataformat1[80] = "%02X";
static int bytesperline = 16, doublechar = 0,
	   dflen = 2;
static unsigned long fileaddr;
static unsigned char lineecho[32];

/*  OUTLINE  --  Edit a line of binary data into the selected output
		 format.  */

static void outline(out, dat, len)
  FILE *out;
  unsigned char *dat;
  int len;
{
    char oline[132];
    int i;

    sprintf(oline, addrformat, fileaddr);
    strcat(oline, ":");
    for (i = 0; i < len; i++) {
	char outedit[80];

	sprintf(outedit, dataformat1, dat[i]);
        strcat(oline, (i == (bytesperline / 2)) ? "  " : " ");
	strcat(oline, outedit);
    }

    if (doublechar) {
	char oc[2];
	int shortfall = ((bytesperline - len) * (dflen + 1)) +
			(len <= (bytesperline / 2) ? 1 : 0);

	while (shortfall-- > 0) {
            strcat(oline, " ");
	}
	oc[1] = EOS;
        strcat(oline, " | ");
	for (i = 0; i < len; i++) {
	    int b = dat[i];

            /* Map non-printing characters to "." according to the
	       definitions for ISO 8859/1 Latin-1. */

            if (b < ' ' || (b > '~' && b < 145)
			|| (b > 146 && b < 160)) {
                b = '.';
	    }
	    oc[0] = b;
	    strcat(oline, oc);
	}
    }
    strcat(oline, "\n");
    fputs(oline, out);
}

/*  XD	--  Dump a buffer.

	    xd(out, buf, bufl, dochar);

	    out     FILE * to which output is sent.
	    buf     Address of buffer to dump.
	    bufl    Buffer length in bytes.
	    dochar  If nonzero, show ASCII/ISO characters
		    as well as hexadecimal.

*/

void xd(out, buf, bufl, dochar)
  FILE *out;
  unsigned char *buf;
  int bufl, dochar;
{
    int b, bp;

    bp = 0;
    fileaddr = 0;
    doublechar = dochar;

    while (bufl-- > 0) {
	b = *buf++;
	if (bp >= bytesperline) {
	    outline(out, lineecho, bp);
	    bp = 0;
	    fileaddr += bytesperline;
	}
	lineecho[bp++] = b;
    }

    if (bp > 0) {
	outline(out, lineecho, bp);
    }
}
