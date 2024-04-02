#include "stdafx.h"

#include "Commander.h"
#include "VerifyChecksums.h"
#include "TextFileReader.h"

#include "../Zlib/zlib.h"
#include "../Checksum/md5.h"
#include "../Checksum/sha1.h"
#include "../Checksum/sha2.h"

#include "ListViewUtils.h"
#include "MiscUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"

#define VERIFYCHECKSUMS_LOAD  2
#define VERIFYCHECKSUMS_CALC  3
#define VERIFYCHECKSUMS_INFO  4

namespace Commander
{
	void CVerifyChecksums::onInit()
	{
		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_CHECKSUMOK ),
			IMAGE_ICON, GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ), LR_DEFAULTCOLOR ) );

		SendMessage( _hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_CHECKSUMOK ),
			IMAGE_ICON, GetSystemMetrics( SM_CXICON ), GetSystemMetrics( SM_CYICON ), LR_DEFAULTCOLOR ) );

		LvUtils::setExtStyle( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );
		LvUtils::setImageList( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );

		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"File", 400 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"Size", 88, false );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), L"Status", 64, false );

		SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETPOS, 0, 0 );

		_progressStyle = GetWindowLongPtr( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), GWL_STYLE );
		SetWindowLongPtr( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), GWL_STYLE, _progressStyle | PBS_MARQUEE );

		_dialogTitle = L"Verify Checksums";

		SetWindowText( _hDlg, _dialogTitle.c_str() );
		SetDlgItemText( _hDlg, IDOK, L"&Focus" );

		EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );

		_currentDirectory = FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getCurrentDirectory();
		_items = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull( false );

		_bytesTotal = 0ull;
		_bytesProcessed = 0;

		_itemsOk = 0;
		_itemsFail = 0;
		_itemsMissing = 0;
		_processingItem = 0;

		// get text from clipboard
		_clipboardText = MiscUtils::getClipboardText( FCS::inst().getFcWindow() );
		StringUtils::trim( _clipboardText );

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_STATUSNOTIFY );
		_worker.start();
	}

	bool CVerifyChecksums::onOk()
	{
		focusSelectedFile();

		return false;
	}

	bool CVerifyChecksums::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		KillTimer( _hDlg, FC_TIMER_GUI_ID );

		return true;
	}

	void CVerifyChecksums::onDestroy()
	{
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) );
		DestroyIcon( (HICON)SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) );
	}

	void CVerifyChecksums::updateDialogTitle( const std::wstring& status )
	{
		std::wostringstream sstrTitle;
		sstrTitle << L"[" << status << L"] - " << _dialogTitle;
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );
	}

	bool CVerifyChecksums::loadChecksums()
	{
		if( ChecksumType::isKnownType( _items[0] ) )
		{
			return openChecksumsFile( ChecksumType::getType( _items[0] ), _items[0] );
		}
		else
		{
			ChecksumType::EChecksumType type = detectChecksumType( _items[0] );

			if( type != ChecksumType::EChecksumType::FMT_UNKNOWN )
				return openChecksumsFile( type, _items[0] );
		}

		if( !_clipboardText.empty() )
		{
			return parseClipboardData( PathUtils::stripPath( _items[0] ) );
		}

		return false;
	}

	bool CVerifyChecksums::_workerProc()
	{
		_worker.sendMessage( VERIFYCHECKSUMS_LOAD, 0 );

		if( loadChecksums() )
		{
			_worker.sendMessage( VERIFYCHECKSUMS_CALC, 0 );

			return verifyChecksums();
		}

		return false;
	}

	bool CVerifyChecksums::calcChecksums( const std::wstring& fileName, ChecksumType::ChecksumData& checksumData )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
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

		return false;
	}

	bool CVerifyChecksums::verifyChecksums()
	{
		for( size_t i = 0; _worker.isRunning() && i < _checksums.size(); ++i )
		{
			_checksums[i].status = L"Verifying..";

		//	_worker.sendNotify( VERIFYCHECKSUMS_INFO, i );
			_processingItem = static_cast<int>( i );

			auto path = PathUtils::stripFileName( _checksums[i].fileName );
			if( !path.empty() ) path = PathUtils::addDelimiter( path );

			auto fileName = PathUtils::getFullPath( PathUtils::addDelimiter(
				PathUtils::stripFileName( _items[0] ) ) + path + _checksums[i].entry.wfd.cFileName );

			if( calcChecksums( fileName, _checksums[i] ) )
			{
				bool res = false;
				if( checkResult( i, res ) )
				{
					if( res ) {
						_itemsOk++;
						_checksums[i].status = L"Ok";
					}
					else {
						_itemsFail++;
						_checksums[i].status = L"Fail";
					}
				}
				else {
					_itemsFail++;
					_checksums[i].status = L"Fail";
				}
			}
			else
			{
				DWORD errorId = GetLastError();
				_itemsMissing++;
				_checksums[i].status = L"Missing";
			}
		}

		return true;
	}

	bool CVerifyChecksums::checkResult( size_t idx, bool& res )
	{
		size_t len = _checksums[idx].checkSum.length();

		if( len == ChecksumType::getLength( ChecksumType::EChecksumType::FMT_CRC ) )
			res = ( _checksums[idx].checkSum == _checksums[idx].crc32 );
		else if( len == ChecksumType::getLength( ChecksumType::EChecksumType::FMT_MD5 ) )
			res = ( _checksums[idx].checkSum == _checksums[idx].md5 );
		else if( len == ChecksumType::getLength( ChecksumType::EChecksumType::FMT_SHA1 ) )
			res = ( _checksums[idx].checkSum == _checksums[idx].sha1 );
		else if( len == ChecksumType::getLength( ChecksumType::EChecksumType::FMT_SHA256 ) )
			res = ( _checksums[idx].checkSum == _checksums[idx].sha256 );
		else if( len == ChecksumType::getLength( ChecksumType::EChecksumType::FMT_SHA512 ) )
			res = ( _checksums[idx].checkSum == _checksums[idx].sha512 );
		else
			return false;

		return true;
	}

	void CVerifyChecksums::selectionChanged( LPNMLISTVIEW lvData )
	{
		BOOL enable = FALSE;

		if( lvData->iItem != -1 && lvData->iItem < (int)_checksums.size() )
			enable = !_checksums[lvData->iItem].md5.empty();

		enable = enable && ( lvData->uNewState == ( LVIS_FOCUSED | LVIS_SELECTED ) );

		EnableWindow( GetDlgItem( _hDlg, IDOK ), enable );
	}

	void CVerifyChecksums::focusSelectedFile()
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

	void CVerifyChecksums::updateProgressStatus()
	{
		double donePercent = ( (double)_bytesProcessed / (double)_bytesTotal ) * 100.0;
		SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETPOS, (int)donePercent, 0 );
		LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), _checksums.size(), LVSICF_NOSCROLL );
		LvUtils::ensureVisible( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), _processingItem, true );
	}

	bool CVerifyChecksums::parseClipboardData( std::wstring& inFileName )
	{
		auto crcSumLength = ChecksumType::getLength( ChecksumType::EChecksumType::FMT_CRC );

		ChecksumType::EChecksumType type;
		std::wstring line;

		if( _clipboardText.length() == static_cast<size_t>( crcSumLength ) )
		{
			line = inFileName + L"  " + _clipboardText;
			type = tryParseLine( line );
		}
		else
		{
			line = _clipboardText + L"  " + inFileName;
			type = tryParseLine( line );
		}

		if( type != ChecksumType::EChecksumType::FMT_UNKNOWN )
		{
			std::wstring fileName, sum;
			if( parseLine( type, line, fileName, sum ) )
			{
				ChecksumType::ChecksumData sumData;

				if( getChecksumData( fileName, sum, sumData ) )
				{
					_checksums.push_back( std::move( sumData ) );
					return true;
				}
			}
		}

		return false;
	}

	bool CVerifyChecksums::parseLine( ChecksumType::EChecksumType type, const std::wstring& line, std::wstring& fileName, std::wstring& sum )
	{
		// read +1 more character, to ensure there is white-space delimiter between sum and filename
		auto sumTypeLength = ChecksumType::getLength( type ) + 1;

		if( line.length() <= static_cast<size_t>( sumTypeLength ) )
			return false;

		if( type == ChecksumType::EChecksumType::FMT_CRC )
		{
			sum = StringUtils::convert2Upr( line.substr( line.length() - sumTypeLength ) );
			fileName = line.substr( 0, line.length() - sumTypeLength );
		}
		else
		{
			sum = StringUtils::convert2Upr( line.substr( 0, sumTypeLength ) );
			fileName = line.substr( sumTypeLength );
		}

		StringUtils::trim( sum );
		StringUtils::trim( fileName );

		return !fileName.empty() && ChecksumType::isValid( type, sum );
	}

	ChecksumType::EChecksumType CVerifyChecksums::tryParseLine( const std::wstring& line )
	{
		std::wstring dummy, sum;

		if( parseLine( ChecksumType::EChecksumType::FMT_SHA512, line, dummy, sum ) )
			return ChecksumType::EChecksumType::FMT_SHA512;
		else if( parseLine( ChecksumType::EChecksumType::FMT_SHA256, line, dummy, sum ) )
			return ChecksumType::EChecksumType::FMT_SHA256;
		else if( parseLine( ChecksumType::EChecksumType::FMT_SHA1, line, dummy, sum ) )
			return ChecksumType::EChecksumType::FMT_SHA1;
		else if( parseLine( ChecksumType::EChecksumType::FMT_MD5, line, dummy, sum ) )
			return ChecksumType::EChecksumType::FMT_MD5;
		else if( parseLine( ChecksumType::EChecksumType::FMT_CRC, line, dummy, sum ) )
			return ChecksumType::EChecksumType::FMT_CRC;

		return ChecksumType::EChecksumType::FMT_UNKNOWN;
	}

	ChecksumType::EChecksumType CVerifyChecksums::detectChecksumType( const std::wstring& fileName )
	{
		std::ifstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );
		CTextFileReader reader( fs, &_worker );

		while( _worker.isRunning() && reader.isText() && reader.readLine() == ERROR_SUCCESS )
		{
			std::wstring& line = reader.getLineRef();

			StringUtils::trimComments( line );

			if( line.empty() )
				continue;

			auto type = tryParseLine( line );

			if( type != ChecksumType::EChecksumType::FMT_UNKNOWN )
				return type;
			else
				break; // not valid checksum file - bail out
		}

		return ChecksumType::EChecksumType::FMT_UNKNOWN;
	}

	bool CVerifyChecksums::getChecksumData( const std::wstring& fileName, const std::wstring& sum, ChecksumType::ChecksumData& data )
	{
		auto fileNameFull = PathUtils::getFullPath( PathUtils::addDelimiter( PathUtils::stripFileName( _items[0] ) ) + fileName );

		if( FsUtils::getFileInfo( fileNameFull, data.entry.wfd ) )
		{
			auto fileSize = FsUtils::getFileSize( &data.entry.wfd );
			data.entry.imgSystem = CDiskReader::getIconIndex( data.entry.wfd.cFileName, data.entry.wfd.dwFileAttributes );
			data.entry.size = FsUtils::getFormatStrFileSize( &data.entry.wfd, false );
			data.entry.date = FsUtils::getFormatStrFileDate( &data.entry.wfd );
			data.entry.time = FsUtils::getFormatStrFileTime( &data.entry.wfd );
			data.entry.attr = FsUtils::getFormatStrFileAttr( &data.entry.wfd );

			_bytesTotal += fileSize;
		}
		else
		{
			ZeroMemory( &data.entry.wfd, sizeof( WIN32_FIND_DATA ) );
			data.entry.size = L"-";
			data.entry.date = L"-";
			data.entry.time = L"-";
			data.entry.attr = L"-";
			data.entry.imgSystem = 0;
		}

		data.fileName = fileName;
		data.checkSum = sum;

		data.entry.imgOverlay = 0; // unused
		data.entry.hardLinkCnt = 0; // unused

		PathUtils::unifyDelimiters( data.fileName );

		return true;
	}

	bool CVerifyChecksums::openChecksumsFile( ChecksumType::EChecksumType type, const std::wstring& inFileName )
	{
		std::ifstream fs( PathUtils::getExtendedPath( inFileName ), std::ios::binary );
		CTextFileReader reader( fs, &_worker );

		std::wstring fileName, sum;
		while( _worker.isRunning() && reader.readLine() == ERROR_SUCCESS )
		{
			std::wstring& line = reader.getLineRef();

			StringUtils::trimComments( line );

			if( line.empty() )
				continue;

			if( parseLine( type, line, fileName, sum ) )
			{
				ChecksumType::ChecksumData sumData;

				if( getChecksumData( fileName, sum, sumData ) )
					_checksums.push_back( std::move( sumData ) );
			}
			else
				break;
		}

		return !_checksums.empty();
	}

	bool CVerifyChecksums::handleContextMenu( HWND hWnd, POINT pt )
	{
		/*auto idx = LvUtils::getSelectedItem( hWnd );
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
		}*/

		return false;
	}

	void CVerifyChecksums::reportResult()
	{
		std::wostringstream sstr;

		if( _checksums.size() == _itemsOk && _itemsFail == 0 && _itemsMissing == 0 )
		{
			sstr << L"All files Ok.";
		}
		else
		{
			if( _itemsFail )
				sstr << _itemsFail << L" failed";

			if( _itemsMissing )
				sstr << ( _itemsFail ? L", " : L"" ) << _itemsMissing << L" missing";

			sstr << L".";
		}

		SetDlgItemText( _hDlg, IDC_STATICCHECKSUM, sstr.str().c_str() );
	}

	bool CVerifyChecksums::onCustomDraw( LPNMLVCUSTOMDRAW lvcd )
	{
		switch( lvcd->nmcd.dwDrawStage )
		{
		case CDDS_PREPAINT:
			// tell the control we are interested in per-item notifications
			// (we need it just to tell the control we want per-subitem notifications)
			SetWindowLongPtr( _hDlg, DWLP_MSGRESULT, (LONG_PTR)( CDRF_DODEFAULT | CDRF_NOTIFYITEMDRAW ) );
			return true;

		case ( CDDS_ITEM | CDDS_PREPAINT ):
			// tell the control we are interested in per-subitem notifications
			SetWindowLongPtr( _hDlg, DWLP_MSGRESULT, (LONG_PTR)( CDRF_DODEFAULT | CDRF_NOTIFYSUBITEMDRAW ) );
			return true;

		case ( CDDS_ITEM | CDDS_SUBITEM | CDDS_PREPAINT ):
			switch( lvcd->iSubItem )
			{
			case 0:
			case 1:
			{
				// missing items in grey color
				auto text = _checksums[lvcd->nmcd.dwItemSpec].entry.size;
				if( text == L"-" )
					lvcd->clrText = FC_COLOR_HIDDEN;
				break;
			}
			case 2:
			{
				// customize "status" column by marking result with appropriate color
				auto text = _checksums[lvcd->nmcd.dwItemSpec].status;
				lvcd->clrText = ( text == L"Ok" ) ? FC_COLOR_GREENOK
					: ( text == L"Verifying.." ) ? 0 : FC_COLOR_REDFAIL;

				// let the control do the painting itself with the new color
				//SetWindowLongPtr( _hDlg, DWLP_MSGRESULT, (LONG_PTR)CDRF_DODEFAULT );
				//return true;
			}
			}
			break;
		}

		// for all unhandled cases, we let the control do the default
		return false;
	}

	//
	// Draw items in virtual list-view
	//
	bool CVerifyChecksums::onDrawDispItems( NMLVDISPINFO *lvDisp )
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
			lvDisp->item.pszText = const_cast<TCHAR*>( _checksums[lvDisp->item.iItem].status.c_str() );
			break;
		default:
			return false;
		}

		return true;
	}

	bool CVerifyChecksums::onNotifyMessage( LPNMHDR notifyHeader )
	{
		switch( notifyHeader->code )
		{
		case NM_RETURN:
		case NM_DBLCLK:
			focusSelectedFile();
			break;

		case NM_CUSTOMDRAW:
			if( notifyHeader->idFrom == IDC_LISTCHECKSUMS )
				return onCustomDraw( reinterpret_cast<LPNMLVCUSTOMDRAW>( notifyHeader ) );
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

	INT_PTR CALLBACK CVerifyChecksums::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_STATUSNOTIFY:
			switch( wParam )
			{
			case VERIFYCHECKSUMS_LOAD:
				updateDialogTitle( L"Loading " + PathUtils::stripPath( _items[0] ) );
				SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETMARQUEE, 1, 0 );
				SetDlgItemText( _hDlg, IDC_STATICCHECKSUM, L"Loading.." );
				show(); // show dialog
				break;
			case VERIFYCHECKSUMS_CALC:
				updateDialogTitle( PathUtils::stripPath( _items[0] ) );
				SendDlgItemMessage( _hDlg, IDC_PROGRESSCHECKSUM, PBM_SETMARQUEE, 0, 0 );
				SetWindowLongPtr( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), GWL_STYLE, _progressStyle );
				SetDlgItemText( _hDlg, IDC_STATICCHECKSUM, L"Verifying:" );
				SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );
				LvUtils::setItemCount( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), _checksums.size(), LVSICF_NOSCROLL );
				break;
			case VERIFYCHECKSUMS_INFO:
				// updates in timer procedure
				break;
			case FC_ARCHDONEOK:
				// verifying checksums succesfully finished
				KillTimer( _hDlg, FC_TIMER_GUI_ID );
				updateProgressStatus();
				ShowWindow( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), SW_HIDE );
				reportResult();
				break;
			case FC_ARCHDONEFAIL:
				MessageBox( _hDlg, L"No checksums in file or clipboard found!", L"Verify Checksums", MB_OK | MB_ICONEXCLAMATION  );
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

	void CVerifyChecksums::updateLayout( int width, int height )
	{
		MoveWindow( GetDlgItem( _hDlg, IDC_LISTCHECKSUMS ), 11, 11, width - 22, height - 56, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_STATICCHECKSUM ), 11, height - 32, 200, 13, true );
		MoveWindow( GetDlgItem( _hDlg, IDC_PROGRESSCHECKSUM ), 72, height - 32, 120, 15, true );
		MoveWindow( GetDlgItem( _hDlg, IDOK ), width - 172, height - 34, 75, 23, true );
		MoveWindow( GetDlgItem( _hDlg, IDCANCEL ), width - 86, height - 34, 75, 23, true );
	}
}
