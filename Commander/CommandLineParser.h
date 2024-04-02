#pragma once

/*
	Commandline parser - currently parses only temporary directory parameter
*/

#include <fcntl.h>

namespace Commander
{
	// Commander's temporary data storage directory
	extern std::wstring g_tempPath;

	//
	// Simple commandline parser class
	//
	class CCommandLineParser
	{
	public:
		// ctor
		inline CCommandLineParser() : _cmdLineParams( nullptr )
		{
			if( initCommandLineParams() )
			{
				std::wstring path;
				if( getParamValue( L"tempdir", path ) )
				{
					// set global temp directory
					g_tempPath = PathUtils::addDelimiter( path );
				}
			}
		}

		// dtor
		inline ~CCommandLineParser()
		{
			// nothing yet
		}

	private:
		//
		// Initialize commandline parameters
		//
		inline bool initCommandLineParams()
		{
			LPCWSTR cmdLine = GetCommandLine();

			if( cmdLine )
			{
				// exclude the program name from commandline parameters
				if( *cmdLine == L'\"' )
				{
					_cmdLineParams = StrChr( cmdLine + 1, L'\"' );

					if( _cmdLineParams )
						_cmdLineParams++;
					else
						return false; // no unquotes - shouldn't happen
				}
				else
					_cmdLineParams = cmdLine;

				return true;
			}

			return false;
		}

		//
		// Get parameter value - should be in following format: L" -param=\"value\""
		// NOTE: quotes aren't mandatory
		inline bool getParamValue( LPCWSTR param, std::wstring& value )
		{
			_ASSERTE( _cmdLineParams != nullptr );

			PCWSTR p = _cmdLineParams;
			size_t len = wcslen( param );

			while( ( p = StrStrW( p, param ) ) != nullptr )
			{
				if( p - _cmdLineParams >= 2 // preceding ' ' and '-'
					&& *(p - 2) == L' ' && *(p - 1) == L'-'
					&& *(p + len) == L'=' && *(p + len + 1) != L'\0' )
				{
					p += len + 1;

					PCWSTR p2 = p + 1;

					if( *p == L'\"' ) // path is within quotation marks
					{
						while( *p2 != L'\"' && *p2 != L'\0' )
							p2++;

						if( *p2 == L'\0' )
							p2 = p; // bad path - no unquote found
						else
							p++; // skip quote
					}
					else
					{
						while( *p2 != L' ' && *p2 != L'\0' )
							p2++;
					}

					value.assign( p, p2 );

					return !value.empty();
				}
				else
					p++;
			}

			return false;
		}

	private:
		LPCWSTR _cmdLineParams;

	private:
		// do not assign nor copy construct this object
		// should be a singleton anyway..
		CCommandLineParser( CCommandLineParser const& ) = delete;
		void operator=( CCommandLineParser const& ) = delete;
	};
}
