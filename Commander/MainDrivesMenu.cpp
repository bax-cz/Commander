#include "stdafx.h"

#include "Commander.h"
#include "IconUtils.h"
#include "MainDrivesMenu.h"

#define ISDRIVESET( val, pos ) ( ( ( ( val ) >> ( pos - L'A' ) ) & 1 ) == 1 )

namespace Commander
{
	//
	// Constructor/destructor
	//
	CMainDrivesMenu::CMainDrivesMenu()
		: _logicalDrives( 0 )
	{
		_hMenuDrives = nullptr;
	}

	CMainDrivesMenu::~CMainDrivesMenu()
	{
		for( const auto& drive : _driveList )
		{
			DeleteObject( drive.second.hBmp );
		}

		DestroyMenu( _hMenuDrives );
	}

	bool CMainDrivesMenu::_workerProc()
	{
		for( int i = L'A'; i <= L'Z'; ++i )
		{
			if( ISDRIVESET( _logicalDrives, i ) )
			{
				WCHAR driveName[] = { (WCHAR)i, L':', L'\0' };
				SendNotifyMessage( FCS::inst().getFcWindow(), UM_FSPACENOTIFY, driveName[0], FsUtils::getDiskFreeSpace( driveName ) );
			}
		}

		return true;
	}

	//
	// Creates disk drives menu and other items
	//
	void CMainDrivesMenu::init()
	{
		_logicalDrives = GetLogicalDrives();
		_hMenuDrives = CreatePopupMenu();

		_worker.init( [this] { return _workerProc(); }, FCS::inst().getFcWindow(), UM_FSPACENOTIFY );

		int idx = 0;

		// add logical drives first
		for( wchar_t i = L'A'; i <= L'Z'; ++i )
		{
			if( ISDRIVESET( _logicalDrives, i ) )
			{
				insertLogicalDrive( idx++, i );
			}
		}

		// add separator
		InsertMenu( _hMenuDrives, idx++, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );

		// add fixed items
		insertMenuItem( idx++, IconUtils::getFromPath( ShellUtils::getKnownFolderPath( FOLDERID_Documents ).c_str() ), L"&; Documents", IDM_OPENFOLDER_MYDOCUMENTS );
		insertMenuItem( idx++, IconUtils::getStock( SIID_INTERNET ), L"&/ FTP Client", IDM_TOOLBAR_FTPCLIENT );
		insertMenuItem( idx++, IconUtils::getSpecial( FCS::inst().getFcWindow(), CSIDL_CONNECTIONS ), L"&' PuTTY", IDM_TOOLBAR_PUTTY );
		insertMenuItem( idx++, IconUtils::getStock( SIID_MYNETWORK ), L"&, Network", IDM_OPENFOLDER_NETWORK );
		insertMenuItem( idx++, IconUtils::getFromPath( ( FsUtils::getSystemDirectory() + L"\\regedit.exe" ).c_str() ), L"  Windows Registry", IDM_TOOLBAR_REGED );
		insertMenuItem( idx++, IconUtils::getFromPath( FsUtils::getSystemDirectory().c_str() ), L"&. As Other Panel", IDM_LEFT_GOTO_PATHOTHERPANEL );
	}

	//
	// Update drive's free-space and return handle
	//
	HMENU CMainDrivesMenu::getHandle()
	{
		_worker.start();
		return _hMenuDrives;
	}

	ULONGLONG CMainDrivesMenu::getFreeSpace( WCHAR driveName )
	{
		if( _driveList.find( driveName ) != _driveList.end() )
			return _driveList[driveName].freeSpace;

		return 0ull;
	}

	//
	// Get disk info as one-line string - for change drive menu
	//
	std::wstring CMainDrivesMenu::getDriveCaption( const std::wstring& path )
	{
		std::wostringstream sstr;
		sstr << L"&" << path[0] << L" ";

		if( !_driveList[path[0]].name.empty() )
			sstr << _driveList[path[0]].name;
		else
			sstr << _driveList[path[0]].type;

		if( _driveList[path[0]].hotPlug )
			sstr << L"\t" << FsUtils::bytesToString( _driveList[path[0]].freeSpace );

		return sstr.str();
	}

	void CMainDrivesMenu::updateMenu()
	{
		UINT idx = 0;
		for( int i = L'A'; i <= L'Z'; ++i )
		{
			if( ISDRIVESET( _logicalDrives, i ) )
			{
				WCHAR driveName[] = { (WCHAR)i, L':', L'\0' };
				ModifyMenu( _hMenuDrives, idx, MF_BYPOSITION | MF_STRING, i, getDriveCaption( driveName ).c_str() );
			}

			if( ISDRIVESET( _logicalDrives, i ) )
				idx++;
		}
	}

	void CMainDrivesMenu::updateFreeSpace( WCHAR driveName, ULONGLONG freeSpace )
	{
		if( driveName == 1 ) // worker successfully finished
			updateMenu();
		else
			_driveList[driveName].freeSpace = freeSpace;
	}

	//
	// Update drives menu when logical drive is added/removed
	//
	void CMainDrivesMenu::driveAdded( const std::wstring& path, bool added )
	{
		PrintDebug("path: %ls(added:%d, drive:%lc)", path.c_str(), added,
			ISDRIVESET( _logicalDrives, path[0] ));

		if( ISDRIVESET( _logicalDrives, path[0] ) )
		{
			if( !added )
				refresh();
		}
		else if( added )
			refresh();
	}

	//
	// Update drives menu when media is inserted/removed
	//
	void CMainDrivesMenu::mediaInserted( const std::wstring& path, bool added )
	{
		PrintDebug("path: %ls(added:%d, drive:%lc)", path.c_str(), added,
			ISDRIVESET( _logicalDrives, path[0] ));

		if( ISDRIVESET( _logicalDrives, path[0] ) )
		{
			UINT idx = 0;
			for( int i = L'A'; i <= L'Z'; ++i )
			{
				// find menu item index and change the caption
				if( i == path[0] )
				{
					_driveList[i].name = FsUtils::getVolumeName( path );
					ModifyMenu( _hMenuDrives, idx, MF_BYPOSITION | MF_STRING, path[0], getDriveCaption( path ).c_str() );
					break;
				}

				if( ISDRIVESET( _logicalDrives, i ) )
					idx++;
			}
		}
		else
			refresh();
	}

	//
	// Update drives menu when logical drive is added/removed
	//
	void CMainDrivesMenu::refresh()
	{
		auto drivesNew = GetLogicalDrives();

		int idx = 0;

		if( drivesNew != _logicalDrives )
		{
			for( wchar_t i = L'A'; i <= L'Z'; ++i )
			{
				if( ISDRIVESET( _logicalDrives, i ) )
					idx++;

				if( ISDRIVESET( _logicalDrives, i ) && !ISDRIVESET( drivesNew, i ) )
				{
					// drive removed
					DeleteMenu( _hMenuDrives, --idx, MF_BYPOSITION );
				}
				else if( !ISDRIVESET( _logicalDrives, i ) && ISDRIVESET( drivesNew, i ) )
				{
					// drive added
					insertLogicalDrive( idx++, i );
				}
			}

			_logicalDrives = drivesNew;
		}
	}

	//
	// Add logical drive into drives menu
	//
	void CMainDrivesMenu::insertLogicalDrive( int idx, wchar_t drive )
	{
		WCHAR driveName[] = { drive, L':', L'\0' };

		_driveList[drive].name = FsUtils::getVolumeName( driveName );
		_driveList[drive].type = FsUtils::getStringDriveType( driveName );
		_driveList[drive].hotPlug = FsUtils::getHotplugInfo( driveName );
		_driveList[drive].freeSpace = FsUtils::getDiskFreeSpace( driveName );
		_driveList[drive].hBmp = nullptr;

		insertMenuItem( idx, IconUtils::getFromPath( driveName ), getDriveCaption( driveName ), (UINT)driveName[0] );
	}

	//
	// Add item into drives menu
	//
	void CMainDrivesMenu::insertMenuItem( int idx, HICON hIcon, const std::wstring& text, UINT cmd )
	{
		InsertMenu( _hMenuDrives, idx, MF_BYPOSITION | MF_STRING, cmd, text.c_str() );

		HBITMAP hBmp = IconUtils::convertIconToBitmap( hIcon, 16, 16 );
		SetMenuItemBitmaps( _hMenuDrives, idx, MF_BYPOSITION, hBmp, NULL );

		if( _driveList.find( cmd ) != _driveList.end() )
			DeleteObject( _driveList[cmd].hBmp );

		_driveList[cmd].hBmp = hBmp;

		DestroyIcon( hIcon );
	}
}
