#pragma once

/*
	Disk reader interface for accessing disk filesystem
*/

#include "BaseReader.h"

namespace Commander
{
	class CDiskReader : public CBaseReader
	{
	public:
		static int getIconIndex( const WCHAR *fileName, DWORD attr );

	public:
		CDiskReader( ESortMode mode, HWND hwndNotify );
		~CDiskReader();

		virtual bool readEntries( std::wstring& path ) override;
		virtual bool copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool renameEntry( const std::wstring& path, const std::wstring& newName ) override;
		virtual bool deleteEntries( const std::vector<std::wstring>& entries ) override;
		virtual bool calculateSize( const std::vector<int>& entries ) override;

		virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;
		virtual bool isOverMaxPath() override { return _overMaxPath; }
		virtual bool isOverlaysDone() override { return _overlaysDone; }

	private:
		bool calculateDirectorySize( const std::wstring& dirName, ULONGLONG *dirSize );
		void onDirectoryContentChanged( FILE_NOTIFY_INFORMATION *pFileNotifyInfo );
		void startFileSystemWatcher( const std::wstring& dirName );
		void stopFileSystemWatcher();

	private:
		bool _readEntries();
		bool _calcItemsSize();
		void _watcherThreadProc();

	private:
		std::wstring _currentDirectory;
		std::wstring _readFromDirectory;
		std::wstring _watchingDirectory;
		std::vector<int> _markedEntries;

		CBackgroundWorker _calcWorker;

		std::unique_ptr<std::thread> _upWatcherThread;

		HANDLE _hWaitEvent;
		HANDLE _hDirWatch;

		bool _isWatcherRunning;
		bool _contentLoaded;
		bool _overMaxPath;
		bool _overlaysDone;
	};
}
