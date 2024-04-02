#pragma once

/*
	File system utils - mostly string related
*/

#include <Shlwapi.h>
#include <winioctl.h>
#include <fstream>
#include <iomanip>

#include "PathUtils.h"

namespace Commander
{
	struct FsUtils
	{
		//
		// Check file attributes
		//
		static bool checkPathAttrib( const std::wstring& path, DWORD attr )
		{
			WIN32_FIND_DATA wfd = { 0 };

			if( getFileInfo( path, wfd ) )
				return ( wfd.dwFileAttributes & attr ) == attr;

			return false;
		}

		//
		// Check file attributes
		//
		static bool checkAttrib( DWORD attributes, DWORD attrib )
		{
			return ( ( attributes & attrib ) == attrib );
		}

		//
		// Set compressed file attribute
		//
		static bool setCompressedAttrib( const std::wstring& path, bool enable = true )
		{
			HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(),
				FILE_READ_DATA | FILE_WRITE_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN,
				NULL );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				SHORT state = enable ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
				DWORD result = 0;

				int ret = DeviceIoControl( hFile, FSCTL_SET_COMPRESSION, &state, sizeof( state ), NULL, 0, &result, NULL );
				CloseHandle( hFile );

				return ret != 0;
			}

			return false;
		}

		//
		// Check path existence
		//
		static bool isPathExisting( const std::wstring& path )
		{
			if( !path.empty() )
			{
				auto testPath = path;

				// remove ending backslash even for eg. 'C:\'
				if( *testPath.rbegin() == L'\\' )
					testPath.pop_back();

				WIN32_FIND_DATA dummy;
				if( getFileInfo( testPath, dummy ) )
					return true;
			}

			return false;
		}

		//
		// Create disk iso image
		//
		static bool createIsoFile( const std::wstring& srcDrive, const std::wstring& dstFile )
		{
			bool res = false;
			const DWORD BUFSIZE = 512;
			ULONGLONG bufsize = BUFSIZE;

			ULONGLONG srcSize = 0ULL;
			getDiskFreeSpace( srcDrive, &srcSize );

			// Try incrementing the buffer read size by 512 for faster read times.
			// May have to cap at certain point depending on the size of the drive.
			while (bufsize < srcSize / BUFSIZE)
			bufsize *= 2;
			ULONGLONG bufWritten = 0ULL;
			std::wstring src = L"\\\\.\\";
			src += srcDrive.substr(0, 2);

			BYTE *buf = new BYTE[(UINT)srcSize];
			BYTE *ptr = buf;
			if (!buf) return false;
			ZeroMemory(buf, (size_t)srcSize);
			DWORD written = 0;

			// percentage will hold the progress incase one may need it.
			float increment = 100.0f / (float) srcSize;
			float percentage = 0.0f;

			HANDLE hSrc = CreateFile(src.c_str(),
					GENERIC_READ | GENERIC_WRITE, 
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					0,
					OPEN_EXISTING,
					0,
					0);

			if (hSrc != INVALID_HANDLE_VALUE)
			{
				printf("Reading source...\n");
				for (;;)
				{
					written = 0;
					ReadFile(hSrc, ptr, (DWORD)bufsize, &written, NULL);
					if (!written) break;
					ptr += written;
					bufWritten += written;
					percentage = (float)bufWritten * increment;
				}
				CloseHandle(hSrc);
			}

			if (bufWritten > 0)
			{
				// We have read some data so lets write to the ISO file.
				FILE *fle = _wfopen(PathUtils::getExtendedPath(dstFile).c_str(), L"wb");
				if (fle)
				{
					percentage = 100.0f;
					fwrite(buf, (size_t)bufWritten, 1, fle);
					fclose(fle);
					if (isPathExisting(dstFile))
					{
						res = true;
					}
				}
			}

			delete[] buf;

			return res;
		}

		//
		// Check whether path is actually a direcotry
		//
		static bool isPathDirectory( const std::wstring& path )
		{
			return checkPathAttrib( path, FILE_ATTRIBUTE_DIRECTORY );
		}

		//
		// Check whether path is actually a file
		//
		static bool isPathFile( const std::wstring& path )
		{
			return !isPathDirectory( path );
		}

		//
		// Create new directory (if one or more of the intermediate folders do not exist, they are created as well)
		//
		static bool makeDirectory( const std::wstring& dirName )
		{
			if( dirName.length() < 248 ) // MSDN: This string is of maximum length of 248 characters, including the terminating null character.
				return SHCreateDirectoryEx( NULL, dirName.c_str(), NULL ) == ERROR_SUCCESS;

			size_t idx = std::wstring::npos;

			// find last existing directory
			while( ( idx = dirName.find_last_of( L'\\', idx ) ) != std::wstring::npos )
			{
			//	OutputDebugString(dirName.substr(0, idx).c_str()); OutputDebugString(L"\n");

				HANDLE hFile = CreateFile( PathUtils::getExtendedPath( dirName.substr( 0, idx ) ).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
				if( hFile != INVALID_HANDLE_VALUE )
				{
					CloseHandle( hFile );
					idx++;
					break;
				}
				idx--;
			}

			while( ( idx = dirName.find_first_of( L'\\', idx ) ) != std::wstring::npos )
			{
			//	OutputDebugString(dirName.substr(0, idx).c_str()); OutputDebugString(L"\n");

				if( !CreateDirectory( PathUtils::getExtendedPath( dirName.substr( 0, idx ) ).c_str(), NULL ) )
					return false;
				idx++;
			}

			return !!CreateDirectory( PathUtils::getExtendedPath( dirName ).c_str(), NULL );
		}

		//
		// Create new file
		//
		static bool makeNewFile( const std::wstring& fileName, DWORD create = CREATE_NEW, DWORD attr = 0 )
		{
			HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_WRITE, NULL, NULL, create, attr, NULL );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				CloseHandle( hFile );
				return true;
			}

			return false;
		}

		//
		// Rename file or directory
		//
		static bool renameFile( const std::wstring& oldName, const std::wstring& newName )
		{
			return !!MoveFile( PathUtils::getExtendedPath( oldName ).c_str(), PathUtils::getExtendedPath( newName ).c_str() );
		}

		//
		// Get next unique non-existing file name (file.txt -> file (1).txt)
		//
		static std::wstring getNextFileName( const std::wstring& dirName, const std::wstring& fileName )
		{
			_ASSERTE( !fileName.empty() );

			std::wstring ext = PathUtils::getFileNameExtension( fileName );
			std::wstring newFileName = fileName;

			WIN32_FIND_DATA wfd = { 0 };
			while( getFileInfo( dirName + newFileName, wfd ) )
			{
				std::wstring name = PathUtils::stripFileExtension( newFileName );

				size_t idx = std::wstring::npos;
				if( !name.empty() && *name.rbegin() == L')' && ( idx = name.find_last_of( L'(' ) ) != std::wstring::npos )
				{
					UINT num = 0;
					std::wstring tmp = name.substr( idx + 1, name.length() - idx - 2 );
					if( swscanf( tmp.c_str(), L"%u", &num ) == 1 )
					{
						newFileName = name.substr( 0, idx + 1 ) + std::to_wstring( num + 1 ) + L")";

						if( !ext.empty() )
							newFileName += L"." + ext;

						continue;
					}
				}

				newFileName = name + L" (1)";

				if( !ext.empty() )
					newFileName += L"." + ext;
			}

			return newFileName;
		}

		//
		// Return only drive component from path
		//
		static std::wstring getPathRoot( const std::wstring& path )
		{
			TCHAR volName[MAX_WPATH];
			auto ret = GetVolumePathName( path.c_str(), volName, sizeof( volName ) );

			return std::wstring( volName );
		}

		//
		// Return path with proper casing: eg. c:\wINDows -> C:\Windows
		//
		static std::wstring getCanonicalPath( const std::wstring& path, DWORD flags = VOLUME_NAME_DOS | FILE_NAME_NORMALIZED )
		{
			HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				std::wstring pathOut;

				DWORD strLen = GetFinalPathNameByHandle( hFile, NULL, 0, flags );
				pathOut.resize( static_cast<size_t>( strLen ) );

				strLen = GetFinalPathNameByHandle( hFile, &pathOut[0], strLen, flags );
				pathOut.resize( static_cast<size_t>( strLen ) );

				CloseHandle( hFile );

				return pathOut;
			}

			return std::wstring();
		}

		//
		// Canonicalize path name and remove the "\\?\" prefix
		//
		static std::wstring canonicalizePathName( const std::wstring& path )
		{
			// get path name with proper casing
			std::wstring pathOut = PathUtils::addDelimiter( getCanonicalPath( path ) );

			return pathOut.empty() ? path : PathUtils::getFullPath( pathOut );
		}

		//
		// Convert file size to human readable format (1000000 -> 976 KB)
		//
		static std::wstring bytesToString( const ULONGLONG byteCount )
		{
#ifdef StrFormatByteSize
			TCHAR buff[64] = { 0 };
			std::wstring str = StrFormatByteSize( byteCount, buff, ARRAYSIZE( buff ) );
#else
			TCHAR *suff[] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB" };

			if (byteCount == 0)
				return std::wstring( L"0 " ) + std::wstring( suff[0] );

			ULONGLONG bytes = llabs( byteCount );
			int place = (int)floor( log( (double)bytes ) / log( 1024.0 ) );
			double num = (double)bytes / pow( 1024.0, place );
			double round = floor( num * 10.0 + 0.5 ) / 10.0;

			std::wostringstream strs;
			strs << round << L" " << suff[place];
			std::wstring str = strs.str();
#endif
			return str;
		}

		//
		// Convert file size to human readable format (1000000 -> 1 000 000)
		//
		static std::wstring bytesToString2( const ULONGLONG byteCount )
		{
			auto bytes = std::to_wstring( byteCount );

			std::wostringstream ss;
			for( size_t i = 1; i < bytes.length() + 1; ++i )
			{
				ss << bytes[i - 1];

				if( i < bytes.length() && ( ( bytes.length() - i ) % 3 ) == 0 )
				{
					ss << L" ";
				}
			}

			return ss.str();
		}

		//
		// Get reparse point type
		//
		void reparsePointType( DWORD type )
		{
			/*switch(type)
			{
			case IO_REPARSE_TAG_AF_UNIX:
			case IO_REPARSE_TAG_APPEXECLINK:
			case IO_REPARSE_TAG_CLOUD:
			case IO_REPARSE_TAG_CLOUD_1:
			case IO_REPARSE_TAG_CLOUD_2:
			case IO_REPARSE_TAG_CLOUD_3:
			case IO_REPARSE_TAG_CLOUD_4:
			case IO_REPARSE_TAG_CLOUD_5:
			case IO_REPARSE_TAG_CLOUD_6:
			case IO_REPARSE_TAG_CLOUD_7:
			case IO_REPARSE_TAG_CLOUD_8:
			case IO_REPARSE_TAG_CLOUD_9:
			case IO_REPARSE_TAG_CLOUD_A:
			case IO_REPARSE_TAG_CLOUD_B:
			case IO_REPARSE_TAG_CLOUD_C:
			case IO_REPARSE_TAG_CLOUD_D:
			case IO_REPARSE_TAG_CLOUD_E:
			case IO_REPARSE_TAG_CLOUD_F:
			//IO_REPARSE_TAG_CLOUD_MASK
			case IO_REPARSE_TAG_CSV:
			case IO_REPARSE_TAG_DEDUP:
			case IO_REPARSE_TAG_DFS:
			case IO_REPARSE_TAG_DFSR:
			case IO_REPARSE_TAG_FILE_PLACEHOLDER:
			case IO_REPARSE_TAG_GLOBAL_REPARSE:
			case IO_REPARSE_TAG_HSM:
			case IO_REPARSE_TAG_HSM2:
			case IO_REPARSE_TAG_MOUNT_POINT:
			case IO_REPARSE_TAG_NFS:
			case IO_REPARSE_TAG_ONEDRIVE:
			case IO_REPARSE_TAG_PROJFS:
			case IO_REPARSE_TAG_PROJFS_TOMBSTONE:
			case IO_REPARSE_TAG_SIS:
			case IO_REPARSE_TAG_STORAGE_SYNC:
			case IO_REPARSE_TAG_SYMLINK:
			case IO_REPARSE_TAG_UNHANDLED:
			case IO_REPARSE_TAG_WCI:
			case IO_REPARSE_TAG_WCI_1:
			case IO_REPARSE_TAG_WCI_LINK:
			case IO_REPARSE_TAG_WCI_LINK_1:
			case IO_REPARSE_TAG_WCI_TOMBSTONE:
			case IO_REPARSE_TAG_WIM:
			case IO_REPARSE_TAG_WOF:
			}*/
		}

		//
		// Get target path from a reparse point
		//
		static std::wstring getReparsePointTarget( const std::wstring& itemPath )
		{
			std::wstring targetDir = L"";

			DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
			DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;

			HANDLE hFile = CreateFile( itemPath.c_str(), FILE_READ_EA, share, 0, OPEN_EXISTING, flags, 0 );
			if( hFile != INVALID_HANDLE_VALUE )
			{
				DWORD size = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
				BYTE rdata[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
				size = ReadReparsePoint( hFile, rdata, size );

				CloseHandle( hFile );

				REPARSE_DATA_BUFFER *reparseData = reinterpret_cast<REPARSE_DATA_BUFFER*>( rdata );
				size_t idx = 0;

				if( !!size && IsReparseTagMicrosoft( reparseData->ReparseTag ) )
				{
					if( IsReparseTagJunction( reparseData ) )
						targetDir = reparseData->MountPointReparseBuffer.PathBuffer;

					if( IsReparseTagSymlink( reparseData ) )
						targetDir = reparseData->SymbolicLinkReparseBuffer.PathBuffer;

					// junctions generally use "\??\<drive>:\" - so remove the prefix
					if( ( idx = targetDir.find_first_of( L':' ) ) != std::wstring::npos )
						targetDir = targetDir.substr( idx - 1 );
				}
			}

			return targetDir;
		}

		//
		// Get actual amount of hardlinks for given file name
		//
		static DWORD getHardLinksCount( const std::wstring& path )
		{
			DWORD linksCount = 0;

			DWORD shareModes = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
			HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), 0, shareModes, NULL, OPEN_EXISTING, 0, NULL );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				BY_HANDLE_FILE_INFORMATION info = { 0 };

				if( GetFileInformationByHandle( hFile, &info ) )
				{
					linksCount = info.nNumberOfLinks;
				}

				CloseHandle( hFile );
			}

			return linksCount;
		}

		//
		// Get file size from 64-bit low and high parts
		//
		static ULONGLONG getFileSize( const WIN32_FIND_DATA *wfd )
		{
			ULARGE_INTEGER fSize = { wfd->nFileSizeLow, wfd->nFileSizeHigh };
			return fSize.QuadPart;
		}

		//
		// Get file or directory attributes as text
		//
		static std::wstring getFormatStrFileAttr( const WIN32_FIND_DATA *wfd )
		{
			std::wstring attrStr;
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_READONLY )
				attrStr += L"R";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN )
				attrStr += L"H";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM )
				attrStr += L"S";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )
				attrStr += L"A";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_DEVICE )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_NORMAL )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY )
				attrStr += L"T";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED )
				attrStr += L"C";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_OFFLINE )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED )
				attrStr += L"E";
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_INTEGRITY_STREAM )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL )
				attrStr += L""; // empty
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_NO_SCRUB_DATA )
				attrStr += L""; // empty
			
			return attrStr.length() ? attrStr : L"-";
		}

		//
		// Format file date from FILETIME struct
		//
		static std::wstring getFormatStrFileDate( const WIN32_FIND_DATA *wfd, bool convertToLocal = true )
		{
			// convert the last-write time to local time
			SYSTEMTIME stUtc, stLocal, *st = &stUtc;
			FileTimeToSystemTime( &wfd->ftLastWriteTime, &stUtc );

			if( convertToLocal )
			{
				SystemTimeToTzSpecificLocalTime( NULL, &stUtc, &stLocal );
				st = &stLocal;
			}

			std::wostringstream sstr;
			sstr << st->wDay << L"." << st->wMonth << L"." << st->wYear;

			return sstr.str();
		}

		//
		// Format file time from FILETIME struct
		//
		static std::wstring getFormatStrFileTime( const WIN32_FIND_DATA *wfd, bool convertToLocal = true )
		{
			SYSTEMTIME stUtc, stLocal, *st = &stUtc;
			FileTimeToSystemTime( &wfd->ftLastWriteTime, &stUtc );

			if( convertToLocal )
			{
				// convert the last-write time to local time
				SystemTimeToTzSpecificLocalTime( NULL, &stUtc, &stLocal );
				st = &stLocal;
			}

			std::wostringstream sstr;
			sstr << st->wHour << L":";
			sstr << std::setfill( L'0' );
			sstr << std::setw( 2 ) << st->wMinute << L":";
			sstr << std::setw( 2 ) << st->wSecond;

			return sstr.str();
		}

		//
		// Format file size to human readable format
		//
		static std::wstring getFormatStrFileSize( const WIN32_FIND_DATA *wfd, bool brief = true )
		{
			if( wfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				return L"DIR";
			else
				return brief ?
					bytesToString( getFileSize( wfd ) ) :
					bytesToString2( getFileSize( wfd ) );
		}

		//
		// Convert windows FILETIME to unix time
		//
		static LONGLONG fileTimeToUnixTime( FILETIME& ft )
		{
			// takes the last modified date
			LARGE_INTEGER date, adjust;
			date.LowPart = ft.dwLowDateTime;
			date.HighPart = ft.dwHighDateTime;

			// 100-nanoseconds = milliseconds * 10000
			adjust.QuadPart = 116444736000000000LL;

			// removes the diff between 1970 and 1601
			date.QuadPart -= adjust.QuadPart;

			// converts back from 100-nanoseconds to seconds
			return date.QuadPart / 10000000;
		}

		//
		// Convert unix time to windows FILETIME
		//
		static FILETIME unixTimeToFileTime( time_t t )
		{
			FILETIME ft = { 0 };
			const LONGLONG EPOCH_DIFF = 116444736000000000LL;
			auto time = Int32x32To64( t, 10000000 ) + EPOCH_DIFF;
			ft.dwLowDateTime = static_cast<DWORD>( time );
			ft.dwHighDateTime = time >> 32;

			return ft;
		}

		//
		// Convert Mac/HFS+ time to windows FILETIME
		//
		static FILETIME macTimeToFileTime( time_t t )
		{
			const LONGLONG MAC_DIFF = 2082844800LL;
			return unixTimeToFileTime( t - MAC_DIFF );
		}

		//
		// Check whether a directory is in list of items
		//
		static bool isDirectoryInList( const std::vector<std::wstring>& items )
		{
			for( const auto& item : items )
			{
				if( isPathDirectory( item ) )
					return true;
			}

			return false;
		}

		//
		// Format output string for panel properties and copy/move/delete dialog
		//
		static std::wstring getFormatStrItemsCount( int filesCount, int dirsCount, ULONGLONG fSize = 0ull, bool includeSize = false )
		{
			std::wostringstream sstr;
			std::wstring appendix;

			if( includeSize )
			{
				sstr << bytesToString( fSize ) << L" (" << bytesToString2( fSize ) << L" bytes) in ";
				appendix = L" selected";
			}

			switch( dirsCount )
			{
			case 0:
				break;
			case 1:
				sstr << dirsCount << appendix << L" directory" << ( filesCount ? L" and " : L"" );
				break;
			default:
				sstr << dirsCount << appendix << L" directories" << ( filesCount ? L" and " : L"" );
				break;
			}

			switch( filesCount )
			{
			case 0:
				break;
			case 1:
				sstr << filesCount << appendix << L" file";
				break;
			default:
				sstr << filesCount << appendix << L" files";
				break;
			}

			return sstr.str();
		}

		//
		// Format offset number from file size
		//
		static std::string getOffsetNumber( LARGE_INTEGER& fileSize, std::streamoff offset )
		{
			std::ostringstream sstr;

			if( fileSize.HighPart > 0xFFFF )
				sstr << std::hex << std::setfill( '0' ) << std::uppercase << std::setw( 4 ) << ( ( offset >> 48 ) & 0xFFFF ) << " ";
			if( fileSize.HighPart )
				sstr << std::hex << std::setfill( '0' ) << std::uppercase << std::setw( 4 ) << ( ( offset >> 32 ) & 0xFFFF ) << " ";
			if( fileSize.LowPart > 0xFFFF )
				sstr << std::hex << std::setfill( '0' ) << std::uppercase << std::setw( 4 ) << ( ( offset >> 16 ) & 0xFFFF ) << " ";

			sstr << std::hex << std::setfill( '0' ) << std::uppercase << std::setw( 4 ) << ( offset & 0xFFFF ) << ": ";

			return sstr.str();
		}

		//
		// Get disk free space as string
		//
		static std::wstring getStringDiskFreeSpace( ULONGLONG freeSpace )
		{
			std::wostringstream sstr;
			sstr << L"Free: " << bytesToString( freeSpace ) << L" (" << bytesToString2( freeSpace ) << L" bytes)";

			return sstr.str();
		}

		//
		// Get drive type as string
		//
		static std::wstring getStringDriveType( const std::wstring& drive )
		{
			switch( GetDriveType( drive.c_str() ) )
			{
			case DRIVE_NO_ROOT_DIR:
				return L"No Root";
			case DRIVE_REMOVABLE:
				return L"Removable";
			case DRIVE_FIXED:
				return L"Local Disk";
			case DRIVE_REMOTE:
				return L"Network Drive";
			case DRIVE_CDROM:
				return L"CD/DVD Drive";
			case DRIVE_RAMDISK:
				return L"Ramdisk";
			case DRIVE_UNKNOWN:
			default:
				return L"Unknown";
			}

			return L"";
		}

		//
		// Get volume info - mounted, hotplugged (USB), removable (CD-ROM, etc.)
		//
		static bool getHotplugInfo( const std::wstring& volName )
		{
			bool retVal = false;

			// deviceName should be lower-case without trailing backslash
			std::wstring deviceName = L"\\\\.\\" + StringUtils::convert2Lwr( volName );
			HANDLE hDevice = CreateFile( deviceName.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0 );
			if( hDevice != INVALID_HANDLE_VALUE )
			{
				//unsigned int flags = 0;
				DWORD returnedSize;
				STORAGE_HOTPLUG_INFO shi = { 0 };

				if( DeviceIoControl( hDevice, IOCTL_STORAGE_GET_HOTPLUG_INFO, 0, 0, &shi, sizeof( shi ), &returnedSize, NULL ) ) {
					if (shi.MediaRemovable) {
						// flags |= VolumeRemoveable;
					}
					if (shi.DeviceHotplug) {
						// flags |= VolumeHotplugable;
					}
				}

				DWORD mediaTypeSize = sizeof(GET_MEDIA_TYPES);
				GET_MEDIA_TYPES* mediaTypes = (GET_MEDIA_TYPES*) new char[mediaTypeSize];

				while( DeviceIoControl( hDevice, IOCTL_STORAGE_GET_MEDIA_TYPES_EX, 0, 0, mediaTypes, mediaTypeSize, &returnedSize, NULL ) == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER )
				{
					delete[](char*) mediaTypes;
					mediaTypeSize *= 2;
					mediaTypes = (GET_MEDIA_TYPES*) new char[mediaTypeSize];
				}

				if( mediaTypes->MediaInfoCount > 0 )
				{
					STORAGE_MEDIA_TYPE mediaType;
					DWORD characteristics = 0;
					// Supports: Disk, CD, DVD
					if( mediaTypes->DeviceType == FILE_DEVICE_DISK || mediaTypes->DeviceType == FILE_DEVICE_CD_ROM || mediaTypes->DeviceType == FILE_DEVICE_DVD ) {
						if( shi.MediaRemovable ) {
							characteristics = mediaTypes->MediaInfo[0].DeviceSpecific.RemovableDiskInfo.MediaCharacteristics;
							mediaType = mediaTypes->MediaInfo[0].DeviceSpecific.RemovableDiskInfo.MediaType;
						}
						else {
							characteristics = mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.MediaCharacteristics;
							mediaType = mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.MediaType;
						}
						bool mounted = false;
						if( characteristics & MEDIA_CURRENTLY_MOUNTED ) {
							mounted = true;
						}
						if( characteristics & ( MEDIA_READ_ONLY | MEDIA_READ_WRITE ) ) {
							//	flags |= VolumeReadable;
						}
						if( ( ( characteristics & MEDIA_READ_WRITE ) != 0 || ( characteristics & MEDIA_WRITE_ONCE) != 0 )
							&&( characteristics & MEDIA_WRITE_PROTECTED ) == 0 && ( characteristics & MEDIA_READ_ONLY ) == 0 ) {
							//	flags |= VolumeWriteable;
							retVal = mounted;
						}
						if( characteristics & MEDIA_ERASEABLE ) {
							//	flags |= VolumeEraseable;
						}
					}
				}

				delete[] (char*)mediaTypes;
				CloseHandle( hDevice );
			}

			return retVal;
		}

		//
		// Get system logical drives, return drives bitmask
		//
		static DWORD getDriveLettersList( std::vector<std::wstring>& driveLetters )
		{
			// individual bits represent drive letters starting with an 'A'
			auto currentDrives = GetLogicalDrives();

			std::wstring letter;
			for( size_t i = 0; i < sizeof( DWORD ) * 8; ++i )
			{
				if( ( ( currentDrives >> i ) & 1 ) == 1 )
				{
					letter = (WCHAR)( L'A' + i );
					letter += L":\\";

					driveLetters.push_back( letter );
				}
			}

			return currentDrives;
		}

		//
		// Get free space on disk
		//
		static ULONGLONG getDiskFreeSpace( const std::wstring& driveName, ULONGLONG *total = NULL )
		{
			ULARGE_INTEGER freeSpace = { 0 }, totalSpace = { 0 }, totalFreeSpace = { 0 };
			GetDiskFreeSpaceEx( driveName.c_str(), &freeSpace, &totalSpace, &totalFreeSpace );

			if( total )
				*total = totalSpace.QuadPart;

			return freeSpace.QuadPart;
		}

		//
		// Get win32 file data info
		//
		static bool getFileInfo( const std::wstring& fileName, WIN32_FIND_DATA& wfd )
		{
			HANDLE hFile = FindFirstFile( PathUtils::getExtendedPath( fileName ).c_str(), &wfd );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				FindClose( hFile );
				return true;
			}

			return false;
		}

		//
		// Try to read and detect UNICODE byte-order-marker for a given input stream
		//
		static StringUtils::EUtfBom readByteOrderMarker( std::istream& inStream, int& bomLen, bool rewindStream = true )
		{
			auto bom = StringUtils::EUtfBom::NOBOM;
			bomLen = 0;

			if( inStream.good() )
			{
				char buf[4] = { 0 };
				std::streamsize bytesRead = inStream.read( reinterpret_cast<char*>( buf ), sizeof( buf ) ).gcount();

				inStream.clear();

				bom = StringUtils::getByteOrderMarker( buf, static_cast<int>( bytesRead ), bomLen );

				inStream.seekg( rewindStream ? 0 : bomLen, std::ios::beg );
			}

			return bom;
		}

		//
		// Save text to a file in current system's (or given) ANSI codepage encoding
		//
		static bool saveFileAnsi( const std::wstring& fileName, const wchar_t *text, size_t len, UINT codepage = CP_ACP )
		{
			std::ofstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );

			if( fs.is_open() )
			{
				std::string outStr;

				if( StringUtils::convert2A( text, len, outStr, codepage ) )
					fs << outStr;
			}

			return fs.good();
		}

		//
		// Save text to a file in UTF-8 encoding
		//
		static bool saveFileUtf8( const std::wstring& fileName, const wchar_t *text, size_t len, bool includeBom = false )
		{
			std::ofstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );

			if( !fs.is_open() )
				return false;

			if( includeBom )
				fs.write( "\xEF\xBB\xBF", 3 );

			std::string outStr;

			if( StringUtils::convert2A( text, len, outStr ) )
				fs << outStr;

			return fs.good();
		}

		//
		// Save text to a file in UTF-16 encoding
		//
		static bool saveFileUtf16( const std::wstring& fileName, const wchar_t *text, size_t len, bool bigEndian = false )
		{
			std::ofstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );

			if( !fs.is_open() )
				return false;

			// always write BOM
			fs.write( bigEndian ? "\xFE\xFF" : "\xFF\xFE", sizeof( wchar_t ) );

			if( bigEndian )
			{
				wchar_t outCh;
				for( size_t i = 0; i < len; i++ )
				{
					outCh = ( ( ( text[i] >> 8 ) & 0x00FF ) | ( ( text[i] << 8 ) & 0xFF00 ) );
					fs.write( reinterpret_cast<char*>( &outCh ), sizeof( wchar_t ) );
				}
			}
			else
				fs.write( reinterpret_cast<const char*>( text ), len * sizeof( wchar_t ) );

			return fs.good();
		}

		//
		// Save text to a file in UTF-32 encoding
		//
		static bool saveFileUtf32( const std::wstring& fileName, const wchar_t *text, size_t len, bool bigEndian = false )
		{
			std::ofstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );

			if( !fs.is_open() )
				return false;

			// always write BOM
			fs.write( bigEndian ? "\0\0\xFE\xFF" : "\xFF\xFE\0\0", sizeof( char32_t ) );

			char32_t outCh;
			for( size_t i = 0; i < len; i++ )
			{
				outCh = text[i];

				if( outCh < HIGH_SURROGATE_START || outCh > LOW_SURROGATE_END )
				{
					if( bigEndian )
						outCh = ( ( ( outCh >> 24 ) & 0x000000FF ) | ( ( outCh >> 8 ) & 0x0000FF00 ) | ( ( outCh << 8 ) & 0x00FF0000 ) | ( ( outCh << 24 ) & 0xFF000000 ) );

					fs.write( reinterpret_cast<char*>( &outCh ), sizeof( char32_t ) );
				}
				else
				{
					if( ( i + 1 < len ) && IS_SURROGATE_PAIR( text[i], text[i + 1] ) )
					{
						outCh = ( text[i] << 10 ) + text[++i] - 0x35FDC00; // surrogate_to_utf32

						if( bigEndian )
							outCh = ( ( ( outCh >> 24 ) & 0x000000FF ) | ( ( outCh >> 8 ) & 0x0000FF00 ) | ( ( outCh << 8 ) & 0x00FF0000 ) | ( ( outCh << 24 ) & 0xFF000000 ) );

						fs.write( reinterpret_cast<char*>( &outCh ), sizeof( char32_t ) );
					}
					else
						fs.write( bigEndian ? "\xFD\xFF" : "\xFF\xFD", 2 ); // ERROR
				}
			}

			return fs.good();
		}

		//
		// Get disk info as string
		//
		static std::wstring getStringDiskInfo( const std::wstring& path )
		{
			TCHAR volumeName[MAX_PATH + 1] = { 0 };
			TCHAR fileSystemName[MAX_PATH + 1] = { 0 };
			DWORD serialNumber = 0;
			DWORD maxComponentLen = 0;
			DWORD fileSystemFlags = 0;

			auto drive = PathUtils::addDelimiter( getPathRoot( path ) );

			std::wostringstream sstr;
			if( GetVolumeInformation( drive.c_str(), volumeName, ARRAYSIZE( volumeName ),
				&serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, ARRAYSIZE( fileSystemName ) ) )
			{
				if( wcslen( volumeName ) )
					sstr << volumeName;
				else
					sstr << getStringDriveType( drive.c_str() );
			}

			sstr << L" (" << getPathRoot( path ) << L")";

			if( wcslen( fileSystemName ) )
				sstr << L"\r\n" << fileSystemName;

			ULONGLONG ulFreeSpace, ulTotalSpace;
			ulFreeSpace = getDiskFreeSpace( drive, &ulTotalSpace );

			sstr << L"\r\n" << bytesToString( ulFreeSpace ) << L" free";
			sstr << L"\r\n" << bytesToString( ulTotalSpace ) << L" total";

			return sstr.str();
		}

		//
		// Get logical disk volume name
		//
		static std::wstring getVolumeName( const std::wstring& path )
		{
			TCHAR volumeName[MAX_PATH + 1] = { 0 };

			// a trailing backslash is required
			if( GetVolumeInformation( PathUtils::addDelimiter( path ).c_str(), volumeName, MAX_PATH, NULL, NULL, NULL, NULL, 0 ) )
			{
				if( wcslen( volumeName ) )
					return std::wstring( volumeName );
			}

			return L"";
		}

		//
		// Get system temp directory location (returned string ends with a backslash)
		//
		static std::wstring getTempDirectory()
		{
			TCHAR tempPath[MAX_PATH + 1] = { 0 };
			GetTempPath( ARRAYSIZE( tempPath ), tempPath );

			return tempPath;
		}

		//
		// Get windows directory location
		//
		static std::wstring getSystemDirectory()
		{
			TCHAR winDir[_MAX_DIR] = { 0 };
			GetWindowsDirectory( winDir, _MAX_DIR );

			return winDir;
		}

		//
		// Find as closest existing directory to dirName as possible
		//
		static std::wstring getClosestExistingDir( const std::wstring& path )
		{
			auto dirName = PathUtils::stripDelimiter( path );

			if( isPathDirectory( dirName ) )
			{
				return path;
			}
			else if( isPathDirectory( PathUtils::stripDelimiter( getPathRoot( dirName ) ) ) )
			{
				return getPathRoot( dirName );
			}
			else
			{
				//return ShellUtils::getKnownFolderPath( FOLDERID_Documents );
				return PathUtils::addDelimiter( getPathRoot( getSystemDirectory() ) );
			}
		}

		//
		// Delete directory recursively
		//
		static bool deleteDirectory( const std::wstring& dirName, DWORD flags )
		{
			auto pathFind = PathUtils::addDelimiter( dirName );

			WIN32_FIND_DATA wfd = { 0 };

			// delete file system structure
			HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, flags );
			if( hFile != INVALID_HANDLE_VALUE )
			{
				do
				{
					if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
					{
						if( !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
							&& !PathUtils::isDirectoryDotted( wfd.cFileName )
							&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ))
						{
							deleteDirectory( pathFind + wfd.cFileName, flags );
						}
					}
					else
					{
						if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY ) == FILE_ATTRIBUTE_READONLY )
							SetFileAttributes( ( PathUtils::getExtendedPath( pathFind ) + wfd.cFileName ).c_str(), ( wfd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY ) );

						DeleteFile( ( PathUtils::getExtendedPath( pathFind ) + wfd.cFileName ).c_str() );
					}
				} while( FindNextFile( hFile, &wfd ) );

				FindClose( hFile );
			}

			return !!RemoveDirectory( dirName.c_str() );
		}

		// do not instantiate this class/struct
		FsUtils() = delete;
		FsUtils( FsUtils const& ) = delete;
		void operator=( FsUtils const& ) = delete;
	};
}