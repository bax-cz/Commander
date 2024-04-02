#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
PalmDoc files are decoded as follows:

Read a byte from the compressed stream. If the byte is
0x00: "1 literal" copy that byte unmodified to the decompressed stream.
0x09 to 0x7f: "1 literal" copy that byte unmodified to the decompressed stream.
0x01 to 0x08: "literals": the byte is interpreted as a count from 1 to 8, and that many literals are copied unmodified from the compressed stream to the decompressed stream.
0x80 to 0xbf: "length, distance" pair: the 2 leftmost bits of this byte ('10') are discarded, and the following 6 bits are combined with the 8 bits of the next byte to make a 14 bit "distance, length" item.
              Those 14 bits are broken into 11 bits of distance backwards from the current location in the uncompressed text, and 3 bits of length to copy from that point (copying n+3 bytes, 3 to 10 bytes).
0xc0 to 0xff: "byte pair": this byte is decoded into 2 characters: a space character, and a letter formed from this byte XORed with 0x80.
Repeat from the beginning until there is no more bytes in the compressed file.
*/

unsigned char *decodeLz77(const unsigned char *data, unsigned int data_len, unsigned int *uncompressed_len)
{
	assert(data && uncompressed_len);
	assert(data_len && data_len < *uncompressed_len);

	unsigned char *data_out = malloc(*uncompressed_len);
	unsigned char ch, *p = data_out;
	unsigned int i;

	if (data_out == NULL)
		return NULL;

	for (i = 0; i < data_len; i++)
	{
		ch = data[i];

		if (ch == 0x00 || (ch >= 0x09 && ch <= 0x7F)) // 1 literal
			*p++ = ch;
		else if (ch >= 0x01 && ch <= 0x08) // literals
		{
			while (ch--)
				*p++ = data[++i];
		}
		else if (ch >= 0x80 && ch <= 0xBF) // length, distance
		{
			unsigned short tmp = (ch << 8) + data[++i];
			unsigned short distance = ((tmp & 0x3FFF) >> 3);
			unsigned short length = ((tmp & 0x07) + 3);

			while (length--)
				*p++ = *(p - distance);
		}
		else if (ch >= 0xC0 && ch <= 0xFF) // byte pair
		{
			*p++ = ' ';
			*p++ = ch ^ 0x80;
		}
	}

	*uncompressed_len = (unsigned int)(p - data_out);

	return data_out;
}
