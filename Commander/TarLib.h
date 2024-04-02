#pragma once

#include <string>
#include <fstream>

#include "TarFileBase.h"
#include "TarFileStream.h"
#include "TarFileGzip.h"
#include "TarFileBzip2.h"
#include "TarFileXzip.h"

#define OLDGNU_MAGIC    "ustar\x20\x20"
#define USTAR_MAGIC     "ustar\x00\x30\x30"
#define GNU_MAGIC       "GNUtar\x20"

#define TSUID    04000     // set UID on execution
#define TSGID    02000     // set GID on execution
#define TSVTX    01000     // reserved
#define TUREAD   00400     // read by owner
#define TUWRITE  00200     // write by owner
#define TUEXEC   00100     // execute/search by owner
#define TGREAD   00040     // read by group
#define TGWRITE  00020     // write by group
#define TGEXEC   00010     // execute/search by group
#define TOREAD   00004     // read by other
#define TOWRITE  00002     // write by other
#define TOEXEC   00001     // execute/search by other

namespace TarLib
{
	const int tarChunkSize = 512;

	enum EFileMode {
		tarModeRead,
		tarModeWrite,
		tarModeAppend
	};

	enum ECompressionType {
		tarNone,
		tarGzip,
		tarBzip2,
		tarXzip
	};

	enum EFormatType {
		tarFormatV7,
		tarFormatOldGNU,
		tarFormatGNU,
		tarFormatUSTAR,
		tarFormatPOSIX
	};

	enum EEntryType {
		tarEntryNormalFile = '0',
		tarEntryNormalFileNull = 0,
		tarEntryHardLink = '1',
		tarEntrySymlink = '2',
		tarEntryCharSpecial = '3',
		tarEntryBlockSpecial = '4',
		tarEntryDirectory = '5',
		tarEntryFIFO = '6',
		tarEntryContiguousFile = '7',
		tarEntryGlobalExtender = 'g', // "././@PaxHeader" - affects all the files
		tarEntryExtHeader = 'x',      // "././@PaxHeader" - affects the following file
		tarEntryVendorSpecA = 'A',
		tarEntryVendorSpecB = 'B',
		tarEntryVendorSpecC = 'C',
		tarEntryVendorSpecD = 'D',
		tarEntryVendorSpecE = 'E',
		tarEntryVendorSpecF = 'F',
		tarEntryVendorSpecG = 'G',
		tarEntryVendorSpecH = 'H',
		tarEntryVendorSpecI = 'I',
		tarEntryVendorSpecJ = 'J',
		tarEntryVendorSpecK = 'K', // GNU Long link name - "././@LongLink"
		tarEntryVendorSpecL = 'L', // GNU Long file name - "././@LongLink"
		tarEntryVendorSpecM = 'M',
		tarEntryVendorSpecN = 'N',
		tarEntryVendorSpecO = 'O',
		tarEntryVendorSpecP = 'P',
		tarEntryVendorSpecQ = 'Q',
		tarEntryVendorSpecR = 'R',
		tarEntryVendorSpecS = 'S', // GNU sparse file
		tarEntryVendorSpecT = 'T',
		tarEntryVendorSpecV = 'U',
		tarEntryVendorSpecU = 'V', // GNU volume header
		tarEntryVendorSpecX = 'X',
		tarEntryVendorSpecY = 'Y',
		tarEntryVendorSpecZ = 'Z',
	};

	class File;

	/*
	-------+------+------------------------------------------------------
	Offset | Size | Field
	-------+------+------------------------------------------------------
	0   	100	   File name
	100	   8	      File mode
	108	   8	      Owner's numeric user ID
	116	   8	      Group's numeric user ID
	124	   12	      File size in bytes
	136	   12	      Last modification time in numeric Unix time format
	148	   8	      Checksum for header block
	156	   1	      Link indicator (file type)
	157	   100	   Name of linked file

	257	   6	      UStar indicator "ustar"
	263	   2	      UStar version "00"
	265	   32	      Owner user name
	297	   32	      Owner group name
	329	   8	      Device major number
	337	   8	      Device minor number
	345	   155	   Filename prefix
	-------+------+------------------------------------------------------
	*/

	// this union represent the header of a tar entry
	// it is a chunk of 512 bytes with all data encoded in ASCII
	// and all numbers in base 8
	union HeaderAscii
	{
		char row[tarChunkSize];
		struct 
		{
			char fileName[100];           // File name
			char fileMode[8];             // File mode
			char ownerId[8];              // Owner's numeric user ID
			char groupId[8];              // Group's numeric user ID
			char fileSize[12];            // File size in bytes
			char lastTime[12];            // Last modification time in numeric Unix time format
			char checksum[8];             // Checksum for header block
			char typeIndicator;           // Link indicator (file type)
			char linkedFileName[100];     // Name of linked file

			// ignored if the chars at 257 are not "ustar"
			char magicWithVersion[8];     // UStar indicator and version
			char ownerName[32];           // Owner user name
			char ownerGroup[32];          // Owner group name
			char deviceMajor[8];          // Device major number
			char deviceMinor[8];          // Device minor number
			char fileNamePrefix[155];     // Filename prefix
			char padding[12];             // padding for end of 512 bytes chunk
		} header;
	};

	// this represents a transformed header for a TAR entry
	struct Header
	{
		std::string fileName;
		char fileMode[8];
		long long ownerId;
		long long groupId;
		long long fileSize;
		long long unixTime;
		unsigned long long checksum;
		char indicator;
		std::string linkedFileName;

		// ignored if the chars at 257 are not "ustar"
		std::string magic;
		char version[2];
		std::string ownerName;
		std::string ownerGroup;
		long long deviceMajor;
		long long deviceMinor;
		std::string fileNamePrefix;

		// helper members
		EFormatType tarType;

		static bool fromAscii( const HeaderAscii& asciiHeader, Header& header, std::wstring& errorMsg );
		static long long getChecksum( const HeaderAscii& asciiHeader );
		static long long getChecksum2( const HeaderAscii& asciiHeader );
		static std::string getFileName( const Header& header );
		static DWORD getFileAttributes( const Header& header );

		Header();
	};

	// this represents an extended Pax header for a TAR entry (used also for LongLink names)
	struct PaxExtendedHeader
	{
		PaxExtendedHeader() { clear(); }

		int status; // 0 - not used, 1 - used only once, 2 - used for all the files

		long long atime; // the file access time for the following file(s), equivalent to the value of the st_atime
		long long mtime; // the file modification time of the following file(s), equivalent to the value of the st_mtime
		std::string charset; // the name of the character set used to encode the data in the following file(s)
		std::string comment; // a series of characters used as a comment
		long long gid;       // the group ID of the group that owns the file, expressed as a decimal number
		long long uid;       // the user ID of the file owner, expressed as a decimal number
		std::string hdrcharset; // character set used to encode: gname, linkpath, path, uname
		std::string gname;    // the group of the file(s), formatted as a group name in the group database
		std::string linkpath; // the pathname of a link being created to another file, of any type, previously archived
		std::string path;     // the pathname of the following file(s). This record shall override the name and prefix
		std::string uname;    // the owner of the following file(s), formatted as a user name in the user database
		long long size;       // the size of the file in octets, expressed as a decimal number
		std::string realtime; // reserved for future
		std::string security; // reserved for future

		// clears all members
		inline void clear() {
			status = 0; atime = mtime = -1; gid = uid = -1; size = -1;
			charset.clear(); comment.clear(); hdrcharset.clear(); gname.clear();
			linkpath.clear(); path.clear(); uname.clear(); realtime.clear(); security.clear();
		}
	};

	// this structure represent an entry in the TAR file that contains an MD5 hash and the name of the archive
	struct MD5
	{
		std::string hash;
		std::string tarName;

		bool isNull() const { return hash.empty() && tarName.empty(); }
	};

	// this is an entry in the TAR file
	// it contains the header for the entry and methods to process the entry
	class Entry
	{
		bool _empty;
		EFileMode _fileMode;
		CTarFileBase *_fileStream;
		std::streampos _entryPos;
		long long _totalRead;
		long long _lastRead;

		// constructors
		Entry( CTarFileBase *tarfile, EFileMode filemode, std::streampos pos, Header& header );

		// tarFile must be friend to be able to construct an entry from a stream
		friend class File;

	public:
		// these constructors create entries used for extracting
		Entry();
		Entry( MD5 const & md5, EFileMode filemode = tarModeRead );
		Entry( Entry const & cpy );
		Entry& operator=( Entry const & rhv );

		// the entry header
		Header header;

		// md5 entry
		MD5 md5;

		// public interface
		bool isEmpty() const { return _empty; }
		bool isMd5() const { return !md5.isNull(); }
		long long sizeLeft() const { return header.fileSize - _totalRead; }

		size_t read( char *buffer, size_t chunksize = tarChunkSize );
		void rewind();

		bool extract( const std::wstring& folder );
		bool extractToFolder( const std::wstring& folder );
		bool extractToFile( const std::wstring& filename );
		bool extractToStream( std::ofstream& stream );

		// static methods
		static Entry makeEmpty();
		static Entry makeMd5( char* buffer, size_t size );
	};

	// this is the representation of a tar file
	class File
	{
		CTarFileBase *_tarFile;
		std::wstring _fileName;       // name of the archive file
		std::wstring _errorMsg;       // reported error message
		EFormatType _outType;         // type of the archive; only used when writing archives
		EFileMode _fileMode;          // the mode the archive is opened
		std::streampos _nextEntryPos; // position in the stream of the next entry
		//long long _fileSize;          // total size of the archive stream
		long long _headerSize;        // size of bytes of the current read header
		PaxExtendedHeader _paxHeader; // extended header for long filenames, etc.

		bool createTarFile( ECompressionType compress );
		void close();

		std::string getLongLinkFileName( Header& header, char *block );
		void readPaxExtendedHeader( char *block );

		File( const File& cpy ) = default;
		File& operator=( const File& rhv ) = default;

	public:
		File( const std::wstring& filename, EFileMode mode, ECompressionType compress, EFormatType type = tarFormatUSTAR );
		~File();

		bool open( const std::wstring& filename, EFileMode mode, ECompressionType compress, EFormatType type = tarFormatUSTAR );
		inline bool isOpen() { return _tarFile && _tarFile->isOpen(); }
		bool extract( const std::wstring& folder );
		void rewind();

		Entry getFirstEntry();
		Entry getNextEntry();

		inline std::wstring getErrorMessage() { return _errorMsg; }
	};
}
