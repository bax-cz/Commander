#pragma once

/*
	System utils - helper functions to make life easier
*/

namespace Commander
{
	struct SysUtils
	{
		//
		// Get the product information from resource
		//
		static bool getProductAndVersion( std::wstring& strProductName, std::wstring& strProductVersion )
		{
			// get the filename of the executable containing the version resource
			TCHAR szFileName[MAX_PATH + 1] = { 0 };
			if( GetModuleFileName( NULL, szFileName, MAX_PATH ) == 0 )
			{
				PrintDebug( "GetModuleFileName failed with error %d\n", GetLastError() );
				return false;
			}

			// allocate a block of memory for the version info
			DWORD dummy;
			DWORD dwSize = GetFileVersionInfoSize( szFileName, &dummy );
			if( dwSize == 0 )
			{
				PrintDebug( "GetFileVersionInfoSize failed with error %d\n", GetLastError() );
				return false;
			}

			std::vector<BYTE> data( dwSize );

			// load the version info
			if( !GetFileVersionInfo( szFileName, NULL, dwSize, &data[0] ) )
			{
				PrintDebug( "GetFileVersionInfo failed with error %d\n", GetLastError() );
				return false;
			}

			// get the name and version strings
			LPVOID pvProductName = NULL;
			unsigned int iProductNameLen = 0;

			LPVOID pvProductVersion = NULL;
			unsigned int iProductVersionLen = 0;

			// replace "000904b0" with the language ID of your resources
			if( !VerQueryValue( &data[0], _T("\\StringFileInfo\\000904b0\\ProductName" ), &pvProductName, &iProductNameLen ) ||
				!VerQueryValue( &data[0], _T("\\StringFileInfo\\000904b0\\ProductVersion" ), &pvProductVersion, &iProductVersionLen ) )
			{
				PrintDebug( "Can't obtain ProductName and ProductVersion from resources\n" );
				return false;
			}

			// copy information into wstrings without ending zeroes
			strProductName.assign( (LPCTSTR)pvProductName, iProductNameLen - 1 );
			strProductVersion.assign( (LPCTSTR)pvProductVersion, iProductVersionLen - 1 );

			return true;
		}

		//
		// Get the system platform string
		//
		static std::wstring getPlatformName()
		{
			static std::wstring platform;

		#if defined( _M_X64 ) || defined( _M_AMD64 )
			platform = L"x64";
		#elif defined( _M_IX86 ) || defined( __i386__ )
			platform = L"x86";
		#elif defined( _M_ARM )
			platform = L"ARM";
		#else
			platform = L"Undefined";
		#endif

			return platform;
		}

		//
		// Translate the system error message (from GetLastError() function) to a meaningful string
		//
		static std::wstring getErrorMessage( DWORD errorId, HMODULE hModule = NULL )
		{
			std::wstring msg = L"No error";

			if( errorId != 0 )
			{
				msg = L"Unknown error";

				TCHAR buff[MAX_WPATH] = L"";
				DWORD ret = FormatMessage(
					FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
					( hModule ? FORMAT_MESSAGE_FROM_HMODULE : 0 ), // retrieve message from specified DLL
					hModule,
					errorId,
					MAKELANGID( LANG_NEUTRAL, SUBLANG_SYS_DEFAULT ),
					(LPTSTR)buff,
					MAX_WPATH - 1,
					NULL );

				if( ret != 0 )
				{
					msg = buff;
				}
			}

			std::wostringstream sstr;
			sstr << L"Error: (" << errorId << L") " << msg;

			return sstr.str();
		}

		// do not instantiate this class/struct
		SysUtils() = delete;
		SysUtils( SysUtils const& ) = delete;
		void operator=( SysUtils const& ) = delete;
	};
}
