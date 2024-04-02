#include "stdafx.h"

#include "Commander.h"
#include "Archiver.h"

#include "FileSystemUtils.h"
#include "IconUtils.h" 

namespace Commander
{
	//
	// Constructor/destructor
	//
	CArchiver::CArchiver()
		: _pDirs( nullptr )
		, _pFiles( nullptr )
		, _pWorker( nullptr )
		, _action( Overwrite )
	{
	}

	CArchiver::~CArchiver()
	{
	}

	//
	// Initialize archiver
	//
	void CArchiver::init( const std::wstring& fileName, std::vector<std::shared_ptr<EntryData>> *pFiles, std::vector<std::shared_ptr<EntryData>> *pDirs, CBackgroundWorker *pWorker, EExtractAction action )
	{
		_fileName = fileName;
		_pWorker = pWorker;
		_pDirs = pDirs;
		_pFiles = pFiles;
		_action = action;
	}

	//
	// Calculate size of directory in archive
	//
	bool CArchiver::calculateDirectorySize( const std::wstring& dirName, ULONGLONG *dirSize )
	{
		std::wstring localDir = _localDirectory + PathUtils::addDelimiter( dirName );

		for( const auto& entry : _dataEntries )
		{
			if( ( entry.wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != FILE_ATTRIBUTE_DIRECTORY )
			{
				if( wcsncmp( entry.wfd.cFileName, localDir.c_str(), localDir.length() ) == 0 )
				{
					*dirSize += FsUtils::getFileSize( &entry.wfd );
				}
			}
		}

		return true;
	}

	//
	// Should this entry be extracted
	//
	bool CArchiver::shouldExtractEntry( const std::vector<std::wstring>& entries, std::wstring& entryName )
	{
		if( !entryName.empty() )
		{
			if( entries.empty() || entries[0] == _fileName )
				return true;

			auto fullPath = PathUtils::addDelimiter( _fileName ) + entryName;

			auto pred = [&fullPath]( const std::wstring& name ) {
				return ( fullPath == name ) || ( StringUtils::startsWith( fullPath, PathUtils::addDelimiter( name ) ) );
			};

			if( std::find_if( entries.begin(), entries.end(), pred ) != entries.end() )
			{
				auto idx = entries[0].find_last_of( L'\\' ) - _fileName.length();
				entryName = entryName.substr( idx );

				return true;
			}
		}

		return false;
	}

	//
	// Add file entries only
	//
	void CArchiver::addEntryFile( EntryData& entry, const std::wstring& fileName )
	{
		if( fileName.length() && ( entry.wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != FILE_ATTRIBUTE_DIRECTORY )
		{
			auto data = std::make_shared<EntryData>( entry ); // TODO: add path to EntryData

			data->imgSystem = CDiskReader::getIconIndex( fileName.c_str(), FILE_ATTRIBUTE_NORMAL );
			wcsncpy( data->wfd.cFileName, fileName.c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

			data->size = FsUtils::getFormatStrFileSize( &data->wfd );
			data->date = FsUtils::getFormatStrFileDate( &data->wfd );
			data->time = FsUtils::getFormatStrFileTime( &data->wfd );
			data->attr = FsUtils::getFormatStrFileAttr( &data->wfd );

			_pFiles->push_back( data );
		}
	}

	//
	// Add directory entries only
	//
	void CArchiver::addEntryDirectory( EntryData& entry, const std::wstring& dirName, bool attrValid )
	{
		_ASSERTE( !dirName.empty() );

		auto pred = [&dirName]( const std::shared_ptr<EntryData> data ) { return dirName == data->wfd.cFileName; };
		auto it = std::find_if( _pDirs->begin(), _pDirs->end(), pred );
		bool found = it != _pDirs->end();

		if( !found )
		{
			auto data = std::make_shared<EntryData>( entry ); // TODO: add path to EntryData

			data->wfd.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
			data->imgSystem = IconUtils::getFromPathWithAttribIndex( L" ", data->wfd.dwFileAttributes );
			wcsncpy( data->wfd.cFileName, dirName.c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

			data->size = FsUtils::getFormatStrFileSize( &data->wfd );
			data->date = FsUtils::getFormatStrFileDate( &data->wfd );
			data->time = FsUtils::getFormatStrFileTime( &data->wfd );
			data->attr = FsUtils::getFormatStrFileAttr( &data->wfd );

			_pDirs->push_back( data );
		}
		else if( attrValid )
		{
			// update directory attributes
			(*it)->wfd.dwFileAttributes = entry.wfd.dwFileAttributes;
			(*it)->wfd.ftLastWriteTime = entry.wfd.ftLastWriteTime;

			(*it)->date = FsUtils::getFormatStrFileDate( &(*it)->wfd );
			(*it)->time = FsUtils::getFormatStrFileTime( &(*it)->wfd );
			(*it)->attr = FsUtils::getFormatStrFileAttr( &(*it)->wfd );
		}
	}

	//
	// Add parent (double dotted) directory entry
	//
	void CArchiver::addParentDirEntry( const WIN32_FIND_DATA& wfd )
	{
		EntryData entry = { wfd };

		entry.wfd = wfd;

		FILETIME ft;
		FileTimeToLocalFileTime( &entry.wfd.ftLastWriteTime, &ft );
//FileTimeToSystemTime 
//SystemTimeToTzSpecificLocalTime 
//SystemTimeToFileTime 
		entry.wfd.ftLastWriteTime = ft;

		addEntryDirectory( entry, PathUtils::getDoubleDottedDirectory() );
	}

	//
	// Read in-archive disk tree into memory
	//
	void CArchiver::readLocalPath( const std::wstring& srcDir )
	{
		_localDirectory = srcDir.substr( _fileName.length() + 1 );

		for( auto& entry : _dataEntries )
		{
			std::wstring entryName = entry.wfd.cFileName;
			
			if( entryName.length() > _localDirectory.length() && StringUtils::startsWith( entryName, _localDirectory ) )
			{
				auto idxBeg = _localDirectory.length();
				auto idxEnd = entryName.find_first_of( L'\\', ( entryName[0] == L'\\' ? idxBeg + 1 : idxBeg ) );

				auto len = ( idxEnd != std::wstring::npos ? idxEnd - idxBeg : idxEnd );

				if( IsDir( entry.wfd.dwFileAttributes ) )
					addEntryDirectory( entry, entryName.substr( idxBeg, len ) );
				else if( idxEnd != std::wstring::npos )
					addEntryDirectory( entry, entryName.substr( idxBeg, len ), false );
				else
					addEntryFile( entry, entryName.substr( idxBeg, len ) );
			}
		}
	}
}
