#include "stdafx.h"

/* Borland C++ Builder string types and helper functions */

#include "Commander.h"
#include "PathUtils.h"
#include "bcb_stuff.h"

using namespace Commander;

namespace bcb
{
	/* Types aliases */
	using TStrings = std::vector<UnicodeString>;
	using TStream = std::ifstream;

	/* Constants */
	const UnicodeString EmptyString( L"" );//TraceInitStr( L"\1\1\1" ) ); // magic

	const wchar_t *EmptyStr = L"";
	const wchar_t *sLineBreak = L"\r\n";
	const wchar_t NormalizedFingerprintSeparator = L'-';

	const int SshPortNumber = 22;
	const int FtpPortNumber = 21;
	const int FtpsImplicitPortNumber = 990;
	const int HTTPPortNumber = 80;
	const int HTTPSPortNumber = 443;
	const int TelnetPortNumber = 23;
	const int DefaultSendBuf = 262144;
	const int ProxyPortNumber = 80;
	const int SFTPMinVersion = 0;
	const int SFTPMaxVersion = 6;
	const UINT GUIUpdateInterval = 100;

	const int MinsPerHour = 60;
	const int MSecsPerSec = 1000;
	const int SecsPerDay = 24 * 60 * 60;

	/*
		Ansi string
	*/
	AnsiString::AnsiString( const UnicodeString& str ) : _str( StringUtils::convert2A( const_cast<UnicodeString&>( str ).get(), CP_ACP ) ) {}

	AnsiString AnsiString::SubString( size_t off, size_t count ) const
	{
		_ASSERTE( off > 0 ); /* BCB indexes strings from 1 */
		return _str.substr( off - 1, count );
	}

	/*
		Unicode string
	*/
	UnicodeString::UnicodeString( const char *str ) : _str( StringUtils::convert2W( str ) ) {}
	UnicodeString::UnicodeString( const std::string& str ) : _str( StringUtils::convert2W( str ) ) {}
	UnicodeString::UnicodeString( const AnsiString& str ) : _str( StringUtils::convert2W( const_cast<AnsiString&>( str ).get(), CP_ACP ) ) {}
	UnicodeString::UnicodeString( const UTF8String& str ) : _str( StringUtils::convert2W( const_cast<UTF8String&>( str ).get() ) ) {}
	UnicodeString::UnicodeString( const RawByteString& str ) : _str( StringUtils::convert2W( const_cast<RawByteString&>( str ).get() ) ) {}

	int UnicodeString::Pos( const wchar_t ch ) const
	{
		size_t idx = _str.find_first_of( ch ); /* BCB indexes strings from 1 */
		return static_cast<int>( ( idx != std::wstring::npos ? idx + 1 : 0 ) );
	}

	int UnicodeString::Pos( const wchar_t *str ) const
	{
		size_t idx = _str.find( str ); /* BCB indexes strings from 1 */
		return static_cast<int>( ( idx != std::wstring::npos ? idx + 1 : 0 ) );
	}

	UnicodeString UnicodeString::Trim() const
	{
		auto str = _str; StringUtils::trim( str ); return str;
	}

	UnicodeString UnicodeString::TrimRight() const
	{
		auto str = _str; StringUtils::rTrim( str ); return str;
	}

	UnicodeString UnicodeString::SubString( size_t off, size_t count ) const
	{
		_ASSERTE( off > 0 ); /* BCB indexes strings from 1 */
		return _str.substr( off - 1, count );
	}

	int UnicodeString::LastDelimiter( const wchar_t *str ) const
	{
		size_t idx = _str.find_last_of( str ); /* BCB indexes strings from 1 */
		return static_cast<int>( ( idx != std::wstring::npos ? idx + 1 : 0 ) );
	}

	void UnicodeString::Insert( const wchar_t *str, size_t off )
	{
		_ASSERTE( off > 0 ); /* BCB indexes strings from 1 */
		_str.insert( off - 1, str );
	}

	void UnicodeString::Delete( size_t off, size_t count )
	{
		_ASSERTE( off > 0 ); /* BCB indexes strings from 1 */
		_str.erase( off - 1, count );
	}

	/*
		Utf8 string
	*/
	UTF8String::UTF8String( const UnicodeString& str ) : _str( StringUtils::convert2A( const_cast<UnicodeString&>( str ).get() ) ) {}
	UTF8String::UTF8String( const RawByteString& str ) : _str( const_cast<RawByteString&>( str ).get() ) {}

	UTF8String UTF8String::SubString( size_t off, size_t count ) const
	{
		_ASSERTE( off > 0 ); /* BCB indexes strings from 1 */
		return _str.substr( off - 1, count );
	}

	/*
		Raw byte string
	*/
	RawByteString::RawByteString( const wchar_t *str ) : _str( StringUtils::convert2A( str ) ) {}
	RawByteString::RawByteString( const UnicodeString& str ) : _str( StringUtils::convert2A( const_cast<UnicodeString&>( str ).get(), CP_ACP ) ) {}

	RawByteString RawByteString::SubString( size_t off, size_t count ) const
	{
		_ASSERTE( off > 0 ); /* BCB indexes strings from 1 */
		return _str.substr( off - 1, count );
	}

	/* Guard helper */
	TGuard::TGuard( TCriticalSection *criticalSection ) : _criticalSection( criticalSection )
	{
		DebugAssert( _criticalSection != nullptr );
		_criticalSection->lock();
	}

	TGuard::~TGuard()
	{
		_criticalSection->unlock();
	}

	/* Unguard helper */
	TUnguard::TUnguard( TCriticalSection *criticalSection ) : _criticalSection( criticalSection )
	{
		DebugAssert( _criticalSection != nullptr );
		_criticalSection->unlock();
	}

	TUnguard::~TUnguard()
	{
		_criticalSection->lock();
	}

	/* Helper functions */
	TDateTime Now()
	{
		SYSTEMTIME st;
		GetSystemTime( &st );

		FILETIME ft;
		SystemTimeToFileTime( &st, &ft );

		return ULARGE_INTEGER{ ft.dwLowDateTime, ft.dwHighDateTime }.QuadPart;
	}

	void Abort() { abort(); }

	bool FileExists( UnicodeString& fileName )
	{
		WIN32_FIND_DATA wfd = { 0 };
		HANDLE hFile = FindFirstFile( PathUtils::getExtendedPath( fileName.get() ).c_str(), &wfd );

		if( hFile != INVALID_HANDLE_VALUE )
		{
			FindClose( hFile );
			return true;
		}

		return false;
	}

	bool SameStr( const UnicodeString& str1, const UnicodeString& str2 )
	{
		return !wcscmp( str1.c_str(), str2.c_str() );
	}

	bool SameText( const UnicodeString& str1, const UnicodeString& str2 )
	{
		return !_wcsicmp( str1.c_str(), str2.c_str() );
	}

	bool SameChecksum( const UnicodeString& sum1, const UnicodeString& sum2, bool base64 )
	{
		UnicodeString Checksum1( sum1 );
		UnicodeString Checksum2( sum2 );

		bool result;

		if( base64 )
		{
			Checksum1 = Base64ToUrlSafe( Checksum1 );
			Checksum2 = Base64ToUrlSafe( Checksum2 );
			result = SameStr( Checksum1, Checksum2 );
		}
		else
		{
			Checksum1 = MD5ToUrlSafe( Checksum1 );
			Checksum2 = MD5ToUrlSafe( Checksum2 );
			result = SameText( Checksum1, Checksum2 );
		}

		return result;
	}

	bool StartsStr( const UnicodeString& str, UnicodeString& text )
	{
		if( text.Length() >= str.Length() )
		{
			return std::equal(
				const_cast<UnicodeString&>( str ).get().begin(),
				const_cast<UnicodeString&>( str ).get().end(),
				text.get().begin() );
		}
		else
			return false;
	}

	bool StartsText( const UnicodeString& str, UnicodeString& text )
	{
		return StringUtils::startsWith( text.get(), const_cast<UnicodeString&>( str ).get() );
	}

	bool CutToken( UnicodeString& str, UnicodeString& token )
	{
		bool result;

		token = L"";

		// inspired by Putty's sftp_getcmd() from PSFTP.C
		int index = 1;
		while( ( index <= str.Length() ) && ( ( str[index] == L' ' ) || ( str[index] == L'\t' ) ) )
		{
			index++;
		}

		if( index <= str.Length() )
		{
			bool quoting = false;

			while( index <= str.Length() )
			{
				if( !quoting && ( ( str[index] == L' ' ) || ( str[index] == L'\t' ) ) )
				{
					break;
				}
				else if( ( str[index] == L'"' ) && ( index + 1 <= str.Length() ) && ( str[index + 1] == L'"' ) && quoting )
				{
					index += 2;
					token += L'"';
				}
				else if( str[index] == L'"' )
				{
					index++;
					quoting = !quoting;
				}
				else
				{
					token += str[index];
					index++;
				}
			}

			if( index <= str.Length() )
				index++;

			str = str.SubString( index, str.Length() );

			result = true;
		}
		else
		{
			result = false;
			str = L"";
		}

		return result;
	}

	bool ContainsStr( UnicodeString& str, const UnicodeString& subStr )
	{
		return ( str.get().find( subStr.c_str() ) != std::wstring::npos );
	}

	bool ContainsText( UnicodeString& str, const wchar_t *subText )
	{
		return !!StrStrIW( &str.get()[0], subText );
	}

	bool IsNumber( UnicodeString& str )
	{
		return StrToInt64( str ) != std::string::npos;
	}

	bool TryStrToInt( UnicodeString& str, int& outVal )
	{
		try {
			outVal = std::stoi( str.get() );
		}
		catch( std::invalid_argument ) {
			return false;
		}
		catch( std::out_of_range ) {
			return false;
		}

		return true;
	}

	bool IsHex( wchar_t ch )
	{
		return
			isdigit( ch ) ||
			( ( ch >= 'A' ) && ( ch <= 'F' ) ) ||
			( ( ch >= 'a' ) && ( ch <= 'f' ) );
	}

	void Shred( AnsiString& str )
	{
		if( !str.IsEmpty() )
			memset( str.c_str(), 0, str.Length() );
	}

	void PackStr( AnsiString& str )
	{
		str = str.c_str();
	}

	uintmax_t StrToInt64( UnicodeString& str )
	{
		uintmax_t num = 0ull;

		try {
			num = std::stoull( str.get() );
		}
		catch( std::invalid_argument ) {
			return std::string::npos;
		}
		catch( std::out_of_range ) {
			return std::string::npos;
		}

		return num;
	}

	AnsiString PuttyStr( const UnicodeString& str )
	{
		return AnsiString( str );
	}

	UnicodeString IntToStr( int num )
	{
		return std::to_wstring( num );
	}

	UnicodeString LoadStr( UINT strId )
	{
		std::wstring str;
		str.resize( 1024 );

		int len = LoadString( FCS::inst().getFcInstance(), strId, &str[0], static_cast<int>( str.length() ) );
		str.resize( static_cast<int>( len ) );

		return str;
	}

	UnicodeString fmtLoadStr( UINT strId, StrArray<UnicodeString>& vals )
	{
		std::wstring str = LoadStr( strId ).get();

		size_t off = 0;
		for( int i = 0; i < vals.size(); i++ )
		{
			// expand %s and %d with values from the StrArray
			off = str.find_first_of( L'%', off );
			if( off != std::wstring::npos && off != str.length() - 1 )
			{
				// we treat %d as string since FMTLOAD macro expects UnicodeString args anyway
				if( str[off+1] == L's' || str[off+1] == L'd' )
					str.replace( off, 2, vals.at( i ).c_str() );
				else
				{
					PrintDebug("Unsupported format element: '%%%lc'", str[off + 1]);
					off++;
				}
			}
		}

		return str;
	}

	UnicodeString IncludeTrailingBackslash( UnicodeString& str )
	{
		std::wstring strOut = PathUtils::addDelimiter( str.get() );

		return strOut;
	}

	UnicodeString ExpandEnvironmentVariables( const UnicodeString& str )
	{
		size_t size = 1024;
		std::wstring buf( size, 0 );

		DWORD len = ExpandEnvironmentStrings( str.c_str(), &buf[0], static_cast<DWORD>( size ) );

		if( len > size )
		{
			buf.resize( len );
			ExpandEnvironmentStrings( str.c_str(), &buf[0], static_cast<DWORD>( len ) );
		}

		return buf.c_str();
	}

	UnicodeString ExtractFileName( const UnicodeString& path )
	{
		return PathUtils::stripPath( const_cast<UnicodeString&>( path ).get() );
	}

	UnicodeString DeleteChar( UnicodeString& str, wchar_t ch )
	{
		str.get().erase( std::remove( str.get().begin(), str.get().end(), '\r'), str.get().end() );

		return str;
	}

	UnicodeString CutToChar( UnicodeString& str, wchar_t ch, bool trim )
	{
		return StringUtils::cutToChar( str.get(), ch, trim );
	}

	UnicodeString RightStr( UnicodeString& str, size_t len )
	{
		return str.get().substr( str.get().length() - len );
	}

	UnicodeString ReplaceStr( const UnicodeString& str, const wchar_t *from, const wchar_t *to )
	{
		std::wstring strOut = const_cast<UnicodeString&>( str) .get();
		StringUtils::replaceAll( strOut, from, to );

		return strOut;
	}

	UnicodeString StripPathQuotes( UnicodeString& path )
	{
		if( ( path.Length() >= 2 ) && ( path[1] == L'\"' ) && ( path[path.Length()] == L'\"' ) )
		{
			return path.SubString( 2, path.Length() - 2 );
		}
		else
		{
			return path;
		}
	}

	UnicodeString MainInstructions( const UnicodeString& str )
	{
		UnicodeString mainMsgTag = LoadStr( MAIN_MSG_TAG );
		return mainMsgTag + str + mainMsgTag;
	}

	UnicodeString NormalizeString( const UnicodeString& str )
	{
		UnicodeString result = str;
		if( result == const_cast<UnicodeString&>( EmptyString ) )
		{
			result = UnicodeString();
		}

		return result;
	}

	UnicodeString AnsiToString( const RawByteString& str )
	{
		return AnsiToString( const_cast<RawByteString&>( str ).c_str(), str.Length() );
	}

	UnicodeString AnsiToString( const char *str, size_t len )
	{
		if( str )
		{
			if( len == std::string::npos )
				len = strlen( str );

			return StringUtils::convert2W( std::string( str, len ), CP_ACP );
		}

		return EmptyString;
	}

	UnicodeString UTF8ToString( const RawByteString& str )
	{
		return UTF8ToString( const_cast<RawByteString&>( str ).c_str(), str.Length() );
	}

	UnicodeString UTF8ToString( const char *str, size_t len )
	{
		if( str )
		{
			if( len == std::string::npos )
				len = strlen( str );

			return StringUtils::convert2W( std::string( str, len ) );
		}

		return EmptyString;
	}

	UnicodeString BytesToHex( const RawByteString& str )
	{
		return BytesToHex( reinterpret_cast<unsigned char*>( const_cast<RawByteString&>( str ).c_str() ), str.Length() );
	}

	UnicodeString BytesToHex( const unsigned char *buf, size_t len )
	{
		return StringUtils::formatHashValue( buf, len );
	}

	UnicodeString EncodeBase64( const char *str, size_t len )
	{
		return StringUtils::convert2W( StringUtils::base64Encode( reinterpret_cast<const unsigned char*>( str ), len ) );
	}

	UnicodeString EncodeStrToBase64( const RawByteString& str )
	{
		UnicodeString result = EncodeBase64( const_cast<RawByteString&>( str ).c_str(), str.Length() );
		result = ReplaceStr( result, sLineBreak, EmptyStr );

		return result;
	}

	RawByteString DecodeBase64ToStr( const UnicodeString& str )
	{
		auto s = StringUtils::convert2A( const_cast<UnicodeString&>( str ).get() );
		return StringUtils::base64Decode( reinterpret_cast<const unsigned char*>( s.c_str() ), s.length() );
	}

	UnicodeString Base64ToUrlSafe( UnicodeString& str )
	{
		std::wstring& ws = str.get();

		while( StringUtils::endsWith( ws, L"=" ) )
			ws.pop_back();

		// See https://en.wikipedia.org/wiki/Base64#Implementations_and_history
		std::replace( ws.begin(), ws.end(), L'+', L'-' );
		std::replace( ws.begin(), ws.end(), L'/', L'_' );

		return ws;
	}

	UnicodeString MD5ToUrlSafe( UnicodeString& str )
	{
		std::wstring& ws = str.get();
		std::replace( ws.begin(), ws.end(), L':', NormalizedFingerprintSeparator );

		return ws;
	}

	bool SameIdent( const UnicodeString& ident1, const UnicodeString& ident2 )
	{
		const wchar_t dash[] = L"-";

		return SameText(
			ReplaceStr( const_cast<UnicodeString&>( ident1 ), dash, EmptyStr ).c_str(),
			ReplaceStr( const_cast<UnicodeString&>( ident2 ), dash, EmptyStr ).c_str() );
	}

	UnicodeString FindIdent( const UnicodeString& ident, TStrings *idents )
	{
		for( int i = 0; i < idents->size(); i++ )
		{
			if( SameIdent( ident, idents->at( i ) ) )
			{
				return idents->at( i );
			}
		}

		return ident;
	}

	void testStrings()
	{
		// TODO
		UnicodeString str(L"TestString");

		UnicodeString str1 = str; str1 += L"123" + UnicodeString(L"456");
		UnicodeString str2 = str.c_str() + UnicodeString(L"123") + L"456";
		UnicodeString str3 = str + L"123" + UnicodeString(L"456");

		DebugAssert(str1 == L"TestString123456");
		DebugAssert(str2 == L"TestString123456");
		DebugAssert(str3 == L"TestString123456");

		RawByteString strb(str);
		RawByteString strb1 = strb; strb1 += "123456";

		DebugAssert(strb1.get() == "TestString123456");
	}
} // namespace bcb
