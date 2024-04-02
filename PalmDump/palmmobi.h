#pragma once

/* PalmDOC compression types */
#define kPalmDocCompressionHuff  17480  /* HUFF/CDIC compression */

/* Type of BOOK and a Creator ID of MOBI */
#define kPalmMobiType    1112493899ul /* BOOK */
#define kPalmMobiCreator 1297039945ul /* MOBI */
#define kPalmMobiExth    1163416648ul /* EXTH */

/* Other constats */
#define kPalmMobiHeaderFixedLen   116

/*
The first record in the Palm Database Format gives more information about the PalmDOC file, and contains 16 bytes

bytes	content	comments
2	Compression      1 == no compression, 2 = PalmDOC compression, 17480 = HUFF/CDIC compression
2	Unused           Always zero
4	Text length	     Uncompressed length of the entire text of the book
2	Record count     Number of PDB records used for the text of the book.
2	Record size      Maximum size of each record containing text, always 4096
2   Encryption type  0 == no encryption, 1 = Old Mobipocket Encryption, 2 = Mobipocket Encryption
2   Unknown          Usually zero
*/

/*
Most Mobipocket file also have a MOBI header in record 0 that follows these
16 bytes, and newer formats also have an EXTH header following the MOBI header,
again all in record 0 of the PDB file format.

The MOBI header is of variable length and is not documented. Some fields have
been tentatively identified as follows:
*/

typedef struct PalmMobiPocketHeader {
	unsigned long identifier;           // The characters M O B I
	unsigned long header_length;        // The length of the MOBI header, including the previous 4 bytes
	unsigned long mobi_type;            // 2 Mobipocket Book, 3 PalmDoc Book, 4 Audio, 257 News, 258 News_Feed, 259 News_Magazine, 513 PICS, 514 WORD, 515 XLS, 516 PPT, 517 TEXT, 518 HTML
	unsigned long text_encoding;        // 1252 = CP1252(WinLatin1); 65001 = UTF - 8
	unsigned long unique_id;            // Some kind of unique ID number (random?)
	unsigned long generator_version;    // Potentially the version of the Mobipocket-generation tool. Always >= the value of the "format version" field and <= the version of mobigen used to produce the file.
	unsigned long reserved1;            // All 0xFF. In case of a dictionary, or some newer file formats, a few bytes are used from this range of 40 0xFFs
	unsigned char reserved2[36];        // All 0xFF. In case of a dictionary, or some newer file formats, a few bytes are used from this range of 40 0xFFs
	unsigned long first_non_book_index; // First record number(starting with 0) that's not the book's text
	unsigned long full_name_offset;     // Offset in record 0 (not from start of file) of the full name of the book
	unsigned long full_name_length;     // Length in bytes of the full name of the book
	unsigned long language;             // Book language code.Low byte is main language 09 = English, next byte is dialect, 08 = British, 04 = US
	unsigned long input_language;       // Input language for a dictionary
	unsigned long output_language;      // Output language for a dictionary
	unsigned long format_version;       // Potentially the version of the Mobipocket format used in this file. Always >= 1 and <= the value of the "generator version" field.
	unsigned long first_image_record;   // First record number(starting with 0) that contains an image.Image records should be sequential.If there are no images this will be 0xffffffff.
	unsigned long huff_record;          // Record containing Huff information used in HUFF / CDIC decompression.
	unsigned long huff_count;           // Number of Huff records.
	unsigned long datp_record;          // Unknown : Records starts with DATP.
	unsigned long datp_count;           // Number of DATP records.
	unsigned long exth_flags;           // Bitfield. if bit 6, 0x40 is set, then there's an EXTH record The following records are only present if the mobi header is long enough.
	/*
	The following records are only present if the mobi header is long enough.
	*/
	unsigned char unknown1[32];         // 32 unknown bytes, if MOBI is long enough
	unsigned long unknown2;             // ?
	unsigned long drm_offset;           // Offset to DRM key info in DRMed files. 0xFFFFFFFF if no DRM
	unsigned long drm_count;            // Number of entries in DRM info.
	unsigned long drm_size;             // Number of bytes in DRM info.
	unsigned long drm_flags;            // Some flags concerning the DRM info.
	unsigned char unknown3[6];          // ?
	unsigned short last_image_record;   // Possible vaule with the last image record.If there are no images in the book this will be 0xffff.
	unsigned long unknown4;             // ?
	unsigned long fcis_record;          // Unknown. Record starts with FCIS.
	unsigned long unknown5;             // ?
	unsigned long flis_record;          // Unknown. Records starts with FLIS.
	unsigned char unknown6[36];         // Bytes to the end of the MOBI header, including the following if the header length >= 228. (244 from start of record)
	unsigned short extra_data_flags;    // A set of binary flags, some of which indicate extra data at the end of each text block. This only seems to be valid for Mobipocket format version 5 and 6 (and higher?), when the header length is 228 (0xE4) or 232 (0xE8).
} PalmMobiPocketHeader;


/*
EXTH Header
-----------

If the MOBI header indicates that there's an EXTH header, it follows immediately
after the MOBI header. since the MOBI header is of variable length, this isn't
at any fixed offset in record 0. Note that some readers will ignore any EXTH
header info if the mobipocket version number specified in the MOBI header is 2
or less (perhaps 3 or less).
*/

/*
The EXTH header is also undocumented, so some of this is guesswork.

bytes   content             comments
4       identifier          the characters E X T H
4       header length       the length of the EXTH header, including the previous 4 bytes
4       record Count        The number of records in the EXTH header. the rest of the EXTH header consists of repeated EXTH records to the end of the EXTH length.
        EXTH record start   Repeat until done.
4       record type         Exth Record type. Just a number identifying what's stored in the record
4       record length       length of EXTH record = L , including the 8 bytes in the type and length fields
L-8     record data         Data.
        EXTH record end     Repeat until done.

There are lots of different EXTH Records types. Ones found so far in Mobipocket
files are listed here, with possible meanings. Hopefully the table will be
filled in as more information comes to light.

record type    usual length     name             comments
1                               drm_server_id
2                               drm_commerce_id
3                               drm_ebookbase_book_id
100                             author
101                             publisher
102                             imprint
103                             description
104                             isbn
105                             subject
106                             publishingdate
107                             review
108                             contributor
109                             rights
110                             subjectcode
111                             type
112                             source
113                             asin
114                             versionnumber
115                             sample
116                             startreading
117            3                adult            Mobipocket Creator adds this if Adult only is checked; contents: "yes" 
118                             retail price     As text, e.g. "4.99" 
119                             retail price currency     As text, e.g. "USD" 
201            4                coveroffset      Add to first image field in Mobi Header to find PDB record containing the cover image 
202            4                thumboffset      Add to first image field in Mobi Header to find PDB record containing the thumbnail cover image 
203                             hasfakecover
204            4                Creator Software Records 204-207 are usually the same for all books from a certain source, e.g. 1-6-2-41 for Baen and 201-1-0-85 for project gutenberg, 200-1-0-85 for amazon when converted to a 32 bit integer. 
205            4                Creator Major Version
206            4                Creator Minor Version
207            4                Creator Build Number
208                             watermark
209                             tamper proof keys Used by the Kindle (and Android app) for generating book-specific PIDs. 
300                             fontsignature
401            1                clippinglimit
402                             publisherlimit
403                             403 Unknown      1 - Text to Speech disabled; 0 - Text to Speech enabled
404            1                404 ttsflag
501            4                cdetype          PDOC - Personal Doc;
                                                 EBOK - ebook;
502                             lastupdatetime
503                             updatedtitle
*/

typedef struct PalmMobiExthHeader {
	unsigned long identifier;     // The characters E X T H
	unsigned long header_length;  // The length of the EXTH header, including the previous 4 bytes
	unsigned long record_count;   // The number of records in the EXTH header.the rest of the EXTH header consists of repeated EXTH records to the end of the EXTH length.
	// EXTH records start
	unsigned long *record_type;   // Exth Record type. Just a number identifying what's stored in the record
	unsigned long *record_length; // Length of EXTH record = L, including the 8 bytes in the type and length fields
	unsigned char **record_data;  // Data.
	// EXTH records end
} PalmMobiExthHeader;

/*
And now, at the end of Record 0 of the PDB file format, we usually get the full
file name, the offset of which is given in the MOBI header.
*/

typedef struct PalmMOBIHeader {
	PalmDOCHeader        *pdh;
	PalmMobiPocketHeader *pmh;
	PalmMobiExthHeader   *pxh;
} PalmMOBIHeader;

extern PalmMOBIHeader *readPalmMOBIHeader(const PDBRecordEntryData *rec);
extern void writePalmMOBIHtml(FILE *f, const PalmMOBIHeader *pmh, PDBRecordEntryData **arec);
extern void writePalmMOBIImages(const PalmMOBIHeader *pmh, const PDBRecordEntryData **arec, const wchar_t *outdir, unsigned short num_records);
extern void disposePalmMOBIData(PalmMOBIHeader **pmh);
