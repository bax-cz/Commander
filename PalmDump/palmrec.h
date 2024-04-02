#pragma once

/*  Function prototypes.  */

extern void palmRecParse(struct palmreadContext *p, void *buf, long bufl);
extern unsigned char palmRecByte(struct palmreadContext *p);
extern unsigned short palmRecShort(struct palmreadContext *p);
extern unsigned long palmRecLong(struct palmreadContext *p);
extern void palmRecBytes(struct palmreadContext *p, void *buf, int bufl);
extern void palmRecString(struct palmreadContext *p, char *buf, int bufl);
