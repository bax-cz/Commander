#pragma once

/*
	Path utils - dealing with system path strings
*/

#include "StringUtils.h"

#ifndef PATH_LOCAL_PREFIX
# define PATH_LOCAL_PREFIX      L"\\\\?\\"
# define PATH_UNC_PREFIX        L"\\\\?\\UNC"
# define PATH_LOCAL_PREFIX_LEN  4
# define PATH_UNC_PREFIX_LEN    7
#endif

#ifndef MAX_WPATH
# define MAX_WPATH          32000
#endif

namespace Commander
{
	struct PathUtils
	{
		//
		// Return superior (double-dotted) directory definition
		//
		static std::wstring getDoubleDottedDirectory()
		{
			return L"..";
		}

		//
		// Return whether directory is current - dotted
		//
		static bool isDirectoryDotted( const WCHAR *subDir )
		{
			return subDir && subDir[0] == L'.' && subDir[1] == L'\0';
		}

		//
		// Return whether directory is superior - double-dotted
		//
		static bool isDirectoryDoubleDotted( const WCHAR *subDir )
		{
			return subDir && subDir[0] == L'.' && subDir[1] == L'.' && subDir[2] == L'\0';
		}

		//
		// Return path with trailing backslash
		//
		static std::wstring addDelimiter( const std::wstring& path, const wchar_t delim = L'\\' )
		{
			if( path.empty() || *path.rbegin() == delim )
				return path;
			else
				return std::wstring( path + L"\\" );
		}

		//
		// Return path without trailing backslash - for path longer than 3 wchars (e.g. L"C:\")
		//
		static std::wstring stripDelimiter( const std::wstring& path, const wchar_t delim = L'\\' )
		{
			if( path.length() > 3 && *path.rbegin() == delim && *(path.rbegin() + 1) != L':' )
				return path.substr( 0, path.length() - 1 );
			
			return path;
		}

		//
		// Return only filename with extension from path
		//
		static std::wstring stripPath( const std::wstring& path, const wchar_t delim = L'\\' )
		{
			auto idx = path.find_last_of( delim );
			return idx != std::wstring::npos ? path.substr( idx + 1 ) : path;
		}

		//
		// Return only path without file name
		//
		static std::wstring stripFileName( const std::wstring& path, const wchar_t delim = L'\\' )
		{
			auto idx = path.find_last_of( delim );
			return idx != std::wstring::npos ? path.substr( 0, idx ) : L"";
		}

		//
		// Return only path without file extension
		//
		static std::wstring stripFileExtension( const std::wstring& path )
		{
			auto idx = path.find_last_of( L'.' );
			return idx != std::wstring::npos ? path.substr( 0, idx ) : path;
		}

		//
		// Unify delimiters to windows (or unix) default
		//
		static void unifyDelimiters( std::wstring& path, bool winFormat = true )
		{
			if( winFormat )
				std::replace( path.begin(), path.end(), L'/', L'\\' );
			else // unix format
				std::replace( path.begin(), path.end(), L'\\', L'/' );
		}

		//
		// Return superior directory title from given path
		//
		static std::wstring getParentDirectory( const std::wstring& path, const wchar_t delim = L'\\' )
		{
			auto dirName = stripDelimiter( path );
			auto idx = dirName.find_last_of( delim );

			return ( idx == std::wstring::npos ? dirName : dirName.substr( idx + 1 ) );
		}

		//
		// Return only extension from filename
		//
		static std::wstring getFileNameExtension( const std::wstring& path )
		{
			auto idx = path.find_last_of( L'.' );
			return idx != std::wstring::npos ? path.substr( idx + 1 ) : L"";
		}

		//
		// Get item filename without extension index
		//
		static int getFileNameExtensionIndex( const std::wstring& fileName )
		{
			return static_cast<int>( fileName.find_last_of( L'.' ) );
		}

		//
		// Return superior directory label
		//
		static std::wstring getLabelFromPath( const std::wstring& path )
		{
			auto dirLabel = getParentDirectory( path );

			if( dirLabel.empty() )
				dirLabel = addDelimiter( path );

			return dirLabel;
		}

		//
		// Get path that can exceed the MAX_PATH limit
		//
		static std::wstring getExtendedPath( const std::wstring& path )
		{
			std::wstring pathOut;

			if( path[0] == L'\\' ) // UNC path
			{
				if( path.length() > 2 && path[1] == L'\\' && path[2] != L'?' )
				{
					pathOut += PATH_UNC_PREFIX;
					pathOut += path.substr( 1 );
				}
				else
					pathOut = path;
			}
			else
			{
				pathOut += PATH_LOCAL_PREFIX;
				pathOut += path;
			}

			return pathOut;
		}

		//
		// Check whether a path is extended - starts with "\\?\"
		//
		static bool isExtendedPath( const std::wstring& path )
		{
			if( path.length() > PATH_LOCAL_PREFIX_LEN &&
				!wmemcmp( &path[0], PATH_LOCAL_PREFIX, PATH_LOCAL_PREFIX_LEN ) )
			{
				return true;
			}

			return false;
		}

		//
		// Check whether a path is UNC path
		//
		static bool isUncPath( const std::wstring& path )
		{
			if( path.length() > 2 )
			{
				if( path.length() > PATH_UNC_PREFIX_LEN &&
					!wmemcmp( &path[0], PATH_UNC_PREFIX, PATH_UNC_PREFIX_LEN ) )
				{
					return true;
				}

				if( path[0] == L'\\' &&
					path[1] == L'\\' &&
					path[2] != L'?' )
				{
					return true;
				}
			}

			return false;
		}

		//
		// Expand full path from relative including "." and ".." names
		//
		static std::wstring getFullPath( const std::wstring& path )
		{
			std::wstring fullPath;
			fullPath.resize( MAX_WPATH );

			auto len = GetFullPathName( getExtendedPath( path ).c_str(), MAX_WPATH, &fullPath[0], NULL );
			if( len != 0 )
			{
				fullPath.resize( len );

				if( isExtendedPath( fullPath ) )
				{
					if( isExtendedPath( path ) && isUncPath( path ) ) // UNC path
					{
						fullPath[PATH_UNC_PREFIX_LEN - 1] = L'\\';
						return fullPath.substr( PATH_UNC_PREFIX_LEN - 1 );
					}
					else
						return fullPath.substr( PATH_LOCAL_PREFIX_LEN );
				}

				return fullPath;
			}

			return path;
		}

		//
		// Get link name and target from such strings: "filename -> some/path/target"
		//
		static std::wstring getLinkTarget( const std::wstring& fileName, std::wstring& link )
		{
			auto idx = fileName.find( L"->" );

			if( idx != std::wstring::npos )
			{
				auto name = fileName.substr( 0, idx );
				link = fileName.substr( idx + 2 );

				StringUtils::rTrim( name );
				StringUtils::lTrim( link );

				return name;
			}

			link.clear();

			return fileName;
		}

		//
		// Combine relative paths
		//
		static std::wstring combinePath( const std::wstring& path1, const std::wstring& path2 )
		{
			/*std::wstring result;
			result.resize( MAX_PATH );

			PathCombine( &result[0], path1.c_str(), path2.c_str() );*/

			return getFullPath( addDelimiter( path1 ) + path2 );
		}

		//
		// Sanitize invalid filename characters
		//
		static wchar_t sanitizeFileNameChar( wchar_t ch )
		{
			if( wcschr( L"<>:\"/\\|?*", ch ) )
				return L'_';

			return ch;
		}

		//
		// Sanitize invalid filenames used by the system
		//
		static std::wstring sanitizeReservedNames( const std::wstring& fileName )
		{
			// the list of reserved device names (COM# and LPT# is separate)
			const wchar_t *reserved[] = { L"CON", L"PRN", L"AUX", L"NUL" };

			// do not allow the names even with extension
			auto fname = stripFileExtension( fileName );
			bool found = false;

			// three letter device names from the reserved list
			if( fname.length() == 3 )
			{
				for( size_t i = 0; i < ARRAYSIZE( reserved ); i++ )
				{
					if( _wcsicmp( fname.c_str(), reserved[i] ) == 0 )
						found = true;
				}
			} // and the remaing fourth letter (COM[1-9] and LPT[1-9]) device names
			else if( fname.length() == 4 && fname[3] != L'0' && isdigit( fname[3] ) )
			{
				auto fnUpper = StringUtils::convert2Upr( fname  );
				if( wmemcmp( fnUpper.c_str(), L"COM", 3 ) == 0 || wmemcmp( fnUpper.c_str(), L"LPT", 3 ) == 0 )
					found = true;
			}

			if( found )
			{
				// insert an underscore character at the end of the filename
				fname += L"_";

				auto ext = getFileNameExtension( fileName );
				if( !ext.empty() )
					fname += L'.' + ext;

				return fname;
			}

			return fileName;
		}

		//
		// Sanitize filename
		//
		static std::wstring sanitizeFileName( const std::wstring& fileName )
		{
			std::wstring fname = fileName;

			for( auto& ch : fname )
			{
				ch = sanitizeFileNameChar( ch );
			}

			return sanitizeReservedNames( fname );
		}

		//
		// Check whether path contains any wildcard characters
		//
		static bool isWild( const std::wstring& path )
		{
			return
				path.find_first_of( L'*' ) != std::wstring::npos ||
				path.find_first_of( L'?' ) != std::wstring::npos;
		}

		//
		// Prepare string for wild character matching
		//
		static std::wstring preMatchWild( const std::wstring& pat )
		{
			auto patterns = StringUtils::split( pat, L";" );
			std::wstring patternOut;

			for( auto it = patterns.begin(); it != patterns.end(); ++it )
			{
				if( it != patterns.begin() && !it->empty() )
					patternOut += L"|";

				if( *it == L"*.*" || *it == L"*" ) {
					return L"*";
				}
				else if( StringUtils::endsWith( *it, L"*.*" ) ) {
					patternOut += it->substr( 0, it->length() - 2 );
				}
				else
					patternOut += *it;
			}

			return StringUtils::convert2Lwr( patternOut );

			/*std::wstring pattern = pat;

			if( pat == L"*.*" ) {
				pattern = L"*";
			}
			else if( StringUtils::endsWith( pat, L"*.*" ) ) {
				pattern = pat.substr( 0, pat.length() - 2 );
			}

			return StringUtils::convert2Lwr( pattern );*/
		}

		//
		// Wild character filename matching - TODO: optimization
		//
		static bool matchFileNameWild( const std::wstring& pat, const std::wstring& fName, const std::wstring& separator = L"|" )
		{
			auto hasExt = ( fName.find_last_of( L'.' ) != std::wstring::npos );
			auto patterns = StringUtils::split( pat, separator );

			for( const auto& pattern : patterns )
			{
				bool ret = false;

				if( !hasExt )
				{
					if( StringUtils::endsWith( pattern, L".*" ) ) {
						ret = matchWild( pattern.substr( 0, pattern.length() - 2 ), fName );
					}
					else if( StringUtils::endsWith( pattern, L"." ) ) {
						ret = matchWild( pattern.substr( 0, pattern.length() - 1 ), fName );
					}
				}

				if( ret || matchWild( pattern, fName ) )
					return true;
			}

			return false;

			/*auto hasExt = ( fName.find_last_of( L'.' ) != std::wstring::npos );
			if( !hasExt )
			{
				if( StringUtils::endsWith( pat, L".*" ) ) {
					return matchWild( pat.substr( 0, pat.length() - 2 ), fName );
				}
				else if( StringUtils::endsWith( pat, L"." ) ) {
					return matchWild( pat.substr( 0, pat.length() - 1 ), fName );
				}
			}

			return matchWild( pat, fName );*/
		}

		//
		// Wild character matching routine - from wxWidgets
		//
		static bool matchWild( const std::wstring& pat, const std::wstring& text, bool dot_special = false )
		{
			/**
			 * Written By Douglas A. Lewis <dalewis@cs.Buffalo.EDU>
			 *
			 * The match procedure is public domain code (from ircII's reg.c)
			 * but modified to suit our tastes (RN: No "%" syntax I guess)
			 */
			if (text.empty())
			{
				/* Match if both are empty. */
				return pat.empty();
			}

			const TCHAR *m = pat.c_str(),
			*n = text.c_str(),
			*ma = nullptr,
			*na = nullptr;
			int just = 0,
			acount = 0,
			count = 0;

			if (dot_special && (*n == L'.'))
			{
				/* Never match so that hidden Unix files
				 * are never found. */
				return false;
			}

			for (;;)
			{
				if (*m == L'*')
				{
					ma = ++m;
					na = n;
					just = 1;
					acount = count;
				}
				else if (*m == L'?')
				{
					m++;
					if (!*n++)
						return false;
				}
				else
				{
					if (*m == L'\\')
					{
						m++;
						/* Quoting "nothing" is a bad thing */
						if (!*m)
							return false;
					}
					if (!*m)
					{
						/*
						* If we are out of both strings or we just
						* saw a wildcard, then we can say we have a
						* match
						*/
						if (!*n)
							return true;
						if (just)
							return true;
						just = 0;
						goto not_matched;
					}
					/*
					* We could check for *n == NULL at this point, but
					* since it's more common to have a character there,
					* check to see if they match first (m and n) and
					* then if they don't match, THEN we can check for
					* the NULL of n
					*/
					just = 0;
					if (*m == *n)
					{
						m++;
						count++;
						n++;
					}
					else
					{

						not_matched:

						/*
						* If there are no more characters in the
						* string, but we still need to find another
						* character (*m != NULL), then it will be
						* impossible to match it
						*/
						if (!*n)
							return false;

						if (ma)
						{
							m = ma;
							n = ++na;
							count = acount;
						}
						else
							return false;
					}
				}
			}
		}


		// do not instantiate this class/struct
		PathUtils() = delete;
		PathUtils( PathUtils const& ) = delete;
		void operator=( PathUtils const& ) = delete;
	};
}