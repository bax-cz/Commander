#include "stdafx.h"

#include "Commander.h"
#include "DuplicateFinder.h"

#include "IconUtils.h"
#include "ListViewUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	CDuplicateFinder::CDuplicateFinder()
		: _findByContent( false )
		, _searchSubdirs( false )
		, _findSize( 0ull )
		, _hStatusBar( nullptr )
	{
	}

	CDuplicateFinder::~CDuplicateFinder()
	{
	}

	void CDuplicateFinder::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, (LPARAM)IconUtils::getStock( SIID_AUTOLIST ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, (LPARAM)IconUtils::getStock( SIID_AUTOLIST, true ) );

		SetWindowText( _hDlg, L"Find duplicates" );

		// create status-bar
		_hStatusBar = CreateWindowEx( 0, STATUSCLASSNAME, (PCTSTR)NULL, SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
			0, 0, 0, 0, _hDlg, (HMENU)103, FCS::inst().getFcInstance(), NULL );

		// limit text to long max path (32000 characters)
		SendDlgItemMessage( _hDlg, IDC_FIND_NAME, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );
		SendDlgItemMessage( _hDlg, IDC_FIND_INDIR, EM_SETLIMITTEXT, (WPARAM)MAX_WPATH, 0 );
		SetDlgItemText( _hDlg, IDC_FIND_NAME, L"*.*" );
		SetDlgItemText( _hDlg, IDC_FIND_INDIR, FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getCurrentDirectory().c_str() );
		SetDlgItemText( _hDlg, IDC_FIND_USECONTENT, L"Ignore File Names (compare by content)" );
		SetWindowText( _hStatusBar, L"Ready" );

		SetWindowPos( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), NULL, 0, 0, 400, 20, SWP_NOZORDER | SWP_NOMOVE );

		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHARCHIVE ), FALSE ); // TODO: search archives
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_CONTAINCAPTION ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_AUTO ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_ANSI ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF8 ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF16 ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_RB_UTF32 ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_MATCHCASE ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_WHOLEWORD ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_HEXMODE ), FALSE );
		ShowWindow( GetDlgItem( _hDlg, IDC_FIND_REGEXMODE ), FALSE );

		CheckDlgButton( _hDlg, IDC_FIND_SEARCHSUBDIR, BST_CHECKED );

		LvUtils::setExtStyle( GetDlgItem( _hDlg, IDC_FIND_RESULT ), LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );
		LvUtils::setImageList( GetDlgItem( _hDlg, IDC_FIND_RESULT ), FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );

		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Name" );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Path", 58 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Count", 42, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Size", 64, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Date", 68, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Time", 56, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_FIND_RESULT ), L"Attr", 32, false );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		show(); // show dialog

		_worker.init( [this] { return _findFilesCore(); }, _hDlg, UM_STATUSNOTIFY );
	}

	bool CDuplicateFinder::onOk()
	{
		findDuplicates();

		return false;
	}

	bool CDuplicateFinder::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		_entries.clear();
		_duplicateEntries.clear();

		return true;
	}

	void CDuplicateFinder::onDestroy()
	{
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) );
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) );
	}

	ULONGLONG CDuplicateFinder::calcFileChecksum( const std::wstring& fileName )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );

		ULONGLONG crc = 0;

		if( hFile != INVALID_HANDLE_VALUE )
		{
			crc = MiscUtils::calcCrc32( hFile, &_worker );

			CloseHandle( hFile );
		}

		return crc;
	}

	bool CDuplicateFinder::compareFiles( Entry& entry1, Entry& entry2 )
	{
		if( entry1._wfd.nFileSizeLow == entry2._wfd.nFileSizeLow && entry1._wfd.nFileSizeHigh == entry2._wfd.nFileSizeHigh )
		{
			if( !_findByContent && !wcsicmp( entry1._wfd.cFileName, entry2._wfd.cFileName ) )
				return true;

			if( _findByContent )
			{
			//	PrintDebug("[%d]%ls, [%d]%ls", !!entry1._crc, (entry1._path + entry1._wfd.cFileName).c_str(), !!entry2._crc, (entry2._path + entry2._wfd.cFileName).c_str());
				if( !entry1._crc )
					entry1._crc = calcFileChecksum( entry1._path + entry1._wfd.cFileName );

				if( !entry2._crc )
					entry2._crc = calcFileChecksum( entry2._path + entry2._wfd.cFileName );

				return ( entry1._crc == entry2._crc );
			}
		}

		return false;
	}

	void CDuplicateFinder::duplicateFound( Entry& entry, const Entry& entryPrev )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		auto pred = [this,&entry]( Entry& item ) { return compareFiles( entry, item ); };
		auto it = std::find_if( _duplicateEntries.begin(), _duplicateEntries.end(), pred );

		if( it != _duplicateEntries.end() )
		{
			if( _findByContent )
				it->_dupls.push_back( entry._path + entry._wfd.cFileName );
			else
				it->_dupls.push_back( entry._path );

			it->_count = std::to_wstring( it->_dupls.size() + 1 );
			_worker.sendNotify( 2, NULL );
		}
		else
		{
			auto icon = IconUtils::getFromPathIndex( ( entry._path + entry._wfd.cFileName ).c_str() );
			auto size = FsUtils::getFormatStrFileSize( &entry._wfd );
			auto date = FsUtils::getFormatStrFileDate( &entry._wfd );
			auto time = FsUtils::getFormatStrFileTime( &entry._wfd );
			auto attr = FsUtils::getFormatStrFileAttr( &entry._wfd );

			if( _findByContent )
				_duplicateEntries.push_back( EntryData { entryPrev, entry._path + entry._wfd.cFileName, icon, size, date, time, attr } );
			else
				_duplicateEntries.push_back( EntryData { entryPrev, entry._path, icon, size, date, time, attr } );

			_worker.sendNotify( 2, NULL );
		}
	}

	void CDuplicateFinder::checkEntry( Entry& entry )
	{
		auto pred = [this,&entry]( Entry& item ) { return compareFiles( entry, item ); };
		auto it = std::find_if( _entries.begin(), _entries.end(), pred );

		if( it != _entries.end() )
		{
			duplicateFound( entry, *it );
		}
		else
			_entries.push_back( entry );
	}

	bool CDuplicateFinder::findEntries( const std::wstring& mask, const std::wstring& dirName )
	{
		auto pathFind = PathUtils::addDelimiter( dirName );

		WIN32_FIND_DATA wfd = { 0 };

		// read file system structure
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( _searchSubdirs && !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
						&& !PathUtils::isDirectoryDotted( wfd.cFileName )
						&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
					{
						findEntries( mask, pathFind + wfd.cFileName );
					}
				}
				else
				{
					// try to match filename with mask and add to list-box
					if( PathUtils::matchFileNameWild( mask, StringUtils::convert2Lwr( wfd.cFileName ) ) )
					{
						checkEntry( Entry { wfd, pathFind } );
					}
				}
			} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

			FindClose( hFile );
		}

		return true;
	}

	bool CDuplicateFinder::_findFilesCore()
	{
		bool ret = false;

		auto dirs = StringUtils::split( _findInDir, L";" );
		for( const auto& dir : dirs )
		{
			ret = findEntries( PathUtils::preMatchWild( _findMask ), dir );
		}

		return ret;
	}

	void CDuplicateFinder::findDuplicates()
	{
		if( _worker.isRunning() )
		{
			SetWindowText( _hStatusBar, L"Stopping.." );

			_worker.stop();

			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), TRUE );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), TRUE );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), TRUE );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHSUBDIR ), TRUE );

			SetWindowText( _hStatusBar, L"Stopped" );
			SetDlgItemText( _hDlg, IDOK, L"&Find" );
		}
		else
		{
			LvUtils::cleanUp( GetDlgItem( _hDlg, IDC_FIND_RESULT ) );
			_duplicateEntries.clear();
			_entries.clear();

			SetWindowText( _hStatusBar, L"Searching.." );
			SetDlgItemText( _hDlg, IDOK, L"&Stop" );

			TCHAR mask[MAX_WPATH + 1], inDir[MAX_WPATH + 1];
			GetDlgItemText( _hDlg, IDC_FIND_NAME, mask, MAX_WPATH );
			GetDlgItemText( _hDlg, IDC_FIND_INDIR, inDir, MAX_WPATH );

			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), FALSE );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), FALSE );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), FALSE );
			EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHSUBDIR ), FALSE );

			_findByContent = !!IsDlgButtonChecked( _hDlg, IDC_FIND_USECONTENT );
			_searchSubdirs = !!IsDlgButtonChecked( _hDlg, IDC_FIND_SEARCHSUBDIR );

			_findMask = mask;
			_findInDir = inDir;
			_findSize = 0ull;

			// focus the stop button window
			SetFocus( GetDlgItem( _hDlg, IDOK ) );

			_worker.start();
		}
	}

	void CDuplicateFinder::sortItems( int code )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		switch( code )
		{
		case 0: // sort by Name
		case VK_F3:
			std::sort( _duplicateEntries.begin(), _duplicateEntries.end(), []( const EntryData& a, const EntryData& b ) { return SortUtils::byName( a._wfd, b._wfd ); } );
			break;
		case 1: // sort by Path
		case VK_F4:
			std::sort( _duplicateEntries.begin(), _duplicateEntries.end(), []( const EntryData& a, const EntryData& b ) { return SortUtils::byPath( a._path, b._path ); } );
			break;
		case 2: // sort by duplicates count
			std::sort( _duplicateEntries.begin(), _duplicateEntries.end(), []( const EntryData& a, const EntryData& b ) { return SortUtils::byNumber( a._count, b._count ); } );
			break;
		case 3: // sort by Size;
		case VK_F6:
			std::sort( _duplicateEntries.begin(), _duplicateEntries.end(), []( const EntryData& a, const EntryData& b ) { return SortUtils::bySize( a._wfd, b._wfd ); } );
			break;
		case 4: // sort by Date
		case 5: // sort by Time
		case VK_F5:
			std::sort( _duplicateEntries.begin(), _duplicateEntries.end(), []( const EntryData& a, const EntryData& b ) { return SortUtils::byTime( a._wfd, b._wfd ); } );
			break;
		case 6: // sort by Attr
			std::sort( _duplicateEntries.begin(), _duplicateEntries.end(), []( const EntryData& a, const EntryData& b ) { return SortUtils::byAttr( a._wfd, b._wfd ); } );
			break;
		}

		LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_FIND_RESULT ), _duplicateEntries.size(), LVSICF_NOSCROLL );
	}

	void CDuplicateFinder::findFilesDone()
	{
		LRESULT itemsCount = LvUtils::getItemCount( GetDlgItem( _hDlg, IDC_FIND_RESULT ) );
		
		std::wostringstream status;
		if( itemsCount )
			status << itemsCount << L" item(s) found";
		else
			status << L"Not found";

		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_USECONTENT ), TRUE );
		EnableWindow( GetDlgItem( _hDlg, IDC_FIND_SEARCHSUBDIR ), TRUE );

		SetWindowText( _hStatusBar, status.str().c_str() );
		SetDlgItemText( _hDlg, IDOK, L"&Find" );

		// focus first edit control window
		SetFocus( GetDlgItem( _hDlg, IDC_FIND_NAME ) );
	}

	void CDuplicateFinder::onExecItem( HWND hwListView )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		auto index = LvUtils::getSelectedItem( hwListView );
		if( index != -1 && (size_t)index < _duplicateEntries.size() )
		{
			ShellUtils::executeFile( _duplicateEntries[index]._wfd.cFileName, PathUtils::addDelimiter( _duplicateEntries[index]._path ) );
			/*std::wostringstream sstr;
			auto& itemData = _duplicateEntries[index];

			for( auto& it : itemData._dupls )
			{
				sstr << it << L"\r\n";
			}

			MessageBox( _hDlg, sstr.str().c_str(), itemData._wfd.cFileName, MB_ICONINFORMATION | MB_OK );*/
		}
	}

	//
	// Draw items in virtual list-view
	//
	bool CDuplicateFinder::onDrawDispItems( NMLVDISPINFO *lvDisp )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		auto& itemData = _duplicateEntries[lvDisp->item.iItem];

		const wchar_t *str = nullptr;
		switch( lvDisp->item.iSubItem )
		{
		case 0:
			str = itemData._wfd.cFileName;
			break;
		case 1:
			str = itemData._path.c_str();
			break;
		case 2:
			str = itemData._count.c_str();
			break;
		case 3:
			str = itemData._fileSize.c_str();
			break;
		case 4:
			str = itemData._fileDate.c_str();
			break;
		case 5:
			str = itemData._fileTime.c_str();
			break;
		case 6:
			str = itemData._fileAttr.c_str();
			break;
		}

		lvDisp->item.pszText = const_cast<LPWSTR>( str );
		lvDisp->item.iImage = itemData._imgIndex;

		return true;
	}

	//
	// Find the index according to LVFINDINFO
	//
	int CDuplicateFinder::onFindItemIndex( LPNMLVFINDITEM lvFind )
	{
		std::lock_guard<std::recursive_mutex> locker( _mutex );

		int itemsCount = static_cast<int>( _duplicateEntries.size() );

		size_t findTokenLen = wcslen( lvFind->lvfi.psz );

		for( int i = lvFind->iStart; i < itemsCount; ++i )
		{
			if( _wcsnicmp( _duplicateEntries[i]._wfd.cFileName, lvFind->lvfi.psz, findTokenLen ) == 0 )
			{
				return i;
			}
		}

		for( int i = 0; i < lvFind->iStart; ++i )
		{
			if( _wcsnicmp( _duplicateEntries[i]._wfd.cFileName, lvFind->lvfi.psz, findTokenLen ) == 0 )
			{
				return i;
			}
		}

		return -1;
	}

	bool CDuplicateFinder::onContextMenu( HWND hWnd, POINT pt )
	{
		auto idx = LvUtils::getSelectedItem( hWnd );
		if( idx == -1 )
			return false;

		// get current cursor (or selected item) position
		LvUtils::getItemPosition( hWnd, idx, &pt );

		// show context menu for currently selected item
		auto hMenu = CreatePopupMenu();
		if( hMenu )
		{
			std::lock_guard<std::recursive_mutex> locker( _mutex );

			UINT_PTR i = 1;

			// add first menu item
			AppendMenu( hMenu, MF_STRING, i++, ( _findByContent ? _duplicateEntries[idx]._path + _duplicateEntries[idx]._wfd.cFileName : _duplicateEntries[idx]._path ).c_str() );

			// add item's duplicates
			for( auto& it : _duplicateEntries[idx]._dupls )
			{
				AppendMenu( hMenu, MF_STRING, i++, it.c_str() );
			}

			UINT cmdId = TrackPopupMenu( hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, _hDlg, NULL );
			if( cmdId )
			{
				if( cmdId > 1 )
				{
					std::wstring dirName = _duplicateEntries[idx]._dupls[cmdId - 2];
					std::wstring itemName = _duplicateEntries[idx]._wfd.cFileName;

					if( _findByContent )
					{
						itemName = PathUtils::stripPath( dirName );
						dirName = PathUtils::stripFileName( dirName );
					}

					FCS::inst().getApp().getActivePanel().getActiveTab()->changeDirectory( dirName, itemName );
				}
				else
					FCS::inst().getApp().getActivePanel().getActiveTab()->changeDirectory( _duplicateEntries[idx]._path, _duplicateEntries[idx]._wfd.cFileName );
			}

			DestroyMenu( hMenu );
			hMenu = NULL;

			return true;
		}

		return false;
	}

	bool CDuplicateFinder::onNotifyMessage( LPNMHDR notifyHeader )
	{
		switch( notifyHeader->code )
		{
		case NM_RETURN:
		case NM_DBLCLK:
			onExecItem( notifyHeader->hwndFrom );
			break;

		case LVN_KEYDOWN:
			if( reinterpret_cast<LPNMLVKEYDOWN>( notifyHeader )->wVKey == VK_SPACE )
			{
				onContextMenu( notifyHeader->hwndFrom, POINT { -1, -1 } );
			}
			break;

		case LVN_GETDISPINFO:
			return onDrawDispItems( reinterpret_cast<NMLVDISPINFO*>( notifyHeader ) );

		case LVN_ODFINDITEM:
			SetWindowLongPtr( _hDlg, DWLP_MSGRESULT, onFindItemIndex( reinterpret_cast<LPNMLVFINDITEM>( notifyHeader ) ) );
			return true;

		case LVN_COLUMNCLICK:
			sortItems( reinterpret_cast<LPNMLISTVIEW>( notifyHeader )->iSubItem );
			break;
		}

		return false;
	}

	INT_PTR CALLBACK CDuplicateFinder::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_STATUSNOTIFY:
			switch( wParam )
			{
			case 2:
			{
				std::lock_guard<std::recursive_mutex> locker( _mutex );
				LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_FIND_RESULT ), _duplicateEntries.size(), LVSICF_NOSCROLL );
				break;
			}
			default:
				findFilesDone();
				break;
			}
			break;

		case WM_NOTIFY:
			return onNotifyMessage( reinterpret_cast<LPNMHDR>( lParam ) );

		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
				case IDC_FIND_USECONTENT:
					//EnableWindow( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), IsDlgButtonChecked( _hDlg, IDC_FIND_USECONTENT ) );
					break;
			}
			break;

		case WM_CONTEXTMENU:
			onContextMenu( reinterpret_cast<HWND>( wParam ), POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } );
			break;

		case WM_SIZE:
			updateLayout( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		}

		return (INT_PTR)false;
	}

	void CDuplicateFinder::updateLayout( int width, int height )
	{
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_NAME ), 68, 20, width - 166, 20, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_INDIR ), 68, 49, width - 166, 20, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDOK ), width - 86, 18, 75, 23, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDCANCEL ), width - 86, 47, 75, 23, TRUE );
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_CONTENT ), 11, 98, width - 23, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_FIND_RESULT ), 11, 117, width - 23, height - 139, TRUE );

		SendMessage( _hStatusBar, WM_SIZE, 0, 0 );

		/*int allWidth = 0; // just for debug, calc entire width
		auto hList = GetDlgItem( _hDlg, IDC_FIND_RESULT );
		for (int i = 0; i < 7; i++)
		{
			allWidth += LvUtils::getColumnWidth( hList, i ); // 420
		}*/

		auto columnWidth = ( width > 424 ? width - 324 : 100 ); // first two columns
		LvUtils::setColumnWidth( GetDlgItem( _hDlg, IDC_FIND_RESULT ), columnWidth / 2, 0 );
		LvUtils::setColumnWidth( GetDlgItem( _hDlg, IDC_FIND_RESULT ), ( columnWidth / 2 ) - 42, 1 );
	}
}
