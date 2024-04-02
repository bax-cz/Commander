#pragma once

/*
	Sorting utils - helper functions for sorting
*/

namespace Commander
{
	struct SortUtils
	{
		//
		// Sort alphabetically by name
		//
		static bool byName( const WIN32_FIND_DATA& first, const WIN32_FIND_DATA& second )
		{
			if( IsDir( first.dwFileAttributes ) && !IsDir( second.dwFileAttributes ) )
				return true;

			if( !IsDir( first.dwFileAttributes ) && IsDir( second.dwFileAttributes ) )
				return false;

			return StrCmpLogicalW( first.cFileName, second.cFileName ) < 0;
			/*std::wstring nameFirst = first.cFileName;
			std::replace( nameFirst.begin(), nameFirst.end(), L'_', L'/' ); // '/' ascii value is just after '.' and before '0-9'

			std::wstring nameSecond = second.cFileName;
			std::replace( nameSecond.begin(), nameSecond.end(), L'_', L'/' ); // ditto

			return _wcsicmp( nameFirst.c_str(), nameSecond.c_str() ) < 0;*/
		}

		//
		// Sort alphabetically by path
		//
		static bool byPath( const std::wstring& first, const std::wstring& second )
		{
			return _wcsicmp( first.c_str(), second.c_str() ) < 0;
		}

		//
		// Sort by file extension
		//
		static bool byExt( const WIN32_FIND_DATA& first, const WIN32_FIND_DATA& second )
		{
			static TCHAR fname1[MAX_PATH], fname2[MAX_PATH];

			fname1[0] = L'\0'; // first filename - swap filename and extension
			auto p = wcsrchr( first.cFileName, L'.' );
			if( p ) { // when extension dot is found, skip dot char and append bare filename
				wcscat( fname1, p + 1 ); wcscat( fname1, L"." );
				wcsncat( fname1, first.cFileName, ( p - first.cFileName ) );
			}
			else { // when no extension dot, add space before filename
				wcscat( fname1, L" " ); wcscat( fname1, first.cFileName );
			}

			fname2[0] = L'\0'; // second filename - ditto
			p = wcsrchr( second.cFileName, L'.' );
			if( p ) {
				wcscat( fname2, p + 1 ); wcscat( fname2, L"." );
				wcsncat( fname2, second.cFileName, ( p - second.cFileName ) );
			}
			else {
				wcscat( fname2, L" " ); wcscat( fname2, second.cFileName );
			}

			/*_wsplitpath( first.cFileName, NULL, NULL, fname, ext );
			std::wstring nameFirst = ( ext[0] ? ext + 1 : L" " ); // skip '.' in extension or add a space when no extension
			nameFirst += fname;

			_wsplitpath( second.cFileName, NULL, NULL, fname, ext );
			std::wstring nameSecond = ( ext[0] ? ext + 1 : L" " ); // ditto
			nameSecond += fname;*/

			return StrCmpLogicalW( fname1, fname2 ) < 0;
		}

		//
		// Sort by file time
		//
		static bool byTime( const WIN32_FIND_DATA& first, const WIN32_FIND_DATA& second )
		{
			ULARGE_INTEGER timeFirst  = { first.ftLastWriteTime.dwLowDateTime, first.ftLastWriteTime.dwHighDateTime };
			ULARGE_INTEGER timeSecond = { second.ftLastWriteTime.dwLowDateTime, second.ftLastWriteTime.dwHighDateTime };

			if( timeFirst.QuadPart == timeSecond.QuadPart )
				return byName( first, second );
			else
				return timeFirst.QuadPart < timeSecond.QuadPart;
		}

		//
		// Sort by file size
		//
		static bool bySize( const WIN32_FIND_DATA& first, const WIN32_FIND_DATA& second )
		{
			if( IsDir( first.dwFileAttributes ) )
			{
				if( IsDir( second.dwFileAttributes ) )
					return byName( first, second );
				else
					return true;
			}
			else
			{
				ULARGE_INTEGER sizeFirst  = { first.nFileSizeLow, first.nFileSizeHigh };
				ULARGE_INTEGER sizeSecond = { second.nFileSizeLow, second.nFileSizeHigh };

				if( sizeFirst.QuadPart == sizeSecond.QuadPart )
					return byName( first, second );
				else
					return sizeFirst.QuadPart < sizeSecond.QuadPart;
			}
		}

		//
		// Sort by file attributes
		//
		static bool byAttr( const WIN32_FIND_DATA& first, const WIN32_FIND_DATA& second )
		{
			if( first.dwFileAttributes == second.dwFileAttributes )
				return byName( first, second );
			else
				return first.dwFileAttributes < second.dwFileAttributes;
		}

		//
		// Sort by numerical value
		//
		static bool byNumber( const std::wstring& first, const std::wstring& second )
		{
			return std::stoi( first ) > std::stoi( second );
		}

		// do not instantiate this class/struct
		SortUtils() = delete;
		SortUtils( SortUtils const& ) = delete;
		void operator=( SortUtils const& ) = delete;
	};
}
