#pragma once

/*
	Base reader interface for accessing disk, archive, network, etc.
*/

#include "BackgroundWorker.h"
#include "DataTypes.h"
#include "SortUtils.h"

#include <memory>
#include <mutex>

namespace Commander
{
	class CBaseReader
	{
	public:
		CBaseReader( ESortMode mode, HWND hwndNotify )
			: _hWndNotify( hwndNotify )
			, _status( EReaderStatus::Invalid )
			, _sortMode( mode )
			, _resultSize( 0ull )
			, _freeSpace( 0ull )
		{}

		virtual ~CBaseReader() = default;

		virtual bool readEntries( std::wstring& path ) = 0;
		virtual bool copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) { return false; }
		virtual bool moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) { return false; }
		virtual bool renameEntry( const std::wstring& path, const std::wstring& newName ) { return false; }
		virtual bool deleteEntries( const std::vector<std::wstring>& entries ) { return false; }
		virtual bool calculateSize( const std::vector<int>& entries ) { return false; }

		virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) {}
		virtual bool isPathValid( const std::wstring& path ) { return true; }
		virtual bool isOverMaxPath() { return false; }
		virtual bool isOverlaysDone() { return false; }

	public:
		inline EReaderStatus getStatus() { return _status; }
		inline ULONGLONG getResultSize() { return _resultSize; }
		inline ULONGLONG getFreeSpace() { return _freeSpace; }
		inline size_t getEntryCount() { std::lock_guard<std::mutex> lock( _mutex ); return _dirEntries.size() + _fileEntries.size(); }
		inline std::wstring getErrorMessage() { return _errorMsg; }
		inline std::wstring getRoot() { return _root; }
		inline std::mutex& getMutex() { return _mutex; }

		inline std::vector<std::shared_ptr<EntryData>>& getDirEntries() { return _dirEntries; }
		inline std::vector<std::shared_ptr<EntryData>>& getFileEntries() { return _fileEntries; }

		std::shared_ptr<EntryData> getEntry( int entryIndex );
		int getEntryIndex( const std::wstring& entryName );
		void setSortMode( ESortMode mode ) { _sortMode = mode; }
		void sortEntries();

	protected:
		std::vector<std::shared_ptr<EntryData>> _dirEntries;
		std::vector<std::shared_ptr<EntryData>> _fileEntries;

		CBackgroundWorker _worker;

		std::wstring _root;
		std::wstring _errorMsg;
		std::mutex _mutex;

		EReaderStatus _status;
		ESortMode     _sortMode;
		ULONGLONG     _resultSize;
		ULONGLONG     _freeSpace;
		HWND          _hWndNotify;
	};
}
