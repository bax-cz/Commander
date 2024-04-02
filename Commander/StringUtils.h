#pragma once

/*
	String utils - helper functions for string conversions
*/

#include <algorithm>
#include <sstream>
#include <cwctype>
#include <iomanip>
#include <locale>

namespace Commander
{
	struct StringUtils
	{
		//
		// Unicode byte-order-mark definitions
		//
		enum EUtfBom : WORD { ANSI = CP_ACP, UTF16LE = 1200, UTF16BE = 1201, UTF32LE = 12000, UTF32BE = 12001, UTF8 = CP_UTF8, NOBOM = 65535 };

		//
		// End of line character definitions
		//
		enum EEol : DWORD { CRLF = 0, LF = 0x0A, CR = 0x0D, NONE = 65535 };

		//
		// Convert enconding name to string
		//
		static std::wstring encodingToStr( EUtfBom bom )
		{
			std::wstring strOut;

			switch( bom )
			{
			case StringUtils::ANSI:
				strOut = L"ANSI";
				break;
			case StringUtils::UTF8:
				strOut = L"UTF-8";
				break;
			case StringUtils::UTF16LE:
				strOut = L"UTF-16LE";
				break;
			case StringUtils::UTF16BE:
				strOut = L"UTF-16BE";
				break;
			case StringUtils::UTF32LE:
				strOut = L"UTF-32LE";
				break;
			case StringUtils::UTF32BE:
				strOut = L"UTF-32BE";
				break;
			case StringUtils::NOBOM:
			default:
				strOut = L"Invalid";
				break;
			}

			return strOut;
		}

		//
		// Convert End-of-line character name to string
		//
		static std::wstring eolToStr( EEol eol )
		{
			std::wstring strOut;

			switch( eol )
			{
			case StringUtils::CRLF:
				strOut = L"CRLF";
				break;
			case StringUtils::LF:
				strOut = L"LF";
				break;
			case StringUtils::CR:
				strOut = L"CR";
				break;
			case StringUtils::NONE:
				strOut = L"-";
				break;
			}

			return strOut;
		}

		//
		// Convert string to wstring (wide string)
		//
		static std::wstring convert2W( const std::string& str, UINT codepage = CP_UTF8 )
		{
			int len = MultiByteToWideChar( codepage, 0, &str[0], static_cast<int>( str.length() ), NULL, 0 );

			if( len == 0 ) // return empty string on error
				return L"";

			std::wstring strOut; strOut.resize( len );

			MultiByteToWideChar( codepage, 0, &str[0], static_cast<int>( str.length() ), &strOut[0], len );

			return strOut;
		}

		//
		// Convert char array to wstring (wide string)
		//
		static bool convert2W( const char *str, size_t str_len, std::wstring& strOut, UINT codepage = CP_UTF8 )
		{
			int len = MultiByteToWideChar( codepage, 0, str, static_cast<int>( str_len ), NULL, 0 );

			if( len == 0 )
				return false;

			strOut.resize( len );

			int outLen = MultiByteToWideChar( codepage, 0, str, static_cast<int>( str_len ), &strOut[0], len );

			return len == outLen;
		}

		//
		// Convert wstring to string (ascii string)
		//
		static std::string convert2A( const std::wstring& str, UINT codepage = CP_UTF8 )
		{
			int len = WideCharToMultiByte( codepage, 0, &str[0], static_cast<int>( str.length() ), NULL, 0, NULL, NULL );

			if( len == 0 ) // return empty string on error
				return "";

			std::string strOut; strOut.resize( len );

			WideCharToMultiByte( codepage, 0, &str[0], static_cast<int>( str.length() ), &strOut[0], len, NULL, NULL );

			return strOut;
		}

		//
		// Convert wchar_t array to string (ascii string)
		//
		static bool convert2A( const wchar_t *str, size_t str_len, std::string& strOut, UINT codepage = CP_UTF8 )
		{
			int len = WideCharToMultiByte( codepage, 0, str, static_cast<int>( str_len ), NULL, 0, NULL, NULL );

			if( len == 0 )
				return false;

			strOut.resize( len );

			int outLen = WideCharToMultiByte( codepage, 0, str, static_cast<int>( str_len ), &strOut[0], len, NULL, NULL );

			return len == outLen;
		}

		//
		// Convert wstring to string (utf-8 encoded ascii string)
		//
		static std::string convert2Utf8( const std::wstring& str )
		{
			//std::string strOut = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes( str );

			return convert2A( str );
		}

		//
		// Convert string to wstring (including UTF-16LE(BE) and UTF-32LE(BE))
		//
		static bool convert2Utf16( const std::string& str, std::wstring& strOut, UINT codepage )
		{
			auto outLen = mb2Utf16( codepage, &str[0], static_cast<int>( str.length() ), NULL, 0 );

			if( outLen != -1 )
			{
				strOut.resize( outLen );
				mb2Utf16( codepage, &str[0], static_cast<int>( str.length() ), &strOut[0], outLen );

				return true;
			}

			return false;
		}

		//
		// Convert hexadecimal string to data
		//
		static bool convertHex( const std::wstring& str, std::string& strOut, UINT codepage = CP_ACP )
		{
			strOut.clear();

			try {
				auto bytesArray = split( str, L" " );

				for( const auto& byte : bytesArray )
				{
					if( byte.length() > 2 )
						return false;

					if( !byte.empty() )
						strOut.append( 1, static_cast<char>(
							std::stol( convert2A( byte, codepage ), nullptr, 16 ) ) );
				}
			}
			catch( std::invalid_argument )
			{
				return false;
			}
			catch( std::out_of_range )
			{
				return false;
			}

			return true;
		}


		//
		// Convert char to lower case - ANSI version
		//
		static char convertChar2Lwr( char ch )
		{
			return (char)LOWORD( CharLowerA( (LPSTR)(WORD)ch ) );
		}

		//
		// Convert char to upper case - ANSI version
		//
		static char convertChar2Upr( char ch )
		{
			return (char)LOWORD( CharUpperA( (LPSTR)(WORD)ch ) );
		}

		//
		// Convert wchar_t to lower case
		//
		static wint_t convertChar2Lwr( wchar_t ch )
		{
			return LOWORD( CharLowerW( (LPWSTR)(WORD)ch ) );
		}

		//
		// Convert wchar_t to upper case
		//
		static wint_t convertChar2Upr( wchar_t ch )
		{
			return LOWORD( CharUpperW( (LPWSTR)(WORD)ch ) );
		}

		//
		// Is character alpha-numeric - ANSI version
		//
		static bool isAlphaNum( char ch )
		{
			return  ch == '_' || IsCharAlphaNumericA( ch );
		}

		//
		// Is character alpha-numeric - wide version
		//
		static bool isAlphaNum( wchar_t ch )
		{
			return ch == L'_' || IsCharAlphaNumericW( ch );
		}

		//
		// Convert string to lower case
		//
		static std::string convert2Lwr( const std::string& str )
		{
			std::string strOut = str;
			convert2LwrInPlace( strOut );

			return strOut;
		}

		//
		// Convert string to upper case
		//
		static std::string convert2Upr( const std::string& str )
		{
			std::string strOut = str;
			convert2UprInPlace( strOut );

			return strOut;
		}

		//
		// Convert wstring to lower case
		//
		static std::wstring convert2Lwr( const std::wstring& str )
		{
			std::wstring strOut = str;
			convert2LwrInPlace( strOut );

			return strOut;
		}

		//
		// Convert wstring to upper case
		//
		static std::wstring convert2Upr( const std::wstring& str )
		{
			std::wstring strOut = str;
			convert2UprInPlace( strOut );

			return strOut;
		}

		//
		// Convert string to lower case - in place
		// MSDN: For Turkish or Azerbaijani, call linguistically sensitive LCMapString
		static void convert2LwrInPlace( std::string& str )
		{
			CharLowerBuffA( &str[0], static_cast<DWORD>( str.length() ) );
		}

		//
		// Convert string to upper case - in place
		//
		static void convert2UprInPlace( std::string& str )
		{
			CharUpperBuffA( &str[0], static_cast<DWORD>( str.length() ) );
		}

		//
		// Convert wstring to lower case - in place
		//
		static void convert2LwrInPlace( std::wstring& str )
		{
			CharLowerBuffW( &str[0], static_cast<DWORD>( str.length() ) );
		}

		//
		// Convert wstring to upper case - in place
		//
		static void convert2UprInPlace( std::wstring& str )
		{
			CharUpperBuffW( &str[0], static_cast<DWORD>( str.length() ) );
		}

		//
		// Convert wstring to mixed case (first letter of each word upper case) - in place
		//
		static void convert2MixInPlace( std::wstring& str )
		{
			int prev = L' ';
			std::transform( str.begin(), str.end(), str.begin(), [&prev]( wchar_t ch ) {
				if( ( prev == L' ' || prev == L'.' ) && ( ch != L' ' && ch != L'.' ) && ::iswalpha( ch ) )
					return ( prev = convertChar2Upr( ch ) );
				else
					return ( prev = convertChar2Lwr( ch ) );
			} );
		}

		//
		// Split wstring to vector of wstrings by separator
		//
		static std::vector<std::wstring> split( const std::wstring& str, const std::wstring& separator = L"|" )
		{
			std::vector<std::wstring> stringsOut;
			std::size_t start = 0, end = 0;

			while( ( end = str.find( separator, start ) ) != std::wstring::npos )
			{
				stringsOut.push_back( str.substr( start, end - start ) );
				start = end + separator.length();
			}

			stringsOut.push_back( str.substr( start ) );

			return stringsOut;
		}

		//
		// Concatenate wstring elements from vector of wstrings delimited by separator
		//
		static std::wstring join( const std::vector<std::wstring>& strElems, const std::wstring& separator = L"|" )
		{
			std::wstring strOut;

			for( auto it = strElems.begin(); it != strElems.end(); ++it )
			{
				if( it != strElems.begin() )
					strOut += separator;

				strOut += *it;
			}

			return strOut;
		}

		//
		// Try to find the 'text' in the 'str' string ignoring case
		// NOTE: This function relies on 'text' to be an uppercase wstring
		static size_t findCaseInsensitive( const std::wstring& str, const std::wstring& text, size_t offset = 0 )
		{
			auto it = std::search( str.begin() + offset, str.end(), text.begin(), text.end(),
				[](wchar_t ch1, wchar_t ch2) {
				return convertChar2Upr( ch1 ) == ch2;
			} );

			return ( it != str.end() ? ( it - str.begin() ) : std::string::npos );
		}

		//
		// Replace all occurrences of wstring in source wstring - in place
		//
		static void replaceAll( std::wstring& str, const std::wstring& from, const std::wstring& to )
		{
			std::wstring strOut;
			strOut.reserve( str.length() ); // avoids a few memory allocations

			std::wstring::size_type lastPos = 0;
			std::wstring::size_type findPos;

			while( ( findPos = str.find( from, lastPos ) ) != std::wstring::npos )
			{
				strOut.append( str, lastPos, findPos - lastPos );
				strOut += to;
				lastPos = findPos + from.length();
			}

			// take care for the rest after last occurrence
			str.swap( strOut + str.substr( lastPos ) );
		}

		//
		// Remove all occurrences of wchar characters in source wstring
		//
		static std::wstring removeAll( const std::wstring& str, wchar_t ch )
		{
			std::wstring strOut = str;

			strOut.erase( std::remove( strOut.begin(), strOut.end(), ch ), strOut.end() );

			return strOut;
		}

		//
		// Check whether string starts with other string
		//
		static bool startsWith( const std::wstring& str, const std::wstring& beginning )
		{
			if( str.length() >= beginning.length() )
			{
				auto strLwr = convert2Lwr( str );
				auto begLwr = convert2Lwr( beginning );

				return std::equal( begLwr.begin(), begLwr.end(), strLwr.begin() );
			}
			else
				return false;
		}

		//
		// Check whether string ends with other string
		//
		static bool endsWith( const std::wstring& str, const std::wstring& ending )
		{
			if( str.length() >= ending.length() )
			{
				auto strLwr = convert2Lwr( str );
				auto endLwr = convert2Lwr( ending );

				return std::equal( endLwr.rbegin(), endLwr.rend(), strLwr.rbegin() );
			}
			else
				return false;
		}

		//
		// Trim leading white chars (in place)
		//
		static void lTrim( std::wstring& str )
		{
			str.erase( str.begin(), std::find_if( str.begin(), str.end(), []( int ch ) {
				return !std::iswspace( ch );
			} ) );
		}

		//
		// Trim trailing white chars (in place)
		//
		static void rTrim( std::wstring& str )
		{
			str.erase( std::find_if( str.rbegin(), str.rend(), []( int ch ) {
				return !std::iswspace( ch );
			} ).base(), str.end() );
		}

		//
		// Trim from both ends (in place)
		//
		static void trim( std::wstring& str )
		{
			lTrim( str );
			rTrim( str );
		}

		//
		// Cut (from source string) until character and trim resulting string (in place)
		//
		static std::wstring cutToChar( std::wstring& str, wchar_t ch, bool trim = true )
		{
			std::wstring strOut;

			auto pos = str.find_first_of( ch );
			if( pos != std::wstring::npos )
			{
				strOut = str.substr( 0, pos );
				str.erase( 0, pos + 1 );
			}
			else
			{
				strOut = str;
				str.clear();
			}

			if( trim )
			{
				rTrim( strOut );
				lTrim( str );
			}

			return strOut;
		}

		//
		// Read strings delimited by zeroes (values stored in windows registry)
		//
		static std::wstring getValueMulti( const WCHAR *str, size_t length )
		{
			std::wostringstream sstr;
			size_t len = 0;

			while( length > 1 )
			{
				sstr << str << L"\r\n";
				len = wcslen( str ) + 1;
				str = str + len;
				length -= len;
			}

			return sstr.str();
		}

		//
		// Trim comments from text after ';' character (in place)
		//
		static void trimComments( std::wstring& str, wchar_t keyChar = L';' )
		{
			size_t idx = str.find_first_of( keyChar );

			if( idx != std::wstring::npos )
				str.erase( str.begin() + idx, str.end() );

			trim( str );
		}

		//
		// Format time from seconds to HH:MM:SS
		//
		static std::wstring formatTime( const ULONGLONG& lengthInSeconds )
		{
			std::wostringstream sstr;

			auto hours = ( lengthInSeconds / 3600 );

			if( hours > 0 )
				sstr << hours << L":";

			sstr << std::setfill( L'0' );
			sstr << std::setw( 2 ) << ( lengthInSeconds / 60 ) % 60 << L":";
			sstr << std::setw( 2 ) << lengthInSeconds % 60;

			return sstr.str();
		}

		//
		// Format time from milliseconds to [H hours M minutes S seconds MS milliseconds]
		//
		static std::wstring formatTime( DWORD ticks )
		{
			std::wostringstream sstr;

			if( ticks == 0 )
				sstr << L"no time";

			DWORD milliseconds = ticks % 1000;

			ticks /= 1000;
			DWORD seconds = ticks % 60;

			ticks /= 60;
			DWORD minutes = ticks % 60;

			ticks /= 60;
			DWORD hours = ticks;

			if( hours )
				sstr << hours << L" hour" << ( hours == 1 ? L" " : L"s " );
			if( minutes )
				sstr << minutes << L" minute" << ( minutes == 1 ? L" " : L"s " );
			if( seconds )
				sstr << seconds << L" second" << ( seconds == 1 ? L" " : L"s " );
			if( milliseconds )
				sstr << milliseconds << L" millisecond" << ( milliseconds == 1 ? L"" : L"s" );

			return sstr.str();
		}

		//
		// Format time from seconds to HH:MM:SS
		//
		static std::wstring formatHex( const std::wstring& str )
		{
			std::wstring strOut;

			bool first = true, space = true;

			for( auto it = str.begin(); it != str.end(); ++it )
			{
				if( std::iswxdigit( *it ) )
				{
					strOut.append( 1, toupper( *it ) );

					if( !first && it + 1 != str.end() )
						strOut.append( 1, L' ' );

					first = space = !first;
				}
				else if( std::iswspace( *it ) )
				{
					if( !space )
						strOut.append( 1, L' ' );

					space = first = true;
				}
			}

			return strOut;
		}

		//
		// Get Crc32 value as string
		//
		static std::wstring formatCrc32( DWORD hash )
		{
			std::wostringstream sstr;
			sstr << std::hex << std::setfill( L'0' ) << std::setw( 8 ) << std::uppercase << hash;

			return sstr.str();
		}

		//
		// Get hash (md5, sha-1, etc.) value as string
		//
		static std::wstring formatHashValue( const unsigned char *hash, size_t len )
		{
			std::wostringstream sstr;

			for( size_t i = 0; i < len; ++i )
				sstr << std::hex << std::setfill( L'0' ) << std::setw( 2 ) << std::uppercase << hash[i];

			return sstr.str();
		}

		//
		// Try to read and detect UNICODE byte-order-mark for a given filename
		//
		static EUtfBom getByteOrderMarker( const char *buf, int len, int& bomLen )
		{
			if( len > 1 )
			{
				// UTF-32 big endian
				if( len >= 4 && !memcmp( buf, "\x00\x00\xFE\xFF", 4 ) ) {
					bomLen = 4;
					return StringUtils::UTF32BE;
				}
				// UTF-32 little endian
				if( len >= 4 && !memcmp( buf, "\xFF\xFE\x00\x00", 4 ) ) {
					bomLen = 4;
					return StringUtils::UTF32LE;
				}
				// UTF-8
				if( len >= 3 && !memcmp( buf, "\xEF\xBB\xBF", 3 ) ) {
					bomLen = 3;
					return StringUtils::UTF8;
				}
				// UTF-16 big endian
				if( !memcmp( buf, "\xFE\xFF", 2 ) ) {
					bomLen = 2;
					return StringUtils::UTF16BE;
				}
				// UTF-16 little endian
				if( !memcmp( buf, "\xFF\xFE", 2 ) ) {
					bomLen = 2;
					return StringUtils::UTF16LE;
				}
			}

			return EUtfBom::NOBOM;
		}

		//
		// Convert bytes from various code-pages (including UTF-32) to UTF16-LE
		//
		static int mb2Utf16( UINT encoding, const LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar )
		{
			int nchLen;

			switch( encoding )
			{
			case EUtfBom::UTF16LE:
			case EUtfBom::UTF16BE:
			{
				if ( !lpMultiByteStr || ( cbMultiByte < 0 ) || ( cchWideChar < 0 ) )
				{
					SetLastError( ERROR_INVALID_PARAMETER );
					return -1;
				}

				cbMultiByte /= 2;
				nchLen = cbMultiByte;

				if( lpWideCharStr )
				{
					if( cchWideChar < nchLen )
					{
						SetLastError( ERROR_INSUFFICIENT_BUFFER );
						return -1;
					}

					if( encoding == EUtfBom::UTF16LE )
					{
						CopyMemory( lpWideCharStr, lpMultiByteStr, nchLen * 2 );
					}
					else
					{
						UINT16 *pCodeUnits = (UINT16*)lpMultiByteStr;

						for( int i = 0; i < cbMultiByte; ++i )
						{
							lpWideCharStr[i] = (WCHAR)( ( ( pCodeUnits[i] << 8 ) & 0xFF00) | ( ( pCodeUnits[i] >> 8 ) & 0x00FF ) );
						}
					}
				}

				SetLastError( 0 );
				break;
			}

			case EUtfBom::UTF32LE:
			case EUtfBom::UTF32BE:
			{
				if( !lpMultiByteStr || ( cbMultiByte < 0 ) || ( cchWideChar < 0 ) )
				{
					SetLastError( ERROR_INVALID_PARAMETER );
					return -1;
				}

				PUINT32 pCodePoints = (PUINT32)lpMultiByteStr;
				cbMultiByte /= 4;

				nchLen = 0;

				for( int i = 0; i < cbMultiByte; ++i )
				{
					UINT32 CodePoint = pCodePoints[i];
					if( encoding == EUtfBom::UTF32BE )
					{
						CodePoint = ( ( ( CodePoint >> 24 ) & 0x000000FF ) | ( ( CodePoint >> 8 ) & 0x0000FF00 ) | ( ( CodePoint << 8 ) & 0x00FF0000 ) | ( ( CodePoint << 24 ) & 0xFF000000 ) );
					}

					if( CodePoint < 0x10000 )
					{
						if( lpWideCharStr )
						{
							if( cchWideChar < 1 )
							{
								SetLastError( ERROR_INSUFFICIENT_BUFFER );
								return -1;
							}

							*lpWideCharStr++ = (WCHAR)( CodePoint & 0xFFFF );
							--cchWideChar;
						}

						++nchLen;
					}
					else if( CodePoint <= 0x10FFFF )
					{
						if( lpWideCharStr )
						{
							if( cchWideChar < 2 )
							{
								SetLastError( ERROR_INSUFFICIENT_BUFFER );
								return -1;
							}

							CodePoint -= 0x10000;
							*lpWideCharStr++ = (WCHAR)( 0xD800 + ( ( CodePoint >> 10 ) & 0x3FF ) );
							*lpWideCharStr++ = (WCHAR)( 0xDC00 + ( CodePoint & 0x3FF ) );
							cchWideChar -= 2;
						}

						nchLen += 2;
					}
					else
					{
						SetLastError( ERROR_NO_UNICODE_TRANSLATION );
						return -1;
					}
				}

				SetLastError( 0 );
				break;
			}

			default:
				nchLen = MultiByteToWideChar( encoding, 0, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar );
				break;
			}

			return nchLen;
		}

		//
		// Convert windows default UTF-16 to bytes in various encodings/code-pages (including UTF-32)
		//
		static void utf16ToMultiByte( const std::wstring& str, std::string& strOut, UINT encoding )
		{
			switch( encoding )
			{
			case EUtfBom::UTF32BE:
			case EUtfBom::UTF32LE:
			{
				strOut.resize( str.length() * sizeof( char32_t ) );
				char32_t uc, *output = (char32_t*)&strOut[0];

				for( auto it = str.begin(); it != str.end(); ++it )
				{
					uc = *it;

					if( !( uc < HIGH_SURROGATE_START || uc > LOW_SURROGATE_END ) )
					{
						if( IS_HIGH_SURROGATE( uc ) && ( it + 1 ) != str.end() && IS_LOW_SURROGATE( *( it + 1 ) ) )
						{
							uc = ( uc << 10 ) + *( ++it ) - 0x35FDC00; // surrogate to utf32

							if( encoding == EUtfBom::UTF32BE )
								uc =  _byteswap_ulong( uc );

							*output++ = uc;
						}
						else
							*output++ = ( encoding == EUtfBom::UTF32BE ? 0xFDFF : 0xFFFD ); // error
					}
					else
					{
						if( encoding == EUtfBom::UTF32BE )
							uc = _byteswap_ulong( uc );

						*output++ = uc;
					}
				}

				size_t len = output - (char32_t*)&strOut[0];
				strOut.resize( len * sizeof( char32_t ) );

				break;
			}
			case EUtfBom::UTF16BE:
			case EUtfBom::UTF16LE:
			{
				strOut.resize( str.length() * sizeof( wchar_t ) );
				wchar_t uc, *output = (wchar_t*)&strOut[0];

				for( auto it = str.begin(); it != str.end(); ++it )
				{
					uc = *it;

					if( encoding == EUtfBom::UTF16BE )
						uc = ( ( ( uc >> 8 ) & 0x00FF ) | ( ( uc << 8 ) & 0xFF00 ) );

					*output++ = uc;
				}
				break;
			}
			default:
				strOut = convert2A( str, encoding );
				break;
			}
		}

		//
		// Check whether the 'str' is a valid utf-8 string
		//
		static bool isUtf8( const char *str, unsigned int strLen, bool *pHasNullChar = nullptr, bool *pHasAnsiChar = nullptr )
		{
			int state = 0;
			bool overlong = false;
			bool surrogate = false;
			bool nonchar = false;
			unsigned short olupper = 0;
			unsigned short slower = 0;

			if( pHasNullChar ) *pHasNullChar = false;
			if( pHasAnsiChar ) *pHasAnsiChar = false;

			const char *p = str;
			const char *pEnd = p + strLen;

			// for each character in this chunk
			while( p < pEnd )
			{
				BYTE c;

				if( state == 0 )
				{
					c = *p++;

					if( ( 0x80 & c ) == 0 ) // ASCII
					{
						if( pHasNullChar && c == 0x00 ) *pHasNullChar = true;
						continue;
					}

					if( c <= 0xBF ) // [80-BF] when it's not expected
						return false;
					else if( c <= 0xDF ) // 2 bytes UTF8
					{
						if( pHasAnsiChar ) *pHasAnsiChar = true;

						state = 1;
						if( ( 0x1e & c ) == 0 ) // overlong : [C0-C1][80-BF]
							return false;
					}
					else if( c <= 0xEF ) // 3 bytes UTF8
					{
						if( pHasAnsiChar ) *pHasAnsiChar = true;

						state = 2;
						if( c == 0xE0 ) // to exclude E0[80-9F][80-BF] 
						{
							overlong = true;
							olupper = 0x9F;
						}
						if( c == 0xED ) // ED[A0-BF][80-BF] : surrogate codepoint
						{
							surrogate = true;
							slower = 0xA0;
						}
						if( c == 0xEF ) // EF BF [BE-BF] : non-character
							nonchar = true;
					}
					else if( c <= 0xF4 ) // 4 bytes UTF8
					{
						if( pHasAnsiChar ) *pHasAnsiChar = true;

						state = 3;
						nonchar = true;
						if( c == 0xF0 ) // to exclude F0[80-8F][80-BF]{2}
						{
							overlong = true;
							olupper = 0x8F;
						}
						if( c == 0xF4 ) // to exclude F4[90-BF][80-BF] 
						{
							// actually not surrogates but codepoints beyond 0x10FFFF
							surrogate = true;
							slower = 0x90;
						}
					}
					else
						return false; // Not UTF8 string
				}

				while( p < pEnd && state )
				{
					c = *p++;
					--state;

					// non-character : EF BF [BE-BF] or F[0-7] [89AB]F BF [BE-BF]
					if( nonchar && ( !state &&  c < 0xBE || state == 1 && c != 0xBF || state == 2 && ( 0x0F & c ) != 0x0F ) )
						nonchar = false;

					if( ( 0xC0 & c ) != 0x80 || overlong && c <= olupper || surrogate && slower <= c || nonchar && !state )
						return false; // Not UTF8 string

					overlong = surrogate = false;
				}
			}

			return !state;
		}

		//
		// Check if buffer contains binary data
		//
		static bool isBinary( const char *buf, int len, bool *pHasNullChar = nullptr )
		{
			bool ret = false;

			if( pHasNullChar )
				*pHasNullChar = false;

			const char NUL = (char)0;  // null character
			const char BS  = (char)8;  // back-space
			const char CR  = (char)13; // carriage return
			const char SUB = (char)26; // substitute

			for( int i = 0; i < len; ++i )
			{
				if( ( buf[i] > NUL && buf[i] < BS ) || ( buf[i] > CR && buf[i] < SUB ) )
				{
					ret = true;
					break;
				}
				else if( buf[i] == NUL && pHasNullChar )
					*pHasNullChar = true;
			}

			return ret;
		}

		//
		// Check if buffer contains null characters in a row for particular encoding
		//
		static bool isNulls( const char *buf, int len, EUtfBom bom, bool *pHasNullChar = nullptr )
		{
			int nullCnt = 0, nullCntMax = 0;
			int max_nulls = 1; // tolerable amount for ansi, utf-8

			switch( bom )
			{
			case EUtfBom::UTF32LE: case EUtfBom::UTF32BE:
				max_nulls = 6;
				break;
			case EUtfBom::UTF16LE: case EUtfBom::UTF16BE:
				max_nulls = 2;
				break;
			}

			for( int i = 0; i < len; ++i )
			{
				if( buf[i] == '\0' )
				{
					nullCnt++;
				}
				else if( nullCnt )
				{
					nullCntMax = max( nullCnt, nullCntMax );
					nullCnt = 0;

					if( nullCntMax > max_nulls )
						break;
				}
			}

			if( pHasNullChar && max( nullCnt, nullCntMax ) > 0 )
				*pHasNullChar = true;

			return max( nullCnt, nullCntMax ) > max_nulls;
		}

		//
		// Check if 'lb' is the lead utf8 byte
		//
		static bool isUtf8LeadByte( unsigned char lb )
		{
			if( ( lb & 0x80 ) == 0 )         // lead bit is zero, must be a single ascii
				return true; // 1 byte
			else if( ( lb & 0xE0 ) == 0xC0 ) // 110x xxxx
				return true; // 2 bytes
			else if( ( lb & 0xF0 ) == 0xE0 ) // 1110 xxxx
				return true; // 3 bytes
			else if( ( lb & 0xF8 ) == 0xF0 ) // 1111 0xxx
				return true; // 4 bytes
			else
				return false;
		}

		//
		// Find text string in a memory buffer
		//
		static const char *memstr( const char *buf, std::streamsize len, const std::string& str )
		{
			const char *p = buf;

			while( p && ( ( p - buf ) < len ) && ( p = reinterpret_cast<const char*>( memchr( p, str[0], static_cast<size_t>( len - ( p - buf ) ) ) ) ) )
			{
				if( ( len - ( p - buf ) ) >= static_cast<std::streamsize>( str.length() ) && !memcmp( p, &str[0], str.length() ) )
					return p;

				++p;
			}

			return nullptr;
		}

		//
		// Find unicode text string in a memory buffer
		//
		static const wchar_t *memstrw( const wchar_t *buf, std::streamsize len, const std::wstring& str )
		{
			const wchar_t *p = buf;

			while( p && ( ( p - buf ) < len ) && ( p = reinterpret_cast<const wchar_t*>( wmemchr( p, str[0], static_cast<size_t>( len - ( p - buf ) ) ) ) ) )
			{
				if( ( len - ( p - buf ) ) >= static_cast<std::streamsize>( str.length() ) && !wmemcmp( p, &str[0], str.length() ) )
					return p;

				++p;
			}

			return nullptr;
		}

		//
		// Find case-insensitive text in a memory buffer (locale needs to be initialized)
		//
		static const char *memstri( const char *buf, std::streamsize len, const std::string& str )
		{
			char cLower = convertChar2Lwr( str[0] );
			char cUpper = convertChar2Upr( str[0] );

			const char *p = buf, *p1, *p2;

			p1 = reinterpret_cast<const char*>( memchr( p, cLower, static_cast<size_t>( len ) ) );

			if( cLower == cUpper )
				p2 = nullptr;
			else
				p2 = reinterpret_cast<const char*>( memchr( p, cUpper, static_cast<size_t>( len ) ) );

			if( !p1 || !p2 )
				p = max( p1, p2 );
			else
				p = min( p1, p2 );

			while( p && ( p - buf ) < len )
			{
				std::streamsize remainder = len - ( p - buf );

				if( remainder < static_cast<std::streamsize>( str.length() ) )
					break;

				if( !_memicmp( p, &str[0], str.length() ) )
				{
					return p;
				}

				if( p == p1 )
					p1 = reinterpret_cast<const char*>( memchr( p + 1, cLower, static_cast<size_t>( remainder - 1 ) ) );
				else
					p2 = reinterpret_cast<const char*>( memchr( p + 1, cUpper, static_cast<size_t>( remainder - 1 ) ) );

				if( !p1 || !p2 )
					p = max( p1, p2 );
				else
					p = min( p1, p2 );
			}

			return nullptr;
		}

		//
		// Find unicode case-insensitive text in a memory buffer
		//
		static const wchar_t *memstriw( const wchar_t *buf, std::streamsize len, const std::wstring& str )
		{
			wchar_t cLower = convertChar2Lwr( str[0] );
			wchar_t cUpper = convertChar2Upr( str[0] );

			const wchar_t *p = buf, *p1, *p2;

			p1 = reinterpret_cast<const wchar_t*>( wmemchr( p, cLower, static_cast<size_t>( len ) ) );

			if( cLower == cUpper )
				p2 = nullptr;
			else
				p2 = reinterpret_cast<const wchar_t*>( wmemchr( p, cUpper, static_cast<size_t>( len ) ) );

			if( !p1 || !p2 )
				p = max( p1, p2 );
			else
				p = min( p1, p2 );

			while( p && ( p - buf ) < len )
			{
				std::streamsize remainder = len - ( p - buf );

				if( remainder < static_cast<std::streamsize>( str.length() ) )
					break;

				// compare strings case insensitive
				if( CompareStringEx(
					LOCALE_NAME_USER_DEFAULT,
					NORM_IGNORECASE,
					p, static_cast<int>( str.length() ),
					&str[0], static_cast<int>( str.length() ),
					NULL, NULL, 0 ) == CSTR_EQUAL )
				{
					return p;
				}

				if( p == p1 )
					p1 = reinterpret_cast<const wchar_t*>( wmemchr( p + 1, cLower, static_cast<size_t>( remainder - 1 ) ) );
				else
					p2 = reinterpret_cast<const wchar_t*>( wmemchr( p + 1, cUpper, static_cast<size_t>( remainder - 1 ) ) );

				if( !p1 || !p2 )
					p = max( p1, p2 );
				else
					p = min( p1, p2 );
			}

			return nullptr;
		}

		//
		// Find unicode string in a memory buffer (with starting offset, match casing, match whole words and direction parameters)
		//
		static const wchar_t *findStr( const wchar_t *buf, size_t len, const std::wstring& text, bool match_case, bool words, bool *bounds, size_t off = 0, bool reverse = false )
		{
			const wchar_t *res = nullptr, *p = nullptr;

			auto pFunc = match_case ? &memstrw : &memstriw;

			size_t idx = 0;

			if( reverse )
			{
				// try to find the last occurrence of 'text' in 'buf' string
				while( ( p = (*pFunc)( buf + idx, (UINT)( off - idx ), text ) ) != 0 )
				{
					idx = ( p - buf ) + text.length();

					if( idx > off )
						break; // text found outside of boundaries

					if( words )
					{
						if( ( p == buf || !bounds[0] || !isAlphaNum( *( p - 1 ) ) ) &&
							( ( p - buf + text.length() ) == len || !bounds[1] || !isAlphaNum( *( p + text.length() ) ) ) )
						{
							res = p;
						}
					}
					else
						res = p;
				}
			}
			else
			{
				idx = off;

				while( ( p = (*pFunc)( buf + idx, (UINT)( len - idx ), text ) ) != 0 )
				{
					idx = ( p - buf ) + text.length();

					if( words )
					{
						if( ( p == buf || !bounds[0] || !isAlphaNum( *( p - 1 ) ) ) &&
							( ( p - buf + text.length() ) == len || !bounds[1] || !isAlphaNum( *( p + text.length() ) ) ) )
						{
							res = p;
							break;
						}
					}
					else
					{
						res = p;
						break;
					}
				}
			}

			return res;
		}

		//
		// Encode base64 string
		//
		static std::string base64Encode( const unsigned char *str, size_t len )
		{
			static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

			size_t olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
			olen += olen / 72; /* line feeds */
			olen++; /* nul termination */

			if( olen < len )
				return std::string(); /* integer overflow */

			unsigned char *out = (unsigned char*)malloc( olen );
			if( out == nullptr )
				return std::string();

			const unsigned char *end = str + len;
			const unsigned char *in = str;
			unsigned char *pos = out;
			int line_len = 0;
			while( end - in >= 3 )
			{
				*pos++ = base64_table[in[0] >> 2];
				*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
				*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
				*pos++ = base64_table[in[2] & 0x3f];
				in += 3;
				line_len += 4;
				if( line_len >= 72 )
				{
					*pos++ = '\n';
					line_len = 0;
				}
			}

			if( end - in )
			{
				*pos++ = base64_table[in[0] >> 2];
				if( end - in == 1 )
				{
					*pos++ = base64_table[(in[0] & 0x03) << 4];
					*pos++ = '=';
				}
				else
				{
					*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
					*pos++ = base64_table[(in[1] & 0x0f) << 2];
				}
				*pos++ = '=';
				line_len += 4;
			}

			if( line_len )
				*pos++ = '\n';

			*pos = '\0';

			size_t out_len = pos - out;
			std::string strOut( (char*)out, out_len );

			free( out );
			
			return strOut;
		}

		//
		// Decode base64 string
		//
		static std::string base64Decode( const unsigned char *str, size_t len )
		{
			static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

			unsigned char dtable[256], *out, *pos, block[4], tmp;
			size_t i, count, olen;
			int pad = 0;

			memset( dtable, 0x80, 256 );
			for( i = 0; i < sizeof(base64_table) - 1; i++ )
				dtable[base64_table[i]] = (unsigned char)i;

			dtable['='] = 0;

			count = 0;
			for( i = 0; i < len; i++ )
			{
				if( dtable[str[i]] != 0x80 )
					count++;
			}

			if( count == 0 || count % 4 )
				return std::string();

			olen = count / 4 * 3;
			pos = out = (unsigned char*)malloc( olen );
			if( out == nullptr )
				return std::string();

			count = 0;
			for( i = 0; i < len; i++ )
			{
				tmp = dtable[str[i]];
				if( tmp == 0x80 )
					continue;

				if( str[i] == '=' )
					pad++;
				block[count] = tmp;
				count++;
				if( count == 4 )
				{
					*pos++ = (block[0] << 2) | (block[1] >> 4);
					*pos++ = (block[1] << 4) | (block[2] >> 2);
					*pos++ = (block[2] << 6) | block[3];
					count = 0;
					if( pad )
					{
						if( pad == 1 )
							pos--;
						else if( pad == 2 )
							pos -= 2;
						else
						{
							/* Invalid padding */
							free( out );
							return std::string();
						}
						break;
					}
				}
			}

			size_t out_len = pos - out;
			std::string strOut( (char*)out, out_len );

			free( out );
			
			return strOut;
		}

		// do not instantiate this class/struct
		StringUtils() = delete;
		StringUtils( StringUtils const& ) = delete;
		void operator=( StringUtils const& ) = delete;
	};
}
