#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "growbuff.h"

/*
	Simple dynamically growing buffer.
*/

void initBuff(GrowBuff *buff)
{
	assert(buff);

	buff->alloc_size = kAllocSize;
	buff->data = malloc(kAllocSize);
	buff->data_len = 0;

	assert("malloc failed" && buff->data != NULL);
}

void appendBuff(GrowBuff *buff, const unsigned char *data, unsigned long len)
{
	assert(buff);
	assert(data);

	if (buff->data_len + len > buff->alloc_size) {
		/* realloc data buffer */
		unsigned char *tmp = NULL;

		while (buff->alloc_size < buff->data_len + len) {
			buff->alloc_size <<= 1;
		}

		tmp = realloc(buff->data, buff->alloc_size);

		if (tmp)
			buff->data = tmp;
		else {
			assert("realloc failed" && 1 != 1);
			return;
		}
	}

	/* append data at the end of the buffer */
	memcpy(buff->data + buff->data_len, data, len);
	buff->data_len += len;
}

void deinitBuff(GrowBuff *buff)
{
	assert(buff);

	free(buff->data);

	buff->data = NULL;
	buff->data_len = 0;
}
