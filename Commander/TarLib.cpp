#include "stdafx.h"

#include "Commander.h"
#include "TarLib.h"

#include "StringUtils.h"
#include "FileSystemUtils.h"

using namespace Commander;

namespace TarLib
{
	namespace Utils
	{
		inline unsigned char octal_to_numeric( char octal )
		{
			return ( octal >= '0' && octal <= '7' ) ? octal - '0' : 0;
		}

		long long octal8_to_numeric( const char octal[8] )
		{
			long long result = 0;

			int pos = 0;
			while( octal[pos]==' ' || octal[pos] == '0' )
				++pos;

			for( int i = pos; i < 8 && octal[i] != 0 && octal[i] != ' '; ++i )
			{
				result = result * 8 + octal_to_numeric( octal[i] );
			}

			return result;
		}

		long long octal12_to_numeric( const char octal[12] )
		{
			long long result = 0;

			int pos = 0;
			while( octal[pos]==' ' || octal[pos] == '0' )
				++pos;

			for( int i = pos; i < 12 && octal[i] != 0 && octal[i] != ' '; ++i )
			{
				result = result * 8 + octal_to_numeric( octal[i] );
			}

			return result;
		}

		std::string numeric_to_octal( long long number, unsigned char length )
		{
			if( length == 0 )
				return "";

			char buffer[32] = { 0 };
			_i64toa_s( number, buffer, 32, 8 );
			int reslen = static_cast<int>( strlen( buffer ) );

			char* result = new char[length];
			memset( result, '0', length );

			strncpy_s( result + length - reslen, length, buffer, reslen );

			std::string snum( result, length );

			return snum;
		}
	}

	Header::Header():
		ownerId( 0 ),
		groupId( 0 ),
		fileSize( 0 ),
		unixTime( 0 ),
		checksum( 0 ),
		indicator( 0 ),
		deviceMajor( 0 ),
		deviceMinor( 0 ),
		tarType( EFormatType::tarFormatV7 )
	{
		memset( fileMode, 0 , sizeof( fileMode ) );
		memset( version, 0, sizeof( version ) );
	}

	long long Header::getChecksum( const HeaderAscii& asciiHeader )
	{
		long long crc = 0;
		for( int i = 0; i < 148; ++i )
			crc += asciiHeader.row[i];

		for( int i = 0; i < 8; ++i )
			crc += ' ';

		for( int i = 156; i < 500; ++i )
			crc += asciiHeader.row[i];

		return crc;
	}

	long long Header::getChecksum2( const HeaderAscii& asciiHeader )
	{
		const unsigned char *pHead = (const unsigned char*)asciiHeader.row;
		char hdrchecksum[8] = { 0 };

		memcpy( hdrchecksum, asciiHeader.header.checksum, sizeof( hdrchecksum ) );
		memset( (void*)asciiHeader.header.checksum, ' ', sizeof( asciiHeader.header.checksum ) );

		long long sum = 0;

		for( int i = 0; i < tarChunkSize; ++i )
			sum += *pHead++;

		memcpy( (void*)asciiHeader.header.checksum, hdrchecksum, sizeof( hdrchecksum ) );

		return sum;
	}

	bool Header::fromAscii( const HeaderAscii& asciiHeader, Header& header, std::wstring& errorMsg )
	{
		auto originalcrc = Utils::octal8_to_numeric( asciiHeader.header.checksum );

		if( ( originalcrc == getChecksum( asciiHeader ) ) || ( originalcrc == getChecksum2( asciiHeader ) )
			// there seem to be some tar files without checksums
		/* ||	( originalcrc == 0 && strlen( asciiHeader.header.fileName ) > 0 )*/ )
		{
			header.fileName = std::string( asciiHeader.header.fileName, 100 );
			header.fileName = header.fileName.c_str(); // trim potential zero chars
			memcpy( header.fileMode, asciiHeader.header.fileMode, 8 );
			header.ownerId = Utils::octal8_to_numeric( asciiHeader.header.ownerId );
			header.groupId = Utils::octal12_to_numeric( asciiHeader.header.groupId );
			header.fileSize = Utils::octal12_to_numeric( asciiHeader.header.fileSize );
			header.unixTime = Utils::octal12_to_numeric( asciiHeader.header.lastTime );
			header.indicator = asciiHeader.header.typeIndicator;
			header.checksum = originalcrc;
			header.linkedFileName = std::string( asciiHeader.header.linkedFileName, 100 );
			header.linkedFileName = header.linkedFileName.c_str(); // trim potential zero chars

			if( memcmp( &asciiHeader.header.magicWithVersion[0], OLDGNU_MAGIC, 8 ) == 0 )
			{
				header.tarType = EFormatType::tarFormatOldGNU;
			}
			else if( memcmp( &asciiHeader.header.magicWithVersion[0], GNU_MAGIC, 8 ) == 0 )
			{
				header.tarType = EFormatType::tarFormatGNU;
			}
			else if( memcmp( &asciiHeader.header.magicWithVersion[0], USTAR_MAGIC, 8 ) == 0 )
			{
				header.tarType = EFormatType::tarFormatUSTAR;
			}

			if( header.tarType != EFormatType::tarFormatV7 )
			{
				header.magic = std::string( &asciiHeader.header.magicWithVersion[0], &asciiHeader.header.magicWithVersion[5] );
				memcpy( &header.version[0], &asciiHeader.header.magicWithVersion[5], 2 );
				header.ownerName = std::string( asciiHeader.header.ownerName, 32 );
				header.ownerName = header.ownerName.c_str();
				header.ownerGroup = std::string( asciiHeader.header.ownerGroup, 32 );
				header.ownerGroup = header.ownerGroup.c_str();
				header.deviceMajor = Utils::octal8_to_numeric( asciiHeader.header.deviceMajor );
				header.deviceMinor = Utils::octal8_to_numeric( asciiHeader.header.deviceMinor );
				header.fileNamePrefix = std::string( asciiHeader.header.fileNamePrefix, 155 );
				header.fileNamePrefix = header.fileNamePrefix.c_str();
			}
		}
		else
		{
			errorMsg = L"CRC mismatch.";

			PrintDebug( "CRC mismatch! (%s)", StringUtils::convert2W( asciiHeader.header.fileName, CP_ACP ).c_str() );

			return false;
		}

		return true;
	}

	std::string Header::getFileName( const Header& header )
	{
		std::stringstream sstr;

		if( header.fileNamePrefix[0] )
			sstr << header.fileNamePrefix.c_str() << "\\";

		sstr << header.fileName;

		return sstr.str();
	}

	DWORD Header::getFileAttributes( const Header& header )
	{
		DWORD fileAttrib = 0;

		switch( header.indicator )
		{
		case EEntryType::tarEntryDirectory:
			fileAttrib = FILE_ATTRIBUTE_DIRECTORY;
			break;
		case EEntryType::tarEntryNormalFile:
		case EEntryType::tarEntryNormalFileNull:
			fileAttrib = FILE_ATTRIBUTE_NORMAL;
			break;
		case EEntryType::tarEntryHardLink: // TODO
			PrintDebug("hardlink");
		case EEntryType::tarEntrySymlink:
			if( header.indicator != EEntryType::tarEntryHardLink ) PrintDebug("softlink");
			fileAttrib = FILE_ATTRIBUTE_REPARSE_POINT;
			break;
		case EEntryType::tarEntryGlobalExtender: // pax extended header
		case EEntryType::tarEntryExtHeader:
			break;
		case EEntryType::tarEntryCharSpecial:
		case EEntryType::tarEntryBlockSpecial:
		case EEntryType::tarEntryFIFO:
		case EEntryType::tarEntryContiguousFile:
		//	PrintDebug("unknown: %d", header.indicator);
		default:
			fileAttrib = 0;
			break;
		}

		return fileAttrib;
	}

	// -------------- tarFile ---------------------
	File::File( const std::wstring& filename, EFileMode mode, ECompressionType compress, EFormatType type )
	{
		_tarFile = nullptr;
		_errorMsg.clear();

		open( filename, mode, compress, type );
	}

	File::~File()
	{
		close();
	}

	bool File::createTarFile( ECompressionType compress )
	{
		switch( compress )
		{
		case ECompressionType::tarNone:
			_tarFile = new CTarFileStream();
			break;
		case ECompressionType::tarGzip:
			_tarFile = new CTarFileGzip();
			break;
		case ECompressionType::tarBzip2:
			_tarFile = new CTarFileBzip2();
			break;
		case ECompressionType::tarXzip:
			_tarFile = new CTarFileXzip();
			break;
		default:
			return false;
		}

		return true;
	}

	/*static std::ios::openmode getOpenMode( EFileMode mode )
	{
		std::ios::openmode openmode;
		switch( mode )
		{
		case EFileMode::tarModeRead:
			openmode = std::ios::in | std::ios::binary;
			break;
		case EFileMode::tarModeWrite:
			openmode = std::ios::out | std::ios::binary;
			break;
		case EFileMode::tarModeAppend:
			openmode = std::ios::out | std::ios::app | std::ios::binary;
			break;
		}

		return openmode;
	}*/

	bool File::open( const std::wstring& filename, EFileMode mode, ECompressionType compress, EFormatType type )
	{
		if( isOpen() )
			return false;

		if( !createTarFile( compress ) )
			return false;

		_fileMode = mode;
		_outType = type;
		_fileName = filename;

		return _tarFile->open( PathUtils::getExtendedPath( filename ) );
	}

	bool File::extract( const std::wstring& folder )
	{
		if( _tarFile->isOpen() && _fileMode != EFileMode::tarModeWrite ) // TODO test
		{
			_tarFile->seek( 0, std::ios::beg );

			bool istar = StringUtils::endsWith( _fileName, L".tar" );
			bool istarmd5 = StringUtils::endsWith( _fileName, L".tar.md5" );

			if( !istar && !istarmd5 )
				return false;

			FsUtils::makeDirectory( folder );

			long long total = 0;
			_tarFile->seek( 0, std::ios::end );
			long long tarsize = _tarFile->tell();
			_tarFile->seek( 0, std::ios::beg );

			char block[tarChunkSize] = { 0 };
			int emptyRecord = 0;
			char zerorecord[tarChunkSize] = { 0 };
			char bigblock[tarChunkSize * 16] = { 0 };

			do 
			{
				_tarFile->read( &block[0], tarChunkSize );
				total += tarChunkSize;

				if( memcmp( block, zerorecord, tarChunkSize ) == 0 )
				{
					emptyRecord++;

					if( tarsize - total < tarChunkSize )
						break;

					continue;
				}

				HeaderAscii headerAscii;
				memcpy( &headerAscii.row[0], block, sizeof( headerAscii ) );

				Header header;
				Header::fromAscii( headerAscii, header, _errorMsg );

				switch( header.indicator )
				{
				case '0': case 0:
					{
						if( header.fileSize > 0 )
						{
							// open the file and copy the content
							std::ofstream file;
							file.open( ( folder + StringUtils::convert2W( Header::getFileName( header ) ) ).c_str(), std::ios::binary );
							if( file.is_open() )
							{
								long long filetotal = 0;

								do 
								{
									size_t toread = ( header.fileSize - filetotal ) > sizeof( bigblock ) ? sizeof( bigblock ) : (size_t)( header.fileSize - filetotal );
									if( ( toread % tarChunkSize ) != 0 )
										toread = ( 1 + ( toread / tarChunkSize ) ) * tarChunkSize;
									size_t left = (size_t)( header.fileSize - filetotal ) > toread ? toread : (size_t)( header.fileSize - filetotal );

									size_t byteread = (size_t)_tarFile->read( &bigblock[0], toread );
									total += byteread;
									filetotal += byteread;

									file.write( &bigblock[0], left );

								} while( total < tarsize && filetotal < header.fileSize );

								file.close();
							}
							else
							{
								// if file cannot be opened then position the cursor to the next tar entry header
								_tarFile->seek( (std::streamoff)header.fileSize, std::ios::cur );
							}
						}
					}
					break;
				case '5': // directory
					{
						FsUtils::makeDirectory( folder + StringUtils::convert2W( header.fileName ) );
					}
					break;
				}

			} while( total < tarsize );

			if( istarmd5 && tarsize > total + 33 )
			{
				char md5hash[32];
				_tarFile->read( &md5hash[0], 32 );

				total += 32;
				size_t left = (size_t)( tarsize - total );
				_tarFile->read( &block[0], left );

				if( block[0] == ' ' && block[1] == ' ' && block[left-1] == 0x0A )
				{
					std::string tarname = std::string( &block[2], &block[left] );
				}
			}

			_tarFile->close();

			return true;
		}

		return false;
	}

	void File::close()
	{
		_tarFile->close();
		_fileName.clear();

		delete _tarFile;
		_tarFile = nullptr;
	}

	Entry File::getFirstEntry()
	{
		if( !isOpen() )
			return Entry::makeEmpty();

		//_tarFile->seek( 0, ios::end );
		//_fileSize = _tarFile->tell();
		_nextEntryPos = (std::streampos)0;
		_headerSize = 0;

		return getNextEntry();
	}

	Entry File::getNextEntry()
	{
		if( !isOpen() )
			return Entry::makeEmpty();

		char block[tarChunkSize] = { 0 };
		char zeroRecord[tarChunkSize] = { 0 };

		_tarFile->seek( _nextEntryPos, std::ios::beg );

		long long bytesRead = _tarFile->read( &block[0], tarChunkSize );
		_headerSize += bytesRead;
		_nextEntryPos += (std::streamoff)bytesRead;

		// if we don't have 512 bytes this should be the MD5 hash and filename
		if( bytesRead < tarChunkSize )
		{
			return Entry::makeMd5( &block[0], (size_t)bytesRead );
		}

		if( memcmp( block, zeroRecord, tarChunkSize ) == 0 )
		{
			/*if( _fileSize - _headersize < tarChunkSize )
			{
				// TODO: read checksum
			}*/

			return Entry();
		}

		HeaderAscii headerAscii;
		memcpy( &headerAscii.row[0], block, sizeof( headerAscii ) );

		Header header;
		if( !Header::fromAscii( headerAscii, header, _errorMsg ) )
			return Entry::makeEmpty();

		// process additional data
		if( _paxHeader.status != 0 )
		{
			// TODO: test this and add the missing ones
			if( !_paxHeader.path.empty() )
				header.fileName = _paxHeader.path.c_str(); // trim potential zeroes at the end
			if( !_paxHeader.linkpath.empty() )
				header.linkedFileName = _paxHeader.linkpath.c_str();
			if( !_paxHeader.gname.empty() )
				header.ownerGroup = _paxHeader.gname.c_str();
			if( !_paxHeader.uname.empty() )
				header.ownerName = _paxHeader.uname.c_str();
			if( _paxHeader.size != -1 )
				header.fileSize = _paxHeader.size;
			if( _paxHeader.mtime != -1 )
				header.unixTime = _paxHeader.mtime;
			if( _paxHeader.gid != -1 )
				header.groupId = _paxHeader.gid;
			if( _paxHeader.uid != -1 )
				header.ownerId = _paxHeader.uid;

			// check whether the pax header is used globally
			if( _paxHeader.status != 2 )
				_paxHeader.clear();
		}

		// read Pax extended header
		if( header.indicator == EEntryType::tarEntryExtHeader ||
			header.indicator == EEntryType::tarEntryGlobalExtender )
		{
			readPaxExtendedHeader( block );
			_paxHeader.status = ( header.indicator == EEntryType::tarEntryExtHeader ? 1 : 2 );
			header.fileName.clear(); // filename will be restored in the next cycle
		}

		if( header.fileSize > 0 )
		{
			if( header.indicator == EEntryType::tarEntryVendorSpecL ||
				header.indicator == EEntryType::tarEntryVendorSpecK )
			{
				_paxHeader.status = 1;
				_paxHeader.path = getLongLinkFileName( header, block );
				header.fileName.clear(); // filename will be restored in the next cycle
			}

			if( header.fileSize % tarChunkSize == 0 )
				_nextEntryPos += (std::streamoff)header.fileSize;
			else
				_nextEntryPos += (std::streamoff)( tarChunkSize * ( 1 + header.fileSize / tarChunkSize ) );
		}

		return Entry( _tarFile, _fileMode, _tarFile->tell(), header );
	}

	size_t paxReadKeyword( char *buf, size_t buf_len, std::string& keyword, std::string& value )
	{
		if( buf && isdigit( buf[0] ) )
		{
			size_t length = 0;
			char *p = nullptr;

			if( ( p = (char*)memchr( buf, ' ', buf_len ) ) )
			{
				if( p - buf <= 3 ) // three digit number (must be lesser than the chunksize)
				{
					*p = '\0';
					length = (size_t)atoi( buf );

					if( length > 0 && length < buf_len )
					{
						std::string record( p + 1, length - ( p - buf ) - 1 );

						if( !record.empty() )
						{
							if( *record.rbegin() == '\n' ) // remove newline character
								record.pop_back();

							size_t off = record.find_first_of( '=' );
							if( off != std::string::npos )
							{
								keyword = record.substr( 0, off );
								value = record.substr( off + 1 );

								return length;
							}
						}
					}
				}
			}
		}

		return 0;
	}

	void File::readPaxExtendedHeader( char *block )
	{
		_paxHeader.clear();

		_tarFile->read( &block[0], tarChunkSize );

		char *p = block;
		size_t off = 0;

		std::string keyword, value;

		while( ( off = paxReadKeyword( p, (size_t)tarChunkSize - off, keyword, value ) ) != 0 )
		{
			if( keyword == "atime" )
				_paxHeader.atime = std::stoll( value );
			else if( keyword == "mtime" )
				_paxHeader.mtime = std::stoll( value );
			else if( keyword == "charset" )
				_paxHeader.charset = value;
			else if( keyword == "comment" )
				_paxHeader.comment = value;
			else if( keyword == "gid" )
				_paxHeader.gid = std::stoll( value );
			else if( keyword == "uid" )
				_paxHeader.uid = std::stoll( value );
			else if( keyword == "hdrcharset" )
				_paxHeader.hdrcharset = value;
			else if( keyword == "gname" )
				_paxHeader.gname = value;
			else if( keyword == "linkpath" )
				_paxHeader.linkpath = value;
			else if( keyword == "path" )
				_paxHeader.path = value;
			else if( keyword == "uname" )
				_paxHeader.uname = value;
			else if( keyword == "size" )
				_paxHeader.size = Utils::octal12_to_numeric( &value[0] ); // TODO: test this
			else if( keyword == "realtime" )
				(void)0; // reserved for future
			else if( keyword == "security" )
				(void)0; // reserved for future

			p += off;
		}
	}

	std::string File::getLongLinkFileName( Header& header, char *block )
	{
		std::string fileName;
		for( auto i = ( 1 + header.fileSize / tarChunkSize ); i > 0 ; --i )
		{
			_tarFile->read( &block[0], tarChunkSize );

			fileName += std::string( block,
				( i == 1 ? ( header.fileSize % tarChunkSize ) : tarChunkSize ) );
		}

		return fileName;
	}

	void File::rewind()
	{
		if( _tarFile->isOpen() )
			_tarFile->seek( 0, std::ios::beg );
	}

	// ----------------- tarFile ------------------
	Entry::Entry()
		: _empty( true )
		, _fileStream( nullptr )
		, _totalRead( 0 )
		, _lastRead( 0 )
		, _fileMode( EFileMode::tarModeRead )
	{
	}

	Entry::Entry( MD5 const& md5, EFileMode filemode )
		: _empty( true )
		, _fileStream( nullptr )
		, _totalRead( 0 )
		, _lastRead( 0 )
		, _fileMode( filemode )
	{
		this->md5 = md5;
	}

	Entry::Entry( CTarFileBase *tarfile, EFileMode filemode, std::streampos pos, Header& header )
		: _fileStream( tarfile )
		, _fileMode( filemode )
		, _entryPos( pos )
		, _empty( tarfile == nullptr )
		, header( header )
		, _totalRead( 0 )
		, _lastRead( 0 )
	{

	}


	Entry::Entry( Entry const& cpy )
	{
		this->_fileStream = cpy._fileStream;
		this->_entryPos = cpy._entryPos;
		this->_empty = cpy._empty;
		this->_totalRead = cpy._totalRead;
		this->header = cpy.header;
		this->_lastRead = cpy._lastRead;
		this->_fileMode = cpy._fileMode;
	}

	Entry& Entry::operator=( Entry const& rhv )
	{
		if( &rhv != this )
		{
			this->_fileStream = rhv._fileStream;
			this->_entryPos = rhv._entryPos;
			this->_empty = rhv._empty;
			this->_totalRead = rhv._totalRead;
			this->header = rhv.header;
			this->_lastRead = rhv._lastRead;
			this->_fileMode = rhv._fileMode;
		}

		return *this;
	}

	void Entry::rewind()
	{
		if( _fileStream != nullptr && _fileStream->isOpen() )
		{
			if( _fileStream->tell() != _entryPos )
				_fileStream->seek( _entryPos, std::ios::beg );
		}

		_totalRead = 0;
	}

	size_t Entry::read( char *buffer, size_t chunksize )
	{
		if( _fileMode == EFileMode::tarModeWrite )
			return 0;

		if( _fileStream == nullptr || buffer == nullptr || chunksize == 0 )
			return 0;

		long long left = header.fileSize - _totalRead;
		if( left == 0 )
			return 0;

		if( (size_t)left < chunksize )
			chunksize = (size_t)left;

		_lastRead = _fileStream->read( buffer, chunksize );

		_totalRead += _lastRead;

		return (size_t)_lastRead;
	}

	bool Entry::extract( const std::wstring& folder )
	{
		if( _fileMode == EFileMode::tarModeWrite )
			return false;

		switch( header.indicator )
		{
		case EEntryType::tarEntryNormalFile:
		case EEntryType::tarEntryNormalFileNull:
			return extractToFolder( folder );

		case EEntryType::tarEntryDirectory:
			return FsUtils::makeDirectory( PathUtils::combinePath( folder, StringUtils::convert2W( Header::getFileName( header ) ) ) );
		}

		return false;
	}

	bool Entry::extractToFolder( const std::wstring& folder )
	{
		if( _fileMode == EFileMode::tarModeWrite )
			return false;

		// if the entry is not a normal file this function fails
		if( header.indicator != EEntryType::tarEntryNormalFile && header.indicator != EEntryType::tarEntryNormalFileNull )
			return false;

		// create the output stream
		auto name = PathUtils::combinePath( folder, StringUtils::convert2W( Header::getFileName( header ) ) );

		FsUtils::makeDirectory( PathUtils::stripFileName( name ) );

		std::ofstream outfile( name.c_str(), std::ios::binary );

		// extract to the stream
		return extractToStream( outfile );
	}

	bool Entry::extractToFile( const std::wstring& filename )
	{
		if( _fileMode == EFileMode::tarModeWrite )
			return false;

		// if the entry is not a normal file this function fails
		if( header.indicator != EEntryType::tarEntryNormalFile && header.indicator != EEntryType::tarEntryNormalFileNull )
			return false;

		// create the output stream
		std::ofstream outfile( filename.c_str(), std::ios::binary );

		// extract to the stream
		return extractToStream( outfile );
	}

	bool Entry::extractToStream( std::ofstream &stream )
	{
		if( _fileMode == EFileMode::tarModeWrite )
			return false;

		if( stream.is_open() && ( header.indicator == EEntryType::tarEntryNormalFile || header.indicator == EEntryType::tarEntryNormalFileNull ) )
		{
			// create an output buffer
			char chunk[tarChunkSize];
			size_t readsize = 0;
			// read chunks from the tar entry until the end
			while( ( readsize = read( chunk ) ) > 0 )
			{
				// write the chunk to the output file
				stream.write( chunk, readsize );
			}

			return true;
		}

		return false;
	}

	Entry Entry::makeEmpty()
	{
		return Entry();
	}

	Entry Entry::makeMd5( char *buffer, size_t size )
	{
		if( buffer != nullptr && size > 0 )
		{
			char *start = buffer;
			char *end = start;
			while( *end++ != 0 );

			MD5 md5;
			md5.hash = std::string( start, end );
			md5.tarName = std::string( end + 1, start + size );

			return Entry( md5 );
		}

		return Entry::makeEmpty();
	}
}
