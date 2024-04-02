#include "stdafx.h"

#include "Commander.h"
#include "CalculateChecksums.h"

#include "../Zlib/zlib.h"
#include "../Checksum/md5.h"
#include "../Checksum/sha1.h"
#include "../Checksum/sha2.h"

#include "ListViewUtils.h"
#include "MiscUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"

#define CALCCHECKSUMS_LOAD  2
#define CALCCHECKSUMS_CALC  3
#define CALCCHECKSUMS_INFO  4

namespace Commander
{
	void CCalcChecksums::onInit()
	{
		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_CHECKSUMOK ),
			IMAGE_ICON, GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ), LR_DEFAULTCOLOR ) );

		SendMessage( _hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_CHECKSUMOK ),
			IMAGE_ICON, GetSystemMetrics( SM_CXICON ), GetSystemMetrics( SM_CYICON ), LR_DEFAULTCOLOR ) );

		LvUtils::setExtStyle( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );
		LvUtils::setImageList( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );

		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"File", 120 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"Size", 82, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"CRC", 76 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"MD5", 220 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"SHA-1", 264 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"SHA-256", 410 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"SHA-512", 820 );

		SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETPOS, 0, 0 );

		_progressStyle = GetWindowLongPtr( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), GWL_STYLE );
		SetWindowLongPtr( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), GWL_STYLE, _progressStyle | PBS_MARQUEE );

		EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );

		_dialogTitle = L"Calculate Checksums";

		_currentDirectory = FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getCurrentDirectory();
		_items = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull();
		_bytesTotal = 0ull;
		_bytesProcessed = 0;
		_itemsProcessed = 0;

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_STATUSNOTIFY );
		_worker.start();
	}

	bool CCalcChecksums::onOk()
	{
		saveChecksumsFile();

		return false;
	}

	bool CCalcChecksums::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		return true;
	}

	void CCalcChecksums::onDestroy()
	{
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) );
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) );
	}

	void CCalcChecksums::updateDialogTitle( const std::wstring& status )
	{
		std::wostringstream sstrTitle;
		sstrTitle << L"[" << status << L"] - " << _dialogTitle;
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );
	}

	bool CCalcChecksums::processItem( const std::wstring& itemName )
	{
		if( FsUtils::isPathDirectory( itemName ) )
		{
			auto pathFind = PathUtils::addDelimiter( itemName );

			WIN32_FIND_DATA wfd = { 0 };
			HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
			if( hFile != INVALID_HANDLE_VALUE )
			{
				do
				{
					if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
					{
						if( !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
							&& !PathUtils::isDirectoryDotted( wfd.cFileName )
							&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
						{
							processItem( pathFind + wfd.cFileName );
						}
					}
					else
					{
						processFileName( pathFind + wfd.cFileName );
					}
				} while( FindNextFile( hFile, &wfd ) && _worker.isRunning() );

				FindClose( hFile );
			}
		}
		else
			processFileName( itemName );

		return false;
	}

	bool CCalcChecksums::processFileName( const std::wstring& fileName )
	{
		WIN32_FIND_DATA wfd = { 0 };
		if( FsUtils::getFileInfo( fileName, wfd ) )
		{
			_checksums.push_back( ChecksumType::ChecksumData { {
					wfd,
					FsUtils::getFormatStrFileSize( &wfd, false ),
					FsUtils::getFormatStrFileDate( &wfd ),
					FsUtils::getFormatStrFileTime( &wfd ),
					FsUtils::getFormatStrFileAttr( &wfd ),
					L"",
					CDiskReader::getIconIndex( wfd.cFileName, wfd.dwFileAttributes ),
					0,
					0
				},
				fileName.substr( _currentDirectory.length() )
			} );

			_bytesTotal += FsUtils::getFileSize( &wfd );

			return true;
		}

		return false;
	}

	bool CCalcChecksums::_workerProc()
	{
		_worker.sendMessage( CALCCHECKSUMS_LOAD, 0 );

		// process files and/or directories
		for( size_t i = 0; i < _items.size() && _worker.isRunning() ; ++i )
		{
			processItem( _items[i] );
		}

		_worker.sendMessage( CALCCHECKSUMS_CALC, 0 );

		// calculate checksums for files in the list
		for( size_t i = 0; i < _checksums.size() && _worker.isRunning(); ++i )
		{
			_checksums[i].crc32 = L"Calculating..";

		//	_worker.sendNotify( CALCCHECKSUMS_INFO, i );

			calcChecksums( _checksums[i] );
		}

		return true;
	}

	bool CCalcChecksums::calcChecksums( ChecksumType::ChecksumData& checksumData )
	{
		std::wstring fileName = PathUtils::getExtendedPath( _currentDirectory + checksumData.fileName );
		HANDLE hFile = CreateFile( fileName.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			SHA_State sha1s; SHA_Init( &sha1s );
			CSha256 sha256; Sha256_Init( &sha256 );
			sha512_ctx sha512; sha512_init_ctx( &sha512 );
			struct MD5_Context md5c; MD5_Init( &md5c );
			uLong crc = crc32( 0L, Z_NULL, 0 );

			DWORD read;
			while( ReadFile( hFile, _dataBuf, sizeof( _dataBuf ), &read, NULL ) && _worker.isRunning() && read != 0 )
			{
				sha512_process_bytes( _dataBuf, read, &sha512 );
				Sha256_Update( &sha256, _dataBuf, read );
				SHA_Bytes( &sha1s, _dataBuf, read );
				MD5_Update( &md5c, _dataBuf, read );
				crc = crc32( crc, _dataBuf, read );

				_bytesProcessed += read;
			}

			CloseHandle( hFile );

			unsigned char keybuf[64]; checksumData.crc32 = StringUtils::formatCrc32( crc );
			MD5_Final( keybuf, &md5c ); checksumData.md5 = StringUtils::formatHashValue( keybuf, 16 );
			SHA_Final( &sha1s, keybuf ); checksumData.sha1 = StringUtils::formatHashValue( keybuf, 20 );
			Sha256_Final( &sha256, keybuf ); checksumData.sha256 = StringUtils::formatHashValue( keybuf, 32 );
			sha512_finish_ctx( &sha512, keybuf ); checksumData.sha512 = StringUtils::formatHashValue( keybuf, 64 );

			return true;
		}
		else
			checksumData.crc32 = L"Error";

		return false;
	}

	void CCalcChecksums::selectionChanged( LPNMLISTVIEW lvData )
	{
		/*auto path = LvUtils::getItemText( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), lvData->iItem, 0 );

		if( !path.empty() && lvData->uNewState == ( LVIS_FOCUSED | LVIS_SELECTED ) )
			EnableWindow( GetDlgItem( _hDlg, IDOK ), TRUE );
		else
			EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );*/
	}

	void CCalcChecksums::focusSelectedFile()
	{
		auto itemIdx = LvUtils::getSelectedItem( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ) );
		if( itemIdx != -1 && itemIdx < (int)_checksums.size() )
		{
			auto path = _currentDirectory + _checksums[itemIdx].fileName;
			FCS::inst().getApp().getActivePanel().getActiveTab()->changeDirectory(
				PathUtils::stripFileName( path ),
				PathUtils::stripPath( path ) );
		}
	}

	void CCalcChecksums::updateProgressStatus()
	{
		double donePercent = ( (double)_bytesProcessed / (double)_bytesTotal ) * 100.0;
		SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETPOS, (int)donePercent, 0 );
		LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), _checksums.size(), LVSICF_NOSCROLL );
	}

	bool CCalcChecksums::writeFile( const std::wstring& fileName, DWORD type )
	{
		std::wostringstream sstr;

		std::wstring product, version;
		SysUtils::getProductAndVersion( product, version );

		sstr << L"; Generated by " << product << L" " << version << L"\r\n;\r\n";

		for( const auto& checksum : _checksums )
		{
			switch( type )
			{
			case 1: // sfv
				sstr << checksum.fileName << L"  " << checksum.crc32 << L"\r\n";
				break;
			case 2: // md5
				sstr << checksum.md5 << L"  " << checksum.fileName << L"\r\n";
				break;
			case 3: // sha-1
				sstr << checksum.sha1 << L"  " << checksum.fileName << L"\r\n";
				break;
			case 4: // sha-256
				sstr << checksum.sha256 << L"  " << checksum.fileName << L"\r\n";
				break;
			case 5: // sha-512
				sstr << checksum.sha512 << L"  " << checksum.fileName << L"\r\n";
				break;
			}
		}

		return FsUtils::saveFileUtf8( fileName, sstr.str().c_str(), sstr.str().length() );
	}

	void CCalcChecksums::saveChecksumsFile()
	{
		auto targetDir = PathUtils::getExtendedPath( PathUtils::stripFileName( _items[0] ) );
		wchar_t szFileName[MAX_WPATH] = { 0 };

		// when only one file is selected, suggest same name without extension
		if( _checksums.size() == 1 )
			wcsncpy( szFileName, PathUtils::stripFileExtension( _checksums[0].fileName ).c_str(), MAX_WPATH );

		OPENFILENAME ofn = { 0 };
		ofn.lStructSize = sizeof( ofn );
		ofn.hwndOwner = _hDlg;
		ofn.lpstrInitialDir = targetDir.c_str();
		ofn.lpstrTitle = L"Save Checksum File";
		ofn.lpstrFilter = L"SFV Files (*.sfv)\0*.sfv\0MD5 Files (*.md5)\0*.md5\0SHA-1 Files (*.sha1)\0*.sha1\0SHA-256 Files (*.sha256)\0*.sha256\0SHA-512 Files (*.sha512)\0*.sha512\0";
		ofn.lpstrDefExt = L"sfv";
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_WPATH;
		ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;

		if( GetSaveFileName( &ofn ) )
		{
			writeFile( szFileName, ofn.nFilterIndex );
		}
	}

	bool CCalcChecksums::handleContextMenu( HWND hWnd, POINT pt )
	{
		auto idx = LvUtils::getSelectedItem( hWnd );
		if( idx == -1 )
			return false;

		// get current cursor (or selected item) position
		LvUtils::getItemPosition( hWnd, idx, &pt );

		// show context menu for currently selected items
		auto hMenu = CreatePopupMenu();
		if( hMenu )
		{
			AppendMenu( hMenu, MF_STRING, 1, L"Copy &CRC to clipboard" );
			AppendMenu( hMenu, MF_STRING, 2, L"Copy &MD5 to clipboard" );
			AppendMenu( hMenu, MF_STRING, 3, L"Copy SHA-&1 to clipboard" );
			AppendMenu( hMenu, MF_STRING, 4, L"Copy SHA-&256 to clipboard" );
			AppendMenu( hMenu, MF_STRING, 5, L"Copy SHA-&512 to clipboard" );

			UINT cmdId = TrackPopupMenu( hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, _hDlg, NULL );
			switch( cmdId )
			{
			case 1:
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _checksums[idx].crc32 );
				break;
			case 2:
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _checksums[idx].md5 );
				break;
			case 3:
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _checksums[idx].sha1 );
				break;
			case 4:
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _checksums[idx].sha256 );
				break;
			case 5:
				MiscUtils::setClipboardText( FCS::inst().getFcWindow(), _checksums[idx].sha512 );
				break;
			default:
				break;
			}

			DestroyMenu( hMenu );
			hMenu = NULL;

			return true;
		}

		return false;
	}

	//
	// Draw items in virtual list-view
	//
	bool CCalcChecksums::onDrawDispItems( NMLVDISPINFO *lvDisp )
	{
		switch( lvDisp->item.iSubItem )
		{
		case 0:
			if( lvDisp->item.mask & LVIF_TEXT )
				lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].fileName.c_str() );

			if( lvDisp->item.mask & LVIF_IMAGE )
				lvDisp->item.iImage = _checksums[lvDisp->item.iItem].entry.imgSystem;

			if( lvDisp->item.mask & LVIF_INDENT )
				lvDisp->item.iIndent = 0;

			if( lvDisp->item.mask & LVIF_GROUPID )
				lvDisp->item.iGroupId = 0;

			if( lvDisp->item.mask & LVIF_COLUMNS )
				lvDisp->item.cColumns = 3;

			if( lvDisp->item.mask & LVIF_COLFMT )
				lvDisp->item.piColFmt = 0;

			if( lvDisp->item.mask & LVIF_PARAM )
				lvDisp->item.lParam = 0;
			break;
		case 1:
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].entry.size.c_str() );
			break;
		case 2:
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].crc32.c_str() );
			break;
		case 3:
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].md5.c_str() );
			break;
		case 4:
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].sha1.c_str() );
			break;
		case 5:
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].sha256.c_str() );
			break;
		case 6:
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].sha512.c_str() );
			break;
		default:
			return false;
		}

		return true;
	}

	bool CCalcChecksums::onNotifyMessage( LPNMHDR notifyHeader )
	{
		switch( notifyHeader->code )
		{
		case NM_RETURN:
		case NM_DBLCLK:
			focusSelectedFile();
			break;

		case LVN_GETDISPINFO:
			return onDrawDispItems( reinterpret_cast<NMLVDISPINFO*>( notifyHeader ) );

		case LVN_KEYDOWN:
			//onHandleVirtualKeys( reinterpret_cast<LPNMLVKEYDOWN>( notifyHeader ) );
			break;

		case LVN_ITEMCHANGED:
			selectionChanged( reinterpret_cast<LPNMLISTVIEW>( notifyHeader ) );
			break;

		case LVN_COLUMNCLICK:
			//sortItems( reinterpret_cast<LPNMLISTVIEW>( notifyHeader )->iSubItem );
			break;
		}

		return false;
	}

	INT_PTR CALLBACK CCalcChecksums::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_STATUSNOTIFY:
			switch( wParam )
			{
			case CALCCHECKSUMS_LOAD:
			//	updateDialogTitle( L"Scanning " + PathUtils::stripPath( _items[lParam] ) );
				SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETMARQUEE, 1, 0 );
				SetDlgItemText( _hDlg, IDC_STATICCHECKSUM, L"Scanning.." );
				show(); // show dialog
				break;
			case CALCCHECKSUMS_CALC:
			//	updateDialogTitle( PathUtils::stripPath( _items[0] ) );
				SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETMARQUEE, 0, 0 );
				SetWindowLongPtr( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), GWL_STYLE, _progressStyle );
				SetDlgItemText( _hDlg, IDC_STATICCHECKSUM, L"Calculating.." );
				SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );
				LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), _checksums.size(), LVSICF_NOSCROLL );
				break;
			case CALCCHECKSUMS_INFO:
				// updates in timer procedure
				break;
			case FC_ARCHDONEOK:
				// calculating checksums succesfully finished
				KillTimer( _hDlg, FC_TIMER_GUI_ID );
				updateProgressStatus();
				LvUtils::ensureVisible( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), static_cast<int>( _checksums.size() - 1 ), true );
				ShowWindow( GetDlgItem( _hDlg, IDC_STATICCHECKSUM ), SW_HIDE );
				ShowWindow( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), SW_HIDE );
				EnableWindow( GetDlgItem( _hDlg, IDOK ), TRUE );
				break;
			case FC_ARCHDONEFAIL:
				close();
				break;
			}
			break;

		case WM_NOTIFY:
			return onNotifyMessage( reinterpret_cast<LPNMHDR>( lParam ) );

		case WM_TIMER:
			updateProgressStatus();
			break;

		case WM_CONTEXTMENU:
			handleContextMenu( reinterpret_cast<HWND>( wParam ), POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } );
			break;

		case WM_SIZE:
			updateLayout( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		}

		return (INT_PTR)false;
	}

	void CCalcChecksums::updateLayout( int width, int height )
	{
		MoveWindow( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), 11, 11, width - 22, height - 56, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_STATICCHECKSUM ), 11, height - 32, 57, 13, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), 72, height - 32, 120, 15, true );
		MoveWindow( GetDlgItem( _hDlg, IDOK ), width - 172, height - 34, 75, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDCANCEL ), width - 86, height - 34, 75, 23, true );
	}
}
