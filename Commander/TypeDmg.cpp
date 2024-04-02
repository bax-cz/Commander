#include "stdafx.h"

#include "Commander.h"
#include "TypeDmg.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchDmg::CArchDmg()
	{
		_volume = nullptr;
	}

	CArchDmg::~CArchDmg()
	{
	}

	std::wstring CArchDmg::getTempFileName()
	{
		std::wstring path = PathUtils::addDelimiter( PathUtils::stripFileName( _fileName ) ) + L".";
		return path + PathUtils::stripFileExtension( PathUtils::stripPath( _fileName ) ) + L".unpack";
	}

	//
	// Open archive
	//
	bool CArchDmg::readEntries()
	{
		_tempFileName = getTempFileName();

		FILE *ftmp = _wfopen( PathUtils::getExtendedPath( _tempFileName ).c_str(), L"r+b" );
		AbstractFile *afOut = createAbstractFileFromFile( ftmp );

		if( !ftmp )
		{
			FILE *f = _wfopen( PathUtils::getExtendedPath( _fileName ).c_str(), L"rb" );
			if( f )
			{
				FsUtils::makeNewFile( _tempFileName.c_str(), CREATE_NEW, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY );
				ftmp = _wfopen( PathUtils::getExtendedPath( _tempFileName ).c_str(), L"r+b" );

				AbstractFile *af = createAbstractFileFromFile( f );
				afOut = createAbstractFileFromFile( ftmp );

				int result = extractDmg( af, afOut, -1 );

				free( af );
				fclose( f );
			}
			else
				return false;
		}

		bool retVal = false;

		_volume = openVolume( IOFuncFromAbstractFile( afOut ) );

		if( _volume )
		{
			HFSPlusCatalogRecord *record = getRecordFromPath( "/", _volume, NULL, NULL );

			readEntriesTree( ((HFSPlusCatalogFolder*)record)->folderID, L"" );

			closeVolume( _volume );

			retVal = true;
		}
		else
			_errorMessage = L"Unable to open HFS+ volume.";

		free( afOut );
		fclose( ftmp );

		return retVal;
	}

	void CArchDmg::readEntriesTree( HFSCatalogNodeID folderID, wchar_t *root )
	{
		CatalogRecordList *rootList = getFolderContents( folderID, _volume );
		CatalogRecordList *pList = rootList;

		EntryData entry = { 0 };
		FILETIME ftime;

		while( _pWorker->isRunning() && pList != NULL )
		{
			wcsncpy( entry.wfd.cFileName, root, ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );
			wcsncat( entry.wfd.cFileName, (wchar_t*)&pList->name.unicode, pList->name.length );

			entry.link.clear();

			if( wcslen( entry.wfd.cFileName ) )
			{
				if ( pList->record->recordType == kHFSPlusFolderRecord )
				{
					HFSPlusCatalogFolder *folder = (HFSPlusCatalogFolder*)pList->record;
					entry.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

					if( !wcsncmp( entry.wfd.cFileName, L".HFS+ Private Directory Data", 28 ) )
						entry.wfd.cFileName[28] = L'\0';

					ftime = FsUtils::macTimeToFileTime( folder->createDate );

					entry.wfd.nFileSizeLow = entry.wfd.nFileSizeHigh = 0;
					wcscat( entry.wfd.cFileName, L"\\" );

					readEntriesTree( folder->folderID, entry.wfd.cFileName );
				}
				else if (pList->record->recordType == kHFSPlusFileRecord)
				{
					HFSPlusCatalogFile *file = (HFSPlusCatalogFile*)pList->record;

					ftime = FsUtils::macTimeToFileTime( file->contentModDate );

					ULARGE_INTEGER fSize;
					fSize.QuadPart = file->dataFork.logicalSize;
					entry.wfd.nFileSizeLow = fSize.LowPart;
					entry.wfd.nFileSizeHigh = fSize.HighPart;

					if( file->userInfo.fileType == kHardLinkFileType )
					{
						// TODO: handle links
						entry.wfd.dwFileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
						//entry.link = getLinkTarget(pList->record, parentID, key, _volume);
					}
					else
						entry.wfd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
				}

				if( ((char*)pList->name.unicode)[0] == '.' )
					entry.wfd.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;

				entry.wfd.ftLastWriteTime = ftime;

				_dataEntries.push_back( entry );
			}
			pList = pList->next;
		}
		releaseCatalogRecordList( rootList );
	}

	void CArchDmg::extractFile( HFSPlusCatalogFile *file, const std::wstring& path )
	{
		FsUtils::makeDirectory( PathUtils::stripFileName( path ) );

		io_func *io;
		size_t curPosition;
		size_t bytesLeft;

		FILE *f = _wfopen( PathUtils::getExtendedPath( path ).c_str(), L"w+b");
		AbstractFile *afOut = createAbstractFileFromFile( f );

		if (file->permissions.ownerFlags & UF_COMPRESSED) {
			io = openHFSPlusCompressed(_volume, file);
			if (io == NULL) {
				hfs_panic("error opening file");
				free( afOut );
				fclose( f );
				return;
			}

			curPosition = 0;
			bytesLeft = ((HFSPlusCompressed *)io->data)->decmpfs->size;
		}
		else {
			io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord *)file, _volume);
			if (io == NULL) {
				hfs_panic("error opening file");
				free( afOut );
				fclose( f );
				return;
			}

			curPosition = 0;
			bytesLeft = file->dataFork.logicalSize;
		}
		while( _pWorker->isRunning() && bytesLeft > 0 )
		{
			if (bytesLeft > sizeof(_dataBuf))
			{
				if (!HFS_READ(io, curPosition, sizeof(_dataBuf), _dataBuf)) {
					hfs_panic("error reading");
				}

				_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, sizeof(_dataBuf) );
				if (afOut->write(afOut, _dataBuf, sizeof(_dataBuf)) != sizeof(_dataBuf)) {
					hfs_panic("error writing");
				}
				curPosition += sizeof(_dataBuf);
				bytesLeft -= sizeof(_dataBuf);
			}
			else
			{
				if (!HFS_READ(io, curPosition, bytesLeft, _dataBuf)) {
					hfs_panic("error reading");
				}

				_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, bytesLeft );
				if (afOut->write(afOut, _dataBuf, bytesLeft) != bytesLeft) {
					hfs_panic("error writing");
				}
				curPosition += bytesLeft;
				bytesLeft -= bytesLeft;
			}
		}

		HFS_CLOSE(io);

		free( afOut );
		fclose( f );

		FILETIME ftime = FsUtils::macTimeToFileTime( file->contentModDate );
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, 0, NULL );
		SetFileTime( hFile, &ftime, &ftime, &ftime );
		CloseHandle( hFile );

		SetFileAttributes( PathUtils::getExtendedPath( path ).c_str(), ( PathUtils::stripPath( path )[0] == L'.' ? FILE_ATTRIBUTE_HIDDEN : 0 ) );
	}

	void CArchDmg::extractEntries( HFSCatalogNodeID folderID, const std::wstring& root, const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		CatalogRecordList *rootList = getFolderContents( folderID, _volume );
		CatalogRecordList *pList = rootList;

		wchar_t fileNameTmp[MAX_PATH] = { 0 };

		while( _pWorker->isRunning() && pList != NULL )
		{
			wcsncpy( fileNameTmp, reinterpret_cast<wchar_t*>( &pList->name.unicode ), pList->name.length );
			fileNameTmp[pList->name.length] = L'\0';

			std::wstring outName = root + fileNameTmp;

			if ( pList->record->recordType == kHFSPlusFolderRecord )
			{
				extractEntries( ((HFSPlusCatalogFolder*)pList->record)->folderID, PathUtils::addDelimiter( outName ), entries, targetDir );
			}

			if( outName.length() && shouldExtractEntry( entries, outName ) )
			{
				if ( pList->record->recordType == kHFSPlusFolderRecord )
				{
					auto dirName = targetDir + outName;
					FsUtils::makeDirectory( dirName );

					auto tmpName = PathUtils::stripPath( PathUtils::stripDelimiter( dirName ) );
					DWORD attr = ( tmpName[0] == L'.' ? FILE_ATTRIBUTE_HIDDEN : 0 );
					SetFileAttributes( PathUtils::getExtendedPath( dirName ).c_str(), attr );

					HFSPlusCatalogFolder *folder = (HFSPlusCatalogFolder*)pList->record;
					FILETIME ftime = FsUtils::macTimeToFileTime( folder->createDate );
					HANDLE hFile = CreateFile( PathUtils::getExtendedPath( dirName ).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
					SetFileTime( hFile, &ftime, &ftime, &ftime );
					CloseHandle( hFile );
				}
				else if( pList->record->recordType == kHFSPlusFileRecord )
				{
					HFSPlusCatalogFile *file = (HFSPlusCatalogFile*)pList->record;

					_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );
					extractFile( file, PathUtils::getFullPath( targetDir + outName ) );
				}

			}
			pList = pList->next;
		}
		releaseCatalogRecordList( rootList );
	}

	//
	// Extract archive to given directory
	//
	/*bool CArchDmg::extractArchive(const std::wstring& targetDir)
	{
		// obsolete
		FILE *fin = _wfopen( PathUtils::getExtendedPath( _fileName ).c_str(), L"rb" );
		FILE *ftmp = tmpfile();

		AbstractFile *afIn = createAbstractFileFromFile( fin );
		AbstractFile *afOut = createAbstractFileFromFile( ftmp );

		if( !afOut )
		{
			PrintDebug("error: can't create tmp file");
			return false;
		}

		int result = extractDmg( afIn, afOut, -1 );
		if( !result )
		{
			PrintDebug( "error: the provided data was not a DMG file." );
		}

		Volume *volume = openVolume( IOFuncFromAbstractFile( afOut ) );
		HFSPlusCatalogRecord *record = getRecordFromPath( "/", volume, NULL, NULL );

		extractAllInFolder( ((HFSPlusCatalogFolder*)record)->folderID, volume );

		closeVolume( volume );

		free( afOut );

		fclose( ftmp );
		fclose( fin );

		return true;
	}*/

	//
	// Extract gziped file to given directory
	//
	bool CArchDmg::extractEntries(const std::vector<std::wstring>& entries, const std::wstring& targetDir)
	{
		_tempFileName = getTempFileName();

		FILE *ftmp = _wfopen( PathUtils::getExtendedPath( _tempFileName ).c_str(), L"r+b" );
		if( ftmp )
		{
			AbstractFile *afOut = createAbstractFileFromFile( ftmp );

			_volume = openVolume( IOFuncFromAbstractFile( afOut ) );
			HFSPlusCatalogRecord *record = getRecordFromPath( "/", _volume, NULL, NULL );

			extractEntries( ((HFSPlusCatalogFolder*)record)->folderID, L"", entries, targetDir );

			closeVolume( _volume );

			free( afOut );
			fclose( ftmp );

			return true;
		}

		return false;
	}
}
