#pragma once

/*
	CDataManager - manages panel's data
*/

#include <Lm.h>
#include <memory>

#include "ReaderTypes.h"
#include "FileSystemUtils.h"
#include "SortUtils.h"

namespace Commander
{
	class CDataManager
	{
	public:
		CDataManager();
		~CDataManager();

	public:
		void init( HWND hWndNotify );
		bool readFromPath( std::wstring& path );

		const std::wstring& getCurrentDirectory() const { return _currentDirectory; }
		EReaderStatus getStatus() const { return _spReader->getStatus(); }
		std::wstring getLastErrorMessage() const { return _spReader->getErrorMessage(); }
		std::shared_ptr<CBaseDataProvider> getDataProvider() { return _spDataProvider; }

		std::wstring getRootPath() const { return _spReader->getRoot(); }
		std::wstring getPropertiesText() const { return _propertiesString; }
		std::wstring getEntryPathFull( const std::wstring& entryName ) const;
		std::wstring getEntryPathFull( int entryIndex ) const;

		EEntryType getEntryType( int entryIndex ) const;
		const TCHAR *getEntryName( int entryIndex ) const;
		const TCHAR *getEntrySize( int entryIndex ) const;
		const TCHAR *getEntryDate( int entryIndex ) const;
		const TCHAR *getEntryTime( int entryIndex ) const;
		const TCHAR *getEntryAttr( int entryIndex ) const;

		int getEntryIndex( const std::wstring& entryName ) const  { return _spReader->getEntryIndex( entryName ); }
		int getEntryImageIndex( int entryIndex ) const;

		void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal );
		//void loadAssociatedlIcons( int cacheFrom, int cacheTo ) const;
		void updateProperties( int entryIndex );

		ULONGLONG getFreeSpace() const  { return _spReader->getFreeSpace(); }
		ULONGLONG getMarkedEntriesSize() const { return _markedEntriesSize; }
		std::vector<int>& getMarkedEntries() { return _markedEntries; }
		void setMarkedEntries( const std::vector<int>& entries ) { _markedEntries = entries; }

		std::vector<std::shared_ptr<EntryData>>& getDirEntries() { return _spReader->getDirEntries(); }
		std::vector<std::shared_ptr<EntryData>>& getFileEntries() { return _spReader->getFileEntries(); }

		size_t getEntryCount() const { return _spReader->getEntryCount(); }
		void countEntries( const std::vector<int>& markedEntries, int& filesCnt, int& dirsCnt );
		bool renameEntry( int entryIndex, const std::wstring& fileName );

		void setFtpData( std::unique_ptr<CFtpReader::FtpData> data ) { _spFtpData = std::move( data ); }
		void setSshData( std::unique_ptr<CSshReader::SshData> data ) { _spSshData = std::move( data ); }

		CFtpReader::SessionData& getFtpSession() { return _spFtpData->session; }
		bcb::TSessionData& getSshSession() { return _spSshData->session; }

	public:
		bool isEntryFile( int entryIndex );
		bool isEntryFixed( int entryIndex ); // unable to rename
		bool isEntryHardLink( int entryIndex );
		bool isEntryAttrSet( int entryIndex, DWORD attrib );

		bool isInReadOnlyMode() const;
		bool isInDiskMode()     const { return _currentMode == EReadMode::Disk;    }
		bool isInArchiveMode()  const { return _currentMode == EReadMode::Archive; }
		bool isInNetworkMode()  const { return _currentMode == EReadMode::Network; }
		bool isInFtpMode()      const { return _currentMode == EReadMode::Ftp;     }
		bool isInPuttyMode()    const { return _currentMode == EReadMode::Putty;   }
		bool isInRegedMode()    const { return _currentMode == EReadMode::Reged;   }

		bool isPathStillValid( const std::wstring& path ) const { return _spReader->isPathValid( path ); }
		bool isOverMaxPath() const { return _spReader->isOverMaxPath(); }

	public:
		void sortEntries( ESortMode mode ) { _sortMode = mode; _spReader->setSortMode( mode ); _spReader->sortEntries(); }

		bool isSortedByName()      const { return _sortMode == ESortMode::ByName; }
		bool isSortedByExtension() const { return _sortMode == ESortMode::ByExt;  }
		bool isSortedByTime()      const { return _sortMode == ESortMode::ByTime; }
		bool isSortedBySize()      const { return _sortMode == ESortMode::BySize; }
		bool isSortedByAttr()      const { return _sortMode == ESortMode::ByAttr; }

	private:
		std::shared_ptr<CBaseReader> createReader( EReadMode mode );
		std::wstring getPropertiesSingle( int entryIndex );

	private:
		std::shared_ptr<CBaseReader> _spDiskReader;
		std::shared_ptr<CBaseReader> _spReader;

		std::shared_ptr<CBaseDataProvider> _spDataProvider;

		std::shared_ptr<CFtpReader::FtpData> _spFtpData;
		std::shared_ptr<CSshReader::SshData> _spSshData;

		EReadMode _currentMode;
		ESortMode _sortMode;

		ULONGLONG _markedEntriesSize;
		std::vector<int> _markedEntries;

		int _markedDirsCount;
		int _markedFilesCount;

		HWND _hWndNotify;

		std::wstring _currentDirectory;
		std::wstring _propertiesString;
	};
}
