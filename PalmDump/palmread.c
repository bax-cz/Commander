/*

			   P A L M R E A D
			   ===============

	    Routines for reading Palm .pdb and .prc files.

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

/*  Byte order independent file input routines.  */

static int inbyte(struct palmreadContext *c)
{
	return (unsigned char)getc(c->fi);
}

static short inshort(struct palmreadContext *c)
{
	unsigned char b[2];

	fread(b, 2, 1, c->fi);
	return (b[0] << 8) | b[1];
}

static long inlong(struct palmreadContext *c)
{
	unsigned char b[4];

	fread(b, 4, 1, c->fi);
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

static void inbytes(struct palmreadContext *c, char *s, int len)
{
	fread(s, len, 1, c->fi);
}

/*  palmReadHeader  --	Initialise to read PDB/PRC file.  The
			header is read into a buffer supplied
			by the caller and a read context is
			returned which the caller may use to
			access the contents of the file.  */

struct palmreadContext *palmReadHeader(FILE *fp, PDBHeader *ph)
{
	struct palmreadContext *p;
	int i;

	p = malloc(sizeof(struct palmreadContext));
	if (p == NULL) {
		return NULL;
	}
	p->fi = fp;

	/*	Determine the length of the file.  We need to know this
	in order to determine the length of the last record
	in the file.  */

	fseek(p->fi, 0L, SEEK_END);
	p->fileLength = ftell(p->fi);
	rewind(p->fi);

	/*  Read the header into the caller's PDBHeader structure.  */

	inbytes(p, ph->name, kMaxPDBNameSize);
	if (memchr(ph->name, '\0', kMaxPDBNameSize) == NULL)
		return NULL; // invalid data

	ph->flags = inshort(p);
	ph->version = inshort(p);
	ph->creationTime = inlong(p);
	ph->modificationTime = inlong(p);
	ph->backupTime = inlong(p);
	ph->modificationNumber = inlong(p);
	ph->appInfoOffset = inlong(p);
	ph->sortInfoOffset = inlong(p);
	ph->type = inlong(p);
	ph->creator = inlong(p);
	ph->uniqueID = inlong(p);
	ph->nextRecordID = inlong(p);
	p->nrecords = ph->numRecords = inshort(p);

	p->isResource = !!(ph->flags & pdbResourceFlag);
	p->rsp = NULL;
	p->rcp = NULL;

	p->lengths = (long *)malloc(sizeof(long) * p->nrecords);
	if (p->lengths == NULL) {
		free(p);
		return NULL;
	}

	if (p->isResource) {
		p->rsp = (struct PDBResourceEntry *)
			malloc(sizeof(struct PDBResourceEntry) * p->nrecords);
		if (p->rsp == NULL) {
			free(p->lengths);
			free(p);
			return NULL;
		}

		for (i = 0; i < p->nrecords; i++) {
			p->rsp[i].type = inlong(p);
			p->rsp[i].id = inshort(p);
			p->rsp[i].offset = inlong(p);
			if (i > 0) {
				p->lengths[i - 1] = p->rsp[i].offset - p->rsp[i - 1].offset;
			}
		}
		if (p->nrecords > 0) {
			p->lengths[p->nrecords - 1] = p->fileLength -
				p->rsp[p->nrecords - 1].offset;
		}
	}
	else {
		p->rcp = (struct PDBRecordEntry *)
			malloc(sizeof(struct PDBRecordEntry) * p->nrecords);
		if (p->rcp == NULL) {
			free(p->lengths);
			free(p);
			return NULL;
		}
		for (i = 0; i < p->nrecords; i++) {
			long l;

			p->rcp[i].offset = inlong(p);
			/*printf("Record %d offset = %d\n", i, p->rcp[i].offset);*/
			l = inlong(p);
			p->rcp[i].attr = (l >> 24) & 0xFF;
			p->rcp[i].uniqueID = l & 0xFFFFFF;
			if (i > 0) {
				p->lengths[i - 1] = p->rcp[i].offset - p->rcp[i - 1].offset;
			}
		}
		if (p->nrecords > 0) {
			p->lengths[p->nrecords - 1] = p->fileLength -
				p->rcp[p->nrecords - 1].offset;
		}
	}

	p->lastrec = NULL;
	p->lastlength = 0;
	p->recptr = 1;

	return p;
}

/*  palmReadResource  --  Read a resource from a PDB/PRC file and
			  return in a newly-allocated buffer (which
						  it's the responsibility of the caller to
			  release).  */

void *palmReadResource(struct palmreadContext *p, int resno,
	PDBResourceEntry *re, long *rlen)
{
	char *rec;

	assert(p->isResource);
	assert((resno >= 0) && (resno < p->nrecords));

	rec = (char *)malloc(p->lengths[resno]);
	if (rec != NULL) {
		fseek(p->fi, p->rsp[resno].offset, SEEK_SET);
		fread(rec, p->lengths[resno], 1, p->fi);
		memcpy(re, &(p->rsp[resno]), sizeof(struct PDBResourceEntry));
		*rlen = p->lengths[resno];
	}

	p->lastrec = (unsigned char *)rec;
	p->lastlength = p->lengths[resno];
	p->recptr = 0;
	return (void *)rec;
}

/*  palmReadRecord  --	Read a resource from a PDB file and
			return in a newly-allocated buffer (which
						it's the responsibility of the caller to
			release).  */

void *palmReadRecord(struct palmreadContext *p, int recno,
	PDBRecordEntry *re, long *rlen)
{
	char *rec;

	assert(!p->isResource);
	assert((recno >= 0) && (recno < p->nrecords));

	rec = (char *)malloc(p->lengths[recno]);
	if (rec != NULL) {
		fseek(p->fi, p->rcp[recno].offset, SEEK_SET);
		fread(rec, p->lengths[recno], 1, p->fi);
		memcpy(re, &(p->rcp[recno]), sizeof(struct PDBRecordEntry));
		*rlen = p->lengths[recno];
	}

	p->lastrec = (unsigned char *)rec;
	p->lastlength = p->lengths[recno];
	p->recptr = 0;
	return (void *)rec;
}

/*  palmDisposeContext	--  Dispose of context, releasing
				all storage.  */

void palmDisposeContext(struct palmreadContext *p)
{
	if (p->rsp != NULL) {
		free(p->rsp);
	}
	if (p->rcp != NULL) {
		free(p->rcp);
	}
	if (p->lengths != NULL) {
		free(p->lengths);
	}
	free(p);
}
