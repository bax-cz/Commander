#pragma once

/*
	Borland C++ Builder string types, constants and helper classes/functions
	NOTE: This is just bare minimum implementation to get WinScp stuff compiled!
*/

#include <string>
#include <vector>
#include <mutex>

#define DebugAssert(p) _ASSERTE(p)
#define DebugCheck(p) (p)
#define DebugFail() __noop

#define DebugAlwaysTrue(p) (p)
#define DebugAlwaysFalse(p) (p)
#define DebugNotNull(p) (p)
#define TraceInitPtr(p) (p)
#define TraceInitStr(p) (p)
#define DebugUsedParam(p) ((&p) == (&p))
#define DebugUsedArg(p)

#define AppLog(x) OutputDebugString(x)
#define STRARRAY(vals) StrArray<UnicodeString> vals
#define FMTLOAD(I, F) fmtLoadStr(I, STRARRAY(F))
#define LENOF(x) ( (sizeof((x))) / (sizeof(*(x))))
#define FLAGSET(SET, FLAG) (((SET) & (FLAG)) == (FLAG))
#define FLAGCLEAR(SET, FLAG) (((SET) & (FLAG)) == 0)
#define FLAGMASK(ENABLE, FLAG) ((ENABLE) ? (FLAG) : 0)

#ifdef LoadStr /* defined by FastCopy */
#undef LoadStr
#endif

namespace bcb
{
	/* Forward declarations */
	class AnsiString;    // TODO: implement move-ctors
	class UTF8String;
	class RawByteString;
	class UnicodeString;

	/* Constants */
	extern const UnicodeString EmptyString;

	extern const wchar_t NormalizedFingerprintSeparator;
	extern const wchar_t *EmptyStr;
	extern const wchar_t *sLineBreak;

	constexpr unsigned int qaYes = 0x00000001;
	constexpr unsigned int qaNo = 0x00000004;
	constexpr unsigned int qaOK = 0x00000008;
	constexpr unsigned int qaCancel = 0x00000010;
	constexpr unsigned int qaYesToAll = 0x00000020;
	constexpr unsigned int qaNoToAll = 0x00000040;
	constexpr unsigned int qaAbort = 0x00000080;
	constexpr unsigned int qaRetry = 0x00000100;

	extern const int SshPortNumber;
	extern const int FtpPortNumber;
	extern const int FtpsImplicitPortNumber;
	extern const int HTTPPortNumber;
	extern const int HTTPSPortNumber;
	extern const int TelnetPortNumber;
	extern const int DefaultSendBuf;
	extern const int ProxyPortNumber;
	extern const int SFTPMinVersion;
	extern const int SFTPMaxVersion;
	extern const UINT GUIUpdateInterval;

	extern const int MinsPerHour;
	extern const int MSecsPerSec;
	extern const int SecsPerDay;

	/* Types aliases */
	using TStrings = std::vector<UnicodeString>;
	using TStream = std::ifstream;
	using TDateTime = ULONGLONG;
	using TNotifyEvent = void(*)(void*);
	using TCriticalSection = std::mutex;

	/* Enumeration types */
	enum TEOLType { eolLF /* \n */, eolCRLF /* \r\n */, eolCR /* \r */ };
	enum TDSTMode { dstmWin =  0, dstmUnix = 1/* adjust UTC time to Windows "bug"*/, dstmKeep = 2 };

	/* Ansi string */
	class AnsiString
	{
	public:
		inline AnsiString() {}
		inline AnsiString( const char *str ) : _str( str ) {}
		inline AnsiString( const char *str, size_t len ) : _str( str, len ) {}
		inline AnsiString( const std::string& str ) : _str( str ) {}
		AnsiString( const UnicodeString& str );

	public:
		AnsiString SubString( size_t off, size_t count = std::string::npos ) const;

	public:
		inline bool IsEmpty() const { return _str.empty(); }
		inline size_t Length() const { return _str.length(); }
		inline void SetLength( size_t newLen ) { _str.resize( newLen ); }
		inline char *c_str() const { return const_cast<char*>( &_str[0] ); }
		inline std::string& get() { return _str; }

	private:
		std::string _str;
	};

	/* Unicode string */
	class UnicodeString
	{
	public:
		inline UnicodeString() {}
		inline UnicodeString( const wchar_t *str ) : _str( str ) {}
		inline UnicodeString( const std::wstring& str ) : _str( str ) {}
		UnicodeString( const char *str );
		UnicodeString( const std::string& str );
		UnicodeString( const AnsiString& str );
		UnicodeString( const UTF8String& str );
		UnicodeString( const RawByteString& str );

	public:
		int Pos( const wchar_t ch ) const;
		int Pos( const wchar_t *str ) const;
		UnicodeString Trim() const;
		UnicodeString TrimRight() const;
		UnicodeString SubString( size_t off, size_t count = std::wstring::npos ) const;
		int LastDelimiter( const wchar_t *str ) const;
		void Insert( const wchar_t *str, size_t off );
		void Delete( size_t off, size_t count );

	public:
		inline bool IsEmpty() const { return _str.empty(); }
		inline size_t Length() const { return _str.length(); }
		inline void reserve( size_t size ) { _str.reserve( size ); }
		inline const wchar_t *c_str() const  { return _str.c_str(); }
		inline std::wstring& get() { return _str; }

		friend UnicodeString operator+( const wchar_t *s1, const UnicodeString& s2 ) { std::wstring str = s1; return str.append( s2.c_str() ); }
		inline UnicodeString operator+( const wchar_t ch ) { return _str + ch; }
		inline UnicodeString operator+( const wchar_t *str ) { return _str + str; }
		inline UnicodeString operator+( const UnicodeString& str ) { return _str + const_cast<UnicodeString&>( str ).get(); }
		inline UnicodeString operator+=( const wchar_t ch ) { return _str.append( 1, ch ); }
		inline UnicodeString operator+=( UnicodeString& str ) { return _str.append( str.get() ); }
		friend bool operator<( const UnicodeString& s1, const UnicodeString& s2 ) { return wcscmp( s1.c_str(), s2.c_str() ) > 0; }
		inline bool operator!=( const wchar_t *str ) const { return _str != str; }
		inline bool operator==( const wchar_t *str ) const { return _str == str; }
		inline bool operator==( const UnicodeString& str ) const { return _str == const_cast<UnicodeString&>( str ).get(); }
		inline wchar_t operator[]( size_t off ) const { _ASSERTE( off > 0 ); return _str.at( off - 1 ); }

	private:
		std::wstring _str;
	};

	/* Utf8 string */
	class UTF8String
	{
	public:
		inline UTF8String() {}
		inline UTF8String( const char *str ) : _str( str ) {}
		inline UTF8String( const char *str, size_t len ) : _str( str, len ) {}
		inline UTF8String( const std::string& str ) : _str( str ) {}
		UTF8String( const UnicodeString& str );
		UTF8String( const RawByteString& str );

	public:
		UTF8String SubString( size_t off, size_t count = std::string::npos ) const;

	public:
		inline bool IsEmpty() const { return _str.empty(); }
		inline size_t Length() const { return _str.length(); }
		inline const char *c_str() const  { return _str.c_str(); }
		inline std::string& get() { return _str; }

	private:
		std::string _str;
	};

	/* Raw byte string */
	class RawByteString
	{
	public:
		inline RawByteString() {}
		inline RawByteString( const char *str, size_t len ) : _str( str, len ) {}
		inline RawByteString( const std::string& str ) : _str( str ) {}
		inline RawByteString( const AnsiString& str ) : _str( const_cast<AnsiString&>( str ).get() ) {}
		inline RawByteString( const UTF8String& str ) : _str( const_cast<UTF8String&>( str ).get() ) {}
		RawByteString( const wchar_t *str );
		RawByteString( const UnicodeString& str );

	public:
		RawByteString SubString( size_t off, size_t count = std::string::npos ) const;

	public:
		inline bool IsEmpty() const { return _str.empty(); }
		inline size_t Length() const { return _str.length(); }
		inline void SetLength( size_t newLen ) { _str.resize( newLen ); }
		inline char *c_str() { return &_str[0]; }
		inline std::string& get() { return _str; }

		inline RawByteString operator+=( const char ch ) { return _str.append( 1, ch ); }
		inline RawByteString operator+=( const char *str ) { return _str.append( str ); }
		inline RawByteString operator+=( RawByteString& str ) { return _str.append( str.get() ); }

	private:
		std::string _str;
	};

	/* Dynamic string array (up to 5 arguments) */
	template<typename T>
	class StrArray
	{
	public:
		inline StrArray( T arg0 ) { arr = new T[cnt = 1]; arr[0] = arg0; }
		inline StrArray( T arg0, T arg1 ) { arr = new T[cnt = 2]; arr[0] = arg0; arr[1] = arg1; }
		inline StrArray( T arg0, T arg1, T arg2 ) { arr = new T[cnt = 3]; arr[0] = arg0; arr[1] = arg1; arr[2] = arg2; }
		inline StrArray( T arg0, T arg1, T arg2, T arg3 ) { arr = new T[cnt = 4]; arr[0] = arg0; arr[1] = arg1; arr[2] = arg2; arr[3] = arg3; }
		inline StrArray( T arg0, T arg1, T arg2, T arg3, T arg4 ) { arr = new T[cnt = 5]; arr[0] = arg0; arr[1] = arg1; arr[2] = arg2; arr[3] = arg3; arr[4] = arg4; }

		inline ~StrArray() { delete[] arr; }

		inline T& at( int i ) { return arr[i]; }
		inline int size() { return cnt; }

	private:
		T *arr;
		int cnt;
	};

	/* Exceptions defitions */
	class Exception : public std::exception
	{
	public:
		inline Exception( UnicodeString& msg ) : std::exception( AnsiString( msg ).c_str() ) {}
		inline Exception( const wchar_t *msg ) : std::exception( AnsiString( msg ).c_str() ) {}
		inline ~Exception() {}

		AnsiString Message;
	};

	class ExtException : public std::exception
	{
	public:
		inline ExtException( UnicodeString& msg, int err ) : std::exception( AnsiString( msg ).c_str(), err ) {}
		inline ExtException( UnicodeString& msg, UnicodeString& err ) : std::exception( AnsiString( msg ).c_str() ) { Message = err; }
		inline ~ExtException() {}

		AnsiString Message;
	};

	class EOSExtException : public std::exception
	{
	public:
		inline EOSExtException( UnicodeString& msg, int err ) : std::exception( AnsiString( msg ).c_str(), err ) {}
		inline ~EOSExtException() {}
	};

	class ESshFatal : public std::exception
	{
	public:
		inline ESshFatal( const char *msg, const char *err ) : std::exception( AnsiString( msg ).c_str() ) { _err = err; _msg = msg; }
		inline ~ESshFatal() {}
	private:
		std::string _err;
		std::string _msg;
	};

	/* Guard helper */
	class TGuard
	{
	public:
		TGuard( TCriticalSection *criticalSection );
		~TGuard();

	private:
		TCriticalSection *_criticalSection;
	};

	/* Unguard helper */
	class TUnguard
	{
	public:
		TUnguard( TCriticalSection *criticalSection );
		~TUnguard();

	private:
		TCriticalSection *_criticalSection;
	};

	/* File stream reader */
	class TFileBuffer
	{
	public:
		inline TFileBuffer() { Data = nullptr; }
		inline ~TFileBuffer() {}

	public:
		inline void Reset() { /* dummy */ }
		inline DWORD LoadStream( TStream *fs, size_t len, bool dummy = false ) {
			/*if( fs && fs->is_open() ) {
				return static_cast<DWORD>( fs->read( Data, len ).gcount() );
			}*/ return 0;
		}

	public:
		char *Data;
	};

	/* Helper functions */
	ULONGLONG Now();
	void Abort();
	bool FileExists( UnicodeString& fileName );
	bool SameStr( const UnicodeString& str1, const UnicodeString& str2 );
	bool SameText( const UnicodeString& str1, const UnicodeString& str2 );
	bool SameChecksum( const UnicodeString& sum1, const UnicodeString& sum2, bool base64 );
	bool StartsStr( const UnicodeString& str, UnicodeString& text );
	bool StartsText( const UnicodeString& str, UnicodeString& text );
	bool CutToken( UnicodeString& str, UnicodeString& token );
	bool ContainsStr( UnicodeString& str, const UnicodeString& subStr ); // case sensitive search
	bool ContainsText( UnicodeString& str, const wchar_t *subText ); // case insensitive search
	bool IsNumber( UnicodeString& str );
	bool TryStrToInt( UnicodeString& str, int& outVal );
	bool IsHex( wchar_t ch );
	void Shred( AnsiString& str );
	void PackStr( AnsiString& str );
	uintmax_t StrToInt64( UnicodeString& str );
	AnsiString PuttyStr( const UnicodeString& str );
	UnicodeString IntToStr( int num );
	UnicodeString LoadStr( UINT strId );
	UnicodeString fmtLoadStr( UINT strId, StrArray<UnicodeString>& vals );
	UnicodeString IncludeTrailingBackslash( UnicodeString& str );
	UnicodeString ExpandEnvironmentVariables( const UnicodeString& str );
	UnicodeString ExtractFileName( const UnicodeString& path );
	UnicodeString DeleteChar( UnicodeString& str, wchar_t ch );
	UnicodeString CutToChar( UnicodeString& str, wchar_t ch, bool trim );
	UnicodeString RightStr( UnicodeString& str, size_t len );
	UnicodeString ReplaceStr( const UnicodeString& str, const wchar_t *from, const wchar_t *to );
	UnicodeString StripPathQuotes( UnicodeString& path );
	UnicodeString MainInstructions( const UnicodeString& str );
	UnicodeString NormalizeString( const UnicodeString& str );
	UnicodeString AnsiToString( const RawByteString& str );
	UnicodeString AnsiToString( const char *str, size_t len = std::string::npos );
	UnicodeString UTF8ToString( const RawByteString& str );
	UnicodeString UTF8ToString( const char *str, size_t len = std::string::npos );
	UnicodeString BytesToHex( const RawByteString& str );
	UnicodeString BytesToHex( const unsigned char *buf, size_t len );
	UnicodeString EncodeBase64( const char *str, size_t len );
	UnicodeString Base64ToUrlSafe( UnicodeString& str );
	UnicodeString EncodeStrToBase64( const RawByteString& str );
	RawByteString DecodeBase64ToStr( const UnicodeString& str );
	UnicodeString MD5ToUrlSafe( UnicodeString& str );
	UnicodeString FindIdent( const UnicodeString& ident, TStrings *idents );
	bool SameIdent( const UnicodeString& ident1, const UnicodeString& ident2 );
}
