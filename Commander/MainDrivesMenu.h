#pragma once

/*
	File commander's main logical drives menu
*/

#include "BackgroundWorker.h"
#include <mutex>

namespace Commander
{
	class CMainDrivesMenu
	{
	private:
		struct DriveInfo {
			std::wstring name;
			std::wstring type;
			HBITMAP hBmp;
			bool hotPlug;
			ULONGLONG freeSpace;
		};

	public:
		CMainDrivesMenu();
		~CMainDrivesMenu();

		HMENU getHandle();
		ULONGLONG getFreeSpace( WCHAR driveName );

	public:
		void init();
		void refresh();
		void updateMenu();
		void driveAdded( const std::wstring& path, bool added = true );
		void mediaInserted( const std::wstring& path, bool added = true );
		void updateFreeSpace( WCHAR driveName, ULONGLONG freeSpace );

	private:
		void insertMenuItem( int idx, HICON hIcon, const std::wstring& text, UINT cmd );
		void insertLogicalDrive( int idx, wchar_t driveName );
		std::wstring getDriveCaption( const std::wstring& path );
		bool _workerProc();

	private:
		CBackgroundWorker _worker;
		std::mutex _mutex;

		HMENU _hMenuDrives;
		DWORD _logicalDrives; // bitmask of available logical drives

		std::map<UINT, DriveInfo> _driveList;
	};
}
