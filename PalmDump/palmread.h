#pragma once

/*  Palm file input context structure.  */

struct palmreadContext {
    FILE *fi;                         /* PDB input file */
    unsigned short nrecords;          /* Number of records */
    int isResource;                   /* Nonzero if this is a resource file */
    struct PDBResourceEntry *rsp;     /* Resource entry array */
    struct PDBRecordEntry *rcp;       /* Record entry array */
    long fileLength;                  /* Total length of file */
    long *lengths;                    /* Computed lengths of records */

    /* The following are not required for the functions in
       palmread.c proper but are set by them so that the
       companion byte-order-independent record cracker
       functions in palmrec.c can parse items in a
       just-returned record. */

    unsigned char *lastrec;           /* Last record or resource read */
    long lastlength;                  /* Length of last record or resource */
    long recptr;                      /* Pointer into record */
};

/*  Function prototypes.  */

extern struct palmreadContext *palmReadHeader(FILE *fp, PDBHeader *ph);
extern void *palmReadResource(struct palmreadContext *p, int resno, PDBResourceEntry *re, long *rlen); 
extern void *palmReadRecord(struct palmreadContext *p, int recno, PDBRecordEntry *re, long *rlen);
extern void palmDisposeContext(struct palmreadContext *p);
