#include "stdafx.h"

#include "Commander.h"
#include "TextFileReader.h"

namespace Commander
{
	CTextFileReader::CTextFileReader( std::istream& inStream, CBackgroundWorker *pWorker, StringUtils::EUtfBom bom, UINT codePage, CTextFileReader::EOptions options )
		: _inStream( inStream )
		, _pWorker( pWorker )
		, _useEncoding( bom )
		, _useCodePage( codePage )
		, _options( options )
	{
		_fileBom = StringUtils::NOBOM;
		_fileEol = StringUtils::NONE;
		_fileSize = 0ll;
		_fileBomLen = 0;

		_bytesRead = 0ull;
		_offset = 0ll;

		_isFileText = false;
		_isFileUtf8 = false;
		_isNullChar = false;
		_isEolEnding = false;

		_eolChar = '\n';

		_lineIn.reserve( 0x4000 );
		_lineOut.reserve( 0x2000 );

		if( options != EOptions::skipAnalyze )
			analyzeFileData();
	}

	CTextFileReader::~CTextFileReader()
	{
	}

	void CTextFileReader::analyzeFileData()
	{
		if( _inStream.fail() )
			return;

		// get file size
		_inStream.seekg( 0, std::ios::end );
		_fileSize = _inStream.tellg();
		_inStream.seekg( 0, std::ios::beg );

		// read BOM and skip it over
		_fileBom = FsUtils::readByteOrderMarker( _inStream, _fileBomLen, false );

		// read data into the buffer
		_bytesRead = readDataChunk();

		// try to detect UTF 16/32 encoding from LF(or CR) characters count
		StringUtils::EUtfBom bom = analyzeCrlf();

		// when Mac's CR end of line detected
		if( _fileEol == StringUtils::CR )
			_eolChar = '\r';

		// is data UTF-8 string until lead-byte
		bool hasAnsiChar = false;
		_isFileUtf8 = isDataChunkUtf8( hasAnsiChar );

		// detect whether file is a binary file and/or contains null characters
		_isFileText = _isFileUtf8 && !_isNullChar || _fileBom != StringUtils::NOBOM || (
			( _isFileUtf8 && _isNullChar || bom != StringUtils::NOBOM ) ?
			!StringUtils::isNulls( _buffer, static_cast<int>( _bytesRead ), bom, &_isNullChar ) :
			!StringUtils::isBinary( _buffer, static_cast<int>( _bytesRead ), &_isNullChar ) );

		// when _fileBom is valid - use it and ignore everything else
		_useEncoding =
			( _fileBom != StringUtils::NOBOM ? _fileBom :
			( _useEncoding != StringUtils::NOBOM ? _useEncoding :
			( bom != StringUtils::NOBOM ? bom :
			( _isFileUtf8 && hasAnsiChar ? StringUtils::UTF8 :
			( _isFileText ? StringUtils::ANSI : _useEncoding ) ) ) ) );
	}

	bool CTextFileReader::isDataChunkUtf8( bool& hasAnsiChar )
	{
		auto offset = _bytesRead;

		// try to trim UTF-8 string until lead-byte
		if( offset == sizeof( _buffer ) )
		{
			while( offset && !StringUtils::isUtf8LeadByte( static_cast<BYTE>( _buffer[offset - 1] ) ) )
				--offset;

			if( offset ) // skip lead-byte
				offset--;
		}

		// detect UTF-8 codepoints
		return StringUtils::isUtf8( _buffer, static_cast<UINT>( offset ), &_isNullChar, &hasAnsiChar );
	}

	//
	// Try to guess end of line from LF and CR count
	//
	StringUtils::EEol CTextFileReader::guessEol( int lfCnt, int crCnt, std::streamsize len )
	{
		if( lfCnt > 0 || crCnt > 0 )
		{
			// CR only eol files are extremely rare, so make sure we don't mistake them for CRLF
			if( lfCnt == crCnt )
				return StringUtils::CRLF;
			else if( crCnt == 0 )
				return StringUtils::LF;
			else if( lfCnt == 0 )
				return StringUtils::CR;

			// for files smaller than 1024 bytes
			if( len < 0x400 )
			{
				if( lfCnt + 1 >= crCnt - 1 && lfCnt - 1 <= crCnt + 1 )
					return StringUtils::CRLF;
				else if( lfCnt + 1 >= crCnt - 1 )
					return StringUtils::LF;
				else
					return StringUtils::CR;
			}
			else
			{
				double coef = 100.0 / (double)max( crCnt, lfCnt );
				double cr = (double)crCnt * coef;
				double lf = (double)lfCnt * coef;

				return ( lf > 50.0 && cr > 50.0 ) ? StringUtils::CRLF : ( lf > 50.0 ) ? StringUtils::LF : StringUtils::CR;
			}
		}

		return StringUtils::NONE;
	}

	//
	// Simple line-feed and carriage-return chars counter
	//
	StringUtils::EUtfBom CTextFileReader::analyzeCrlf()
	{
		std::streamsize sizeTotal = _fileSize - _fileBomLen;
		std::streamsize len = _bytesRead;

		// add padding to the size of 'char32_t' so we don't lose any useful data
		if( sizeTotal == len && ( len % sizeof( char32_t ) ) != 0 )
			for( std::streamsize i = sizeof( char32_t ) - ( len % sizeof( char32_t ) ); i > 0; --i )
				_buffer[++len - 1] = ' ';

		_ASSERTE( len <= sizeof( _buffer ) );

		// count the eol characters in different encodings
		int lf32leCnt = 0, cr32leCnt = 0;
		int lf32beCnt = 0, cr32beCnt = 0;
		int lf16leCnt = 0, cr16leCnt = 0;
		int lf16beCnt = 0, cr16beCnt = 0;
		int lf8Cnt    = 0, cr8Cnt    = 0;

		char32_t *p = (char32_t*)_buffer;

		// count line-feed characters
		for( std::streamsize i = 0; i < len / sizeof( char32_t ); ++i, ++p )
		{
			if( *p == 0x0000000A ) lf32leCnt++;
			if( *p == 0x0000000D ) cr32leCnt++;
			if( *p == 0x0A000000 ) lf32beCnt++;
			if( *p == 0x0D000000 ) cr32beCnt++;
			if( LOWORD( *p ) == 0x000A ) lf16leCnt++;
			if( HIWORD( *p ) == 0x000A ) lf16leCnt++;
			if( LOWORD( *p ) == 0x000D ) cr16leCnt++;
			if( HIWORD( *p ) == 0x000D ) cr16leCnt++;
			if( LOWORD( *p ) == 0x0A00 ) lf16beCnt++;
			if( HIWORD( *p ) == 0x0A00 ) lf16beCnt++;
			if( LOWORD( *p ) == 0x0D00 ) cr16beCnt++;
			if( HIWORD( *p ) == 0x0D00 ) cr16beCnt++;
			if( LOBYTE( LOWORD( *p ) ) == 0x0A ) lf8Cnt++;
			if( HIBYTE( LOWORD( *p ) ) == 0x0A ) lf8Cnt++;
			if( LOBYTE( HIWORD( *p ) ) == 0x0A ) lf8Cnt++;
			if( HIBYTE( HIWORD( *p ) ) == 0x0A ) lf8Cnt++;
			if( LOBYTE( LOWORD( *p ) ) == 0x0D ) cr8Cnt++;
			if( HIBYTE( LOWORD( *p ) ) == 0x0D ) cr8Cnt++;
			if( LOBYTE( HIWORD( *p ) ) == 0x0D ) cr8Cnt++;
			if( HIBYTE( HIWORD( *p ) ) == 0x0D ) cr8Cnt++;
		}

		if( ( lf8Cnt || cr8Cnt ) )
		{
			const double threshold = 80.0;

			double lf32le = (double)lf32leCnt / (double)lf8Cnt * 100.0;
			double lf32be = (double)lf32beCnt / (double)lf8Cnt * 100.0;
			double lf16le = (double)lf16leCnt / (double)lf8Cnt * 100.0;
			double lf16be = (double)lf16beCnt / (double)lf8Cnt * 100.0;
			double cr32le = (double)cr32leCnt / (double)cr8Cnt * 100.0;
			double cr32be = (double)cr32beCnt / (double)cr8Cnt * 100.0;
			double cr16le = (double)cr16leCnt / (double)cr8Cnt * 100.0;
			double cr16be = (double)cr16beCnt / (double)cr8Cnt * 100.0;

		//	PrintDebug("[CR Count:%d] 32le:%lf, 32be:%lf, 16le:%lf, 16be:%lf", cr8Cnt, cr32le, cr32be, cr16le, cr16be);
		//	PrintDebug("[LF Count:%d] 32le:%lf, 32be:%lf, 16le:%lf, 16be:%lf", lf8Cnt, lf32le, lf32be, lf16le, lf16be);

			if( _fileBom == StringUtils::UTF32LE || ( ( sizeTotal % sizeof( char32_t ) == 0 ) && ( ( lf32leCnt == lf16leCnt && lf32le > threshold ) || ( cr32leCnt == cr16leCnt && cr32le > threshold ) ) ) ) {
				_fileEol = guessEol( lf32leCnt, cr32leCnt, len );
				return StringUtils::UTF32LE;
			}
			else if( _fileBom == StringUtils::UTF16LE || ( ( sizeTotal % sizeof( wchar_t ) == 0 ) && ( ( lf16le > threshold ) || ( cr16le > threshold ) ) ) ) {
				_fileEol = guessEol( lf16leCnt, cr16leCnt, len );
				return StringUtils::UTF16LE;
			}
			else if( _fileBom == StringUtils::UTF32BE || ( ( sizeTotal % sizeof( char32_t ) == 0 ) && ( ( lf32beCnt == lf16beCnt && lf32be > threshold ) || ( cr32beCnt == cr16beCnt && cr32be > threshold ) ) ) ) {
				_fileEol = guessEol( lf32beCnt, cr32beCnt, len );
				return StringUtils::UTF32BE;
			}
			else if( _fileBom == StringUtils::UTF16BE || ( ( sizeTotal % sizeof( wchar_t ) == 0 ) && ( ( lf16be > threshold ) || ( cr16be > threshold ) ) ) ) {
				_fileEol = guessEol( lf16beCnt, cr16beCnt, len );
				return StringUtils::UTF16BE;
			}
			else
				_fileEol = guessEol( lf8Cnt, cr8Cnt, len );
		}

		return StringUtils::NOBOM;
	}

	std::streamsize CTextFileReader::readDataChunk()
	{
		bool isRunning = _pWorker ? _pWorker->isRunning() : true;

		if( _inStream.good() && isRunning )
		{
			_bytesRead = _inStream.read( _buffer, sizeof( _buffer ) ).gcount();

			_offset = 0ll;

			return _bytesRead;
		}

		_bytesRead = 0ull;

		return _bytesRead;
	}

	DWORD CTextFileReader::readLine()
	{
		DWORD ret = ERROR_BAD_FORMAT;

		switch( _useEncoding )
		{
		case StringUtils::UTF32BE:
			ret = readLineUnicode<char32_t>( static_cast<char32_t>( _eolChar << 24 ), _useEncoding, _fileBom == StringUtils::NOBOM );
			break;
		case StringUtils::UTF32LE:
			ret = readLineUnicode<char32_t>( static_cast<char32_t>( _eolChar ), _useEncoding, _fileBom == StringUtils::NOBOM );
			break;
		case StringUtils::UTF16BE:
			ret = readLineUnicode<wchar_t>( static_cast<wchar_t>( _eolChar << 8 ), _useEncoding, _fileBom == StringUtils::NOBOM );
			break;
		case StringUtils::UTF16LE:
			ret = readLineUnicode<wchar_t>( static_cast<wchar_t>( _eolChar ), _useEncoding, _fileBom == StringUtils::NOBOM );
			break;
		case StringUtils::UTF8:
			ret = readLineUnicode<char>( _eolChar, _useEncoding, !_isFileText || _isNullChar );
			break;
		case StringUtils::ANSI:
			if( _options != EOptions::skipAnalyze )
				ret = readLineUnicode<char>( _eolChar, _useCodePage, !_isFileText || _isNullChar );
			else
				ret = readLineAnsi();
			break;
		case StringUtils::NOBOM:
		default:
			if( _options == EOptions::forceAnsi || _isFileText )
				ret = readLineUnicode<char>( _eolChar, _isFileUtf8 ? StringUtils::UTF8 : _useCodePage, !_isFileText || _isNullChar );
			break;
		}

		return ret;
	}

	DWORD CTextFileReader::readLineAnsi()
	{
		std::streamsize read = _bytesRead;

		char *buf = _buffer;
		char *p = nullptr;

		_lineIn.clear();

		while( _offset < read || ( read = readDataChunk() ) )
		{
			p = buf + _offset;

			if( ( p = std::find( p, buf + read, _eolChar ) ) != ( buf + read ) )
			{
				_lineIn.append( (char*)( buf + _offset ), (char*)++p ); // include EOL char

				_offset = ( p - buf );

				return ERROR_SUCCESS;
			}
			else
			{
				_lineIn.append( (char*)( buf + _offset ), (char*)( buf + read ) );
				read = 0ull;
			}
		}

		if( !_lineIn.empty() )
		{
			return ERROR_SUCCESS;
		}

		return _inStream.eof() ? ERROR_HANDLE_EOF : ERROR_SUCCESS;
	}

	template<typename T>
	DWORD CTextFileReader::readLineUnicode( T eol, UINT encoding, bool removeNull )
	{
		std::streamsize read = _bytesRead / sizeof( T );

		T *buf = (T*)_buffer;
		T *p = nullptr;

		_lineIn.clear();

		while( _offset < read || ( read = readDataChunk() / sizeof( T ) ) )
		{
			p = buf + _offset;

			if( ( p = std::find( p, buf + read, eol ) ) != ( buf + read ) )
			{
				_lineIn.append( (char*)( buf + _offset ), (char*)p++ );

				if( StringUtils::convert2Utf16( _lineIn, _lineOut, encoding ) )
				{
					if( removeNull )
						std::replace( _lineOut.begin(), _lineOut.end(), L'\0', L' ' );
						//_lineOut.erase( std::remove( _lineOut.begin(), _lineOut.end(), L'\0' ), _lineOut.end() );

					// remove preceding carriage-return character
					if( !_lineOut.empty() && *_lineOut.rbegin() == L'\r' )
						_lineOut.pop_back();
				}
				else
					return GetLastError(); // report error

				_offset = ( p - buf );

				_isEolEnding = true;

				return ERROR_SUCCESS;
			}
			else
			{
				_lineIn.append( (char*)( buf + _offset ), (char*)( buf + read ) );
				read = 0ull;
			}
		}

		if( !_lineIn.empty() )
		{
			_isEolEnding = false;

			if( StringUtils::convert2Utf16( _lineIn, _lineOut, encoding ) )
			{
				if( removeNull )
					std::replace( _lineOut.begin(), _lineOut.end(), L'\0', L' ' );
					//_lineOut.erase( std::remove( _lineOut.begin(), _lineOut.end(), L'\0' ), _lineOut.end() );

				return ERROR_SUCCESS;
			}
			else
				return GetLastError(); // report error
		}

		return _inStream.eof() ? ERROR_HANDLE_EOF : ERROR_SUCCESS;
	}
}
