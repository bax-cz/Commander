#ifndef INC_DEFINEMENTS_H
#define INC_DEFINEMENTS_H

#include <sstream>
#include <string>
#include <set>
#include <time.h>
#include <assert.h>
#include <stdlib.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
#ifndef WIN32
#define WIN32
#endif
#elif defined(unix) || defined(__unix) || defined(__unix__)
#ifndef unix
#define unix
#endif
#define override
#endif

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#if _MSC_VER < 1500
#define override
#endif
#else
#include <errno.h>  // needed for errno
#include <string.h> // needed for strerror
#define Sleep sleep
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef int   BOOL;
typedef unsigned int UINT;
typedef unsigned int *PUINT;
typedef char TCHAR;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef unsigned int DWORD;
typedef unsigned short USHORT;
typedef unsigned int ULONG;
#endif

#ifndef VERIFY
#define VERIFY
#endif

#ifndef ASSERT
#define ASSERT assert
#endif

// definements for unicode support
#if defined _UNICODE || defined UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef UNICODE
#define UNICODE
#endif

typedef std::wstring tstring;
typedef std::wostream tostream;
typedef std::wofstream tofstream;
typedef std::wostringstream tostringstream;
typedef std::wistringstream tistringstream;
typedef std::wstringstream tstringstream;
typedef std::wstreambuf tstreambuf;

#define tcin  wcin
#define tcout wcout
#define tcerr wcerr
#define tclog wclog
#define tcstoul wcstoul
#define tcstoll wcstoll
#define tcschr wcschr
#define tcsncmp wcsncmp
#define tisdigit iswdigit
#if _MSC_VER >= 1500
#define tsprintf(buffer, numberOfChars, format, ...) swprintf_s(buffer, numberOfChars, format, __VA_ARGS__)
#else
#define tsprintf(buffer, numberOfChars, format, ...) swprintf(buffer, format, __VA_ARGS__)
#endif
#define tcsncpy wcsncpy
#define ttol wcstol

#ifndef _T
#define _T(x) L ## x
#endif
#else
typedef std::string tstring;
typedef std::ostream tostream;
typedef std::ofstream tofstream;
typedef std::ostringstream tostringstream;
typedef std::istringstream tistringstream;
typedef std::stringstream tstringstream;
typedef std::streambuf tstreambuf;

#define tcin  cin
#define tcout cout
#define tcerr cerr
#define tclog clog
#define tcstoul strtoul
#define tcschr strchr
#define tcsncmp strncmp
#define tisdigit isdigit
#if _MSC_VER >= 1500
#define tsprintf(buffer, numberOfChars, format, ...) sprintf_s(buffer, numberOfChars, format, __VA_ARGS__)
#else
#define tsprintf(buffer, numberOfChars, format, ...) sprintf(buffer, format, __VA_ARGS__)
#endif
#define tcsncpy strncpy
#define ttol strtol

#ifndef _T
#define _T
#endif
#endif

namespace nsHelper
{
#ifdef WIN32
	/// Class with static functions to get information about an error.
	class CError
	{
	public:
		static DWORD GetLastError() { return ::GetLastError(); }

		static tstring GetErrorDescription(int iErrorCode = GetLastError())
		{
			LPVOID lpMsgBuf = NULL;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, iErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				reinterpret_cast<LPTSTR>(&lpMsgBuf), 0, NULL);
			tostringstream sstrErrorDescription;
			sstrErrorDescription << L"Error: (" << iErrorCode << L") ";
			if (lpMsgBuf)
			{
				sstrErrorDescription << reinterpret_cast<LPTSTR>(lpMsgBuf);
				LocalFree(lpMsgBuf);
			}
			return sstrErrorDescription.str();
		}
	};
#else
	/// Class with static functions to get information about an error.
	class CError
	{
	public:
		static DWORD GetLastError() { return errno; }

		static tstring GetErrorDescription(int iErrorCode = GetLastError())
		{
			return strerror(iErrorCode);
		}
	};
#endif

	/// Class with static functions to do string conversions.
	class CCnv
	{
	public:
		static long TStringToLong(const tstring& strIn)
		{
			TCHAR *pStopString = 0;
#ifdef _UNICODE
			return wcstol(&*strIn.begin(), &pStopString, 10);
#else
			return strtol(&*strIn.begin(), &pStopString, 10);
#endif
		}

		static long long TStringToLongLong(const tstring& strIn)
		{
			TCHAR *pStopString = 0;
#ifdef _UNICODE
			return wcstoll(&*strIn.begin(), &pStopString, 10);
#else
			return strtoll(&*strIn.begin(), &pStopString, 10);
#endif
		}
#ifdef WIN32
		static tstring MakeLongPath(const tstring& path)
		{
			tstring pathOut;
			if (path[0] == L'\\') // UNC path
			{
				if (path.length() > 2 && path[1] == L'\\' && path[2] != L'?')
				{
					pathOut += L"\\\\?\\UNC";
					pathOut += path.substr(1);
				}
				else
					pathOut = path;
			}
			else
			{
				pathOut += L"\\\\?\\";
				pathOut += path;
			}

			return pathOut;
		}
#endif
		static std::string ConvertToString(const tstring& strIn, bool utf8 = false)
		{
			std::string strOut;
			return ConvertToString(strIn, strOut, utf8);
		}

		static std::string& ConvertToString(const tstring& strIn, std::string& strOut, bool utf8)
		{
#ifdef _UNICODE
			if (strIn.empty())
			{
				strOut.clear();
			}
			else
			{
# ifdef WIN32
				int len = WideCharToMultiByte(utf8 ? CP_UTF8 : CP_ACP, 0, &strIn[0], (int)strIn.length(), NULL, 0, NULL, NULL);
				strOut.resize(len);
				WideCharToMultiByte(utf8 ? CP_UTF8 : CP_ACP, 0, &strIn[0], (int)strIn.length(), &strOut[0], len, NULL, NULL);
# else
				strOut.resize(strIn.size());
				wcstombs(&*strOut.begin(), strIn.c_str(), strOut.size());
# endif
			}
#else
			strOut = strIn;
#endif
			return strOut;
		}

		static tstring ConvertToTString(const char *szIn, bool utf8 = false)
		{
			tstring strOut;
			return ConvertToTString(szIn, -1, strOut, utf8);
		}

		static tstring& ConvertToTString(const char *szIn, int lenIn, tstring& strOut, bool utf8)
		{
#ifdef _UNICODE
			if (*szIn == '\0')
			{
				strOut.clear();
			}
			else
			{
# ifdef WIN32
				int len = MultiByteToWideChar(utf8 ? CP_UTF8 : CP_ACP, 0, szIn, lenIn, NULL, 0);
				strOut.resize(len - 1); // no need for ending '\0' in std::string
				MultiByteToWideChar(utf8 ? CP_UTF8 : CP_ACP, 0, szIn, lenIn, &strOut[0], len - 1);
# else
				strOut.resize(strlen(szIn));
				mbstowcs(&*strOut.begin(), szIn, strOut.size());
# endif
			}
#else
			strOut = szIn;
#endif
			return strOut;
		}
	};

	/// Base class for implementing the notification stuff.
	/// @remarks Inherit public (instead of private) because it wouldn't compile under Dev-C++
	template <typename T, typename T2> class CObserverPatternBase : public std::set<T>
	{
	public:
		typedef typename std::set<T> base_type;
		typedef typename std::set<T>::iterator iterator;

		~CObserverPatternBase()
		{
			for (iterator it = base_type::begin(); it != base_type::end(); it = base_type::begin())
				(*it)->Detach(static_cast<T2>(this));
		}

		bool Attach(T p)
		{
			if (find(p) != base_type::end())
				return false;
			insert(p);
			p->Attach(static_cast<T2>(this));
			return true;
		}
		bool Detach(T p)
		{
			if (find(p) == base_type::end())
				return false;
			erase(p);
			p->Detach(static_cast<T2>(this));
			return true;
		}
	};


	/// @brief Calculates elapsed CPU time.
	/// Is useful for calculating transfer rates.
	class CTimer
	{
	public:
		CTimer() : m_dfStart(clock()) {}

		/// Restarts the timer.
		void Restart() { m_dfStart = clock(); }

		/// Get the elapsed time in seconds.
		double GetElapsedTime() const
		{
			return (static_cast<double>(clock()) - m_dfStart) / CLOCKS_PER_SEC;
		}

	private:
		double m_dfStart; ///< elapsed CPU time for process (start of measurement)
	};
}

#endif // INC_DEFINEMENTS_H
