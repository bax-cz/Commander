#pragma once

/*
	Logger lite object
*/

#include <fcntl.h>

namespace Commander
{
	// Commander's temporary data storage directory
	extern std::wstring g_tempPath;

	//
	// Simple logging class
	//
	class CLogLite
	{
	public:
		inline CLogLite()
		{
			// get current date/time
			SYSTEMTIME st; GetLocalTime( &st );

			// create log filename
			wchar_t logFileName[MAX_PATH];
			wsprintf( logFileName, L"_fc_%4d%02d%02d_%02d%02d%02d.%.3d.log",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );

			// create temporary directory if it doesn't exist yet
			_wmkdir( g_tempPath.c_str() );

			// create log file
			_logFile = _wfopen( ( g_tempPath + logFileName ).c_str(), L"wt" );

			_ASSERTE( _logFile != nullptr );

			// set utf8 mode
			_setmode( fileno( _logFile ), _O_U8TEXT );

			// get product name and its version
			std::wstring product, version;
			SysUtils::getProductAndVersion( product, version );

			// log date/time to the file
			fwprintf( _logFile, L"-- %ls %ls started: %d/%d/%d %d:%02d:%02d.%.3d\n",
				product.c_str(), version.c_str(), st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
		}

		inline ~CLogLite()
		{
			_ASSERTE( _logFile != nullptr );

			// log date/time to the file
			SYSTEMTIME st; GetLocalTime( &st );
			std::lock_guard<std::mutex> lock( _mutex );
			fwprintf( _logFile, L"-- Application ended: %d/%d/%d %d:%02d:%02d.%.3d\n",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );

			// close log file
			fclose( _logFile );
			_logFile = nullptr;
		}

		inline void write( const wchar_t *str ) // write data to log file
		{
			_ASSERTE( _logFile != nullptr );

			// add timestamp to at the start of each log line
			SYSTEMTIME st; GetLocalTime( &st );
			std::lock_guard<std::mutex> lock( _mutex );
			fwprintf( _logFile, L"[%d:%02d:%02d.%.3d]%ls",
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, str );
		}

		inline void flush() // flush log file to disk
		{
			_ASSERTE( _logFile != nullptr );

			std::lock_guard<std::mutex> lock( _mutex );
			fflush( _logFile );
		}

	private:
		FILE *_logFile;
		std::mutex _mutex;
	};
}
