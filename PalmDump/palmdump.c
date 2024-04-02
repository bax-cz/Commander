/*

P A L M D U M P
===============

Dump a Palm OS .pdb or .prc file.

by John Walker
http://www.fourmilab.ch/
This program is in the public domain

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "pdb.h"
#include "palmread.h"
#include "palmdoc.h"
#include "palmmobi.h"

extern void xd(FILE *out, unsigned char *buf, int bufl, int dochar);

/*	Extract a 4 character string jammed into a long.  */

#define stringAlong(s, l)       \
	s[0] = (char) (l >> 24);    \
	s[1] = (char) (l >> 16);    \
	s[2] = (char) (l >> 8);     \
	s[3] = (char) (l);          \
	s[4] = 0

/*  palmDate  --  Edit Palm date and time, checking for invalid
dates and erroneous PC/Unix style dates.  */

static char *palmDate(unsigned long t)
{
	struct tm *utm;
	time_t ut = (time_t) t;
	static char s[132];
	static char invalid[] = "Invalid (%lu)\n";

	/*	Microsoft marching morons return NULL from localtime() if
	passed a date before the epoch, thus planting a Y2038
	bomb in every program that uses the function.  Just in
	case, we test for NULL return and say "Invalid" if so.  */

	if (ut < timeOffset) {	      /* This looks like a PC/Unix style date */
		utm = localtime(&ut);

		if (utm == NULL) {
			sprintf(s, invalid, t);
			return s;
		}
		sprintf(s, "PC/Unix time: %s", asctime(utm));
		return s;
	}

	/* Otherwise assume it is a Palm/Macintosh style date */

	ut = ((unsigned long) ut) - timeOffset;
	utm = localtime(&ut);
	if (utm == NULL) {
		sprintf(s, invalid, t);
		return s;
	}
	sprintf(s, "Palm time: %s", asctime(utm));
	return s;
}

/*  Main program.  */

int dumpPalmFile(const wchar_t *inname, const wchar_t *outname)
{
	FILE *fi = NULL, *fo = NULL;

	int i, content = 0;
	struct palmreadContext *rc;
	PDBHeader ph;
	char dbname[kMaxPDBNameSize + 2];
	wchar_t outdir[_MAX_PATH], *p;

	fi = _wfopen(inname, L"rb");
	if (fi == NULL) {
		fwprintf(stderr, L"Cannot open input file %ls.\n", inname);
		return 11;
	}

	fo = _wfopen(outname, L"wb");
	if (fo == NULL) {
		fwprintf(stderr, L"Cannot open output file %ls.\n", outname);
		fclose(fi);
		return 12;
	}

	rc = palmReadHeader(fi, &ph);

	if (rc == NULL) {
		fprintf(stderr, "Cannot read Palm file header.\n");
		fclose(fi);
		fclose(fo);
		return 10;
	}

	p = wcsrchr(outname, '\\');
	if (p) {
		size_t len = min(p + 1 - outname, _MAX_PATH - 2);
		swprintf(outdir, len, L"%ls", outname);
	}
	else
		wcscpy(outdir, L".");

	/*
		Only PalmDOC and MOBI formats are supported - Type: "TEXt", Creator ID: "REAd"
		Type: "BOOK ", Creator ID: "MOBI"
	*/
	if ((ph.creator == kPalmDocCreator && ph.type == kPalmDocType) ||
		(ph.creator == kPalmMobiCreator && ph.type == kPalmMobiType))
		content = 1;

	/*	Dump header.  */
	if (!content)
	{
		memcpy(dbname, ph.name, kMaxPDBNameSize);
		dbname[kMaxPDBNameSize] = 0;      /* Guarantee null terminated */
		fprintf(fo, "Database name:           %s\n", dbname);
		fprintf(fo, "Flags:                   0x%X", ph.flags);
		if (ph.flags & pdbOpenFlag) {
			fprintf(fo, " Open");
		}
		if (ph.flags & pdbStream) {
			fprintf(fo, " Stream");
		}
		if (ph.flags & pdbResetAfterInstall) {
			fprintf(fo, " ResetAfterInstall");
		}
		if (ph.flags & pdbOKToInstallNewer) {
			fprintf(fo, " InstallNewerOK");
		}
		if (ph.flags & pdbBackupFlag) {
			fprintf(fo, " Backup");
		}
		if (ph.flags & pdbAppInfoDirtyFlag) {
			fprintf(fo, " AppInfoDirty");
		}
		if (ph.flags & pdbReadOnlyFlag) {
			fprintf(fo, " Read-only");
		}
		if (ph.flags & pdbResourceFlag) {
			fprintf(fo, " Resource");
		}
		fprintf(fo, "\n");
		fprintf(fo, "Version:                 0x%X", ph.version);
		if (ph.version & pdbVerShowSecret) {
			fprintf(fo, " ShowSecret");
		}
		if (ph.version & pdbVerExclusive) {
			fprintf(fo, " Exclusive");
		}
		if (ph.version & pdbVerLeaveOpen) {
			fprintf(fo, " LeaveOpen");
		}
		if ((ph.version & 3) == pdbVerReadWrite) {
			fprintf(fo, " Read/write");
		}
		if ((ph.version & 3) == pdbVerWrite) {
			fprintf(fo, " Write");
		}
		if ((ph.version & 3) == pdbVerReadOnly) {
			fprintf(fo, " Read-only");
		}
		fprintf(fo, "\n");

		fprintf(fo, "Creation time:           %s", palmDate(ph.creationTime));
		fprintf(fo, "Modification time:       %s", palmDate(ph.modificationTime));
		fprintf(fo, "Backup time:             %s", ph.backupTime == 0 ? "Never\n" :
			palmDate(ph.backupTime));


		fprintf(fo, "Modification number:     %lu\n", ph.modificationNumber);
		fprintf(fo, "Application Info offset: %lu\n", ph.appInfoOffset);
		fprintf(fo, "Sort Info offset:        %lu\n", ph.sortInfoOffset);
		stringAlong(dbname, ph.type);
		fprintf(fo, "Type:                    %s\n", dbname);
		stringAlong(dbname, ph.creator);
		fprintf(fo, "Creator:                 %s\n", dbname);
		fprintf(fo, "Unique ID:               %lu\n", ph.uniqueID);
		fprintf(fo, "Next record ID:          %lu\n", ph.nextRecordID);
		fprintf(fo, "Number of records:       %u\n", ph.numRecords);
	}

	/*	Dump records or resources.  */

	PDBRecordEntryData **arec_data;
	arec_data = malloc(ph.numRecords * sizeof(PDBRecordEntryData*));

	for (i = 0; i < ph.numRecords; i++) {
		unsigned char *rdata;
		long rlen;

		arec_data[i] = NULL;

		if (rc->isResource) {
			PDBResourceEntry re;

			rdata = palmReadResource(rc, i, &re, &rlen);
			if (rdata != NULL) {
				char s[5]; 

				stringAlong(s, re.type);
				fprintf(fo, "\nResource %d:  Length %ld, Offset = %ld, Type \"%s\", ID = %d\n",
					i, rlen, rc->rsp[i].offset, s, re.id);

				xd(fo, rdata, rlen, 1);
				free(rdata);
			} else {
				fprintf(fo, "\nCannot load resource %d.\n", i);
			}
		} else {
			PDBRecordEntry re;

			rdata = palmReadRecord(rc, i, &re, &rlen);
			if (rdata != NULL) {
				if (!content) {
					fprintf(fo, "\nRecord %d:  Length %ld, Offset = %ld, Attributes 0x%X",
						i, rlen, rc->rcp[i].offset, re.attr);
					if (re.attr & dmRecAttrDelete) {
						fprintf(fo, " Deleted");
					}
					if (re.attr & dmRecAttrDirty) {
						fprintf(fo, " Dirty");
					}
					if (re.attr & dmRecAttrBusy) {
						fprintf(fo, " Busy");
					}
					if (re.attr & dmRecAttrSecret) {
						fprintf(fo, " Secret");
					}
					fprintf(fo, ", Category %d, UniqueID %d\n",
						re.attr & dmRecAttrCategoryMask, re.uniqueID);

					xd(fo, rdata, rlen, 1);
				}
				else {
					arec_data[i] = malloc(sizeof(PDBRecordEntryData));
					arec_data[i]->data = malloc(rlen);
					arec_data[i]->length = rlen;
					memcpy(arec_data[i]->data, rdata, rlen);
				}
				free(rdata);
			} else {
				fprintf(fo, "\nCannot load record %d.\n", i);
			}
		}
	}

	/*	Decode and/or write data.	*/

	if (content && ph.numRecords && arec_data[0] != NULL) {
		if (ph.creator == kPalmDocCreator && ph.type == kPalmDocType) {
			PalmDOCHeader *pdh = readPalmDOCHeader(arec_data[0]);
			assert(pdh->record_count == ph.numRecords - 1);
			writePalmDOCText(fo, pdh, arec_data);
			disposePalmDOCData(&pdh);
		}
		else if (ph.creator == kPalmMobiCreator && ph.type == kPalmMobiType) {
			PalmMOBIHeader *pmh = readPalmMOBIHeader(arec_data[0]);
		//	assert(pmh->pdh->record_count == ph.numRecords - 1);
			writePalmMOBIHtml(fo, pmh, arec_data);
			writePalmMOBIImages(pmh, arec_data, outdir, ph.numRecords);
			disposePalmMOBIData(&pmh);
		}
	}

	/* Dispose data record array. */
	for (i = 0; i < ph.numRecords; i++) {
		if (arec_data[i] != NULL) {
			free(arec_data[i]->data);
			free(arec_data[i]);
		}
	}
	free(arec_data);

	palmDisposeContext(rc);

	fclose(fi);
	fclose(fo);

	return 0;
}
