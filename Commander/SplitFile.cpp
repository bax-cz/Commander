#include "stdafx.h"

#include "Commander.h"
#include "SplitFile.h"
#include "ComboBoxUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	void initComboData( HWND hWnd )
	{
		int idx = 0;
		CbUtils::insertString( hWnd, idx++, L"Autodetect", (LPARAM)0 );
		CbUtils::insertString( hWnd, idx++, L"1.44 MB (3.5\" Floppy)", (LPARAM)1457664 );
		CbUtils::insertString( hWnd, idx++, L"720 KB (3.5\" Floppy)", (LPARAM)730112 );
		CbUtils::insertString( hWnd, idx++, L"1.2 MB (5.25\" Floppy)", (LPARAM)1213952 );
		CbUtils::insertString( hWnd, idx++, L"360 KB (5.25\" Floppy)", (LPARAM)362496 );
		CbUtils::insertString( hWnd, idx++, L"100 MB (ZIP)", (LPARAM)100431872 );
		CbUtils::insertString( hWnd, idx++, L"250 MB (ZIP)", (LPARAM)250331136 );
		CbUtils::insertString( hWnd, idx++, L"120 MB (LS-120)", (LPARAM)125829120 );
		CbUtils::insertString( hWnd, idx++, L"650 MB (CD-R)", (LPARAM)681574400 );
		CbUtils::insertString( hWnd, idx++, L"700 MB (CD-R)", (LPARAM)734003200);
		CbUtils::insertString( hWnd, idx++, L"4.38 GB (DVD-R)", (LPARAM)4707319808 );
		CbUtils::insertString( hWnd, idx++, L"4.38 GB (DVD+R)", (LPARAM)4700372992 );
		CbUtils::insertString( hWnd, idx++, L"8.15 GB (DVD-R DL)", (LPARAM)8543666176 );
		CbUtils::insertString( hWnd, idx++, L"8.15 GB (DVD+R DL)", (LPARAM)8547991552 );
		CbUtils::insertString( hWnd, idx++, L"25 GB (Blu-ray)", (LPARAM)25025314816 );
		CbUtils::insertString( hWnd, idx++, L"50 GB (Blu-ray)", (LPARAM)50050629632 );
	}

	//
	// Text input with params initialization
	//
	void CSplitFile::onInit()
	{
		// get focused item name full
		auto item = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull( false );
		_outDir = FCS::inst().getApp().getOppositePanel().getActiveTab()->getDataManager()->getCurrentDirectory();

		if( !item.empty() )
			_fileName = item[0];

		// set progress-bar style as indifferent
		_progressStyle = GetWindowLongPtr( GetDlgItem( _hDlg, IDC_SPLITFILE_PROGRESS ), GWL_STYLE );
		SetWindowLongPtr( GetDlgItem( _hDlg, IDC_SPLITFILE_PROGRESS ), GWL_STYLE, _progressStyle | PBS_MARQUEE );

		SendDlgItemMessage( _hDlg, IDC_SPLITFILE_PROGRESS, PBM_SETMARQUEE, 1, 0 );
		SendDlgItemMessage( _hDlg, IDC_SPLITFILE_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_SPLITFILE_PROGRESS, PBM_SETPOS, 0, 0 );

		// init combo predefined capacities
		initComboData( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ) );
		CbUtils::selectItem( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ), 0 );

		CheckRadioButton( _hDlg, IDC_SPLITFILE_RB_SIZE, IDC_SPLITFILE_RB_PARTS, IDC_SPLITFILE_RB_SIZE );

		ZeroMemory( &_wfd, sizeof( WIN32_FIND_DATA ) );

		_fileSize = 0ull;
		_partSize = 0ull;
		_lastSize = 0ull;
		_numParts = 0ul;
		_freeSpace = 0ull;
		_bytesProcessed = 0ull;

		_isInitialized = false;
		_fireEvent = true;
		_autoDetect = false;

		MiscUtils::getWindowText( _hDlg, _windowTitle );
		MiscUtils::centerOnWindow( _hDlgParent, _hDlg );

		updateGuiStatus( false );
		show();

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );
		_worker.start();
	}

	bool convert2Num( HWND hWnd, const std::wstring& str, ULONGLONG& result, bool silent = false )
	{
		auto strNum = StringUtils::removeAll( str, L' ' );
		auto idx = strNum.find_first_of( L'(' );

		if( idx != std::wstring::npos )
			strNum = strNum.substr( 0, idx );

		StringUtils::trim( strNum );
		double mult = 1.0;

		if( strNum.length() > 1 && StringUtils::endsWith( strNum, L"b" ) )
		{
			wchar_t ch = StringUtils::convertChar2Lwr( strNum[strNum.length() - 2] );
			switch( ch )
			{
			case L'k':
				mult = 1024.0;
				break;
			case L'm':
				mult = 1024.0 * 1024.0;
				break;
			case L'g':
				mult = 1024.0 * 1024.0 * 1024.0;
				break;
			case L't':
				mult = 1024.0 * 1024.0 * 1024.0 * 1024.0;
				break;
			default:
				if( !isdigit( ch ) )
				{
					PrintDebug("Invalid character: '%c'", (char)ch);
				}
				break;
			}
		}

		try {
			if( mult < 1024.0 )
				result = std::stoull( strNum );
			else
				result = (ULONGLONG)( std::stold( strNum ) * mult );
		}
		catch( std::invalid_argument ) {
			if( !silent )
				MessageBox( hWnd, FORMAT( L"Cannot convert \"%ls\" to number.", strNum.c_str() ).c_str(),
					L"Invalid Number", MB_ICONEXCLAMATION | MB_OK );
			return false;
		}
		catch( std::out_of_range ) {
			if( !silent )
				MessageBox( hWnd, FORMAT( L"Number \"%ls\" is out of range.", strNum.c_str() ).c_str(),
					L"Number Out of Range", MB_ICONEXCLAMATION | MB_OK );
			return false;
		}

		return true;
	}

	//
	// Text input with params when Ok button pressed
	//
	bool CSplitFile::onOk()
	{
		ShowWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_PROGRESS ), SW_SHOWNORMAL );

	//	auto id = MiscUtils::getCheckedRadio( _hDlg, { IDC_SPLITFILE_RB_SIZE, IDC_SPLITFILE_RB_PARTS } );

		std::wstring tempStr;
		MiscUtils::getWindowText( GetDlgItem(_hDlg, IDE_SPLITFILE_OUTDIR ), tempStr );
		_outDir = PathUtils::addDelimiter( tempStr );

	//	MiscUtils::getWindowText( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ), tempStr );
	//	convert2Num( _hDlg, tempStr, _partSize );

	//	_numParts = GetDlgItemInt( _hDlg, IDE_SPLITFILE_PARTS, NULL, FALSE );

		updateGuiStatus( false );

		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );

		_worker.start();

		return false;
	}

	bool CSplitFile::onClose()
	{
		KillTimer( _hDlg, FC_TIMER_GUI_ID );
		_worker.stop();

		return true;
	}

	void CSplitFile::updateWindowTitle( const std::wstring& title )
	{
		SetWindowText( _hDlg, title.empty() ? _windowTitle.c_str() : title.c_str() );
	}

	void CSplitFile::updateGuiStatus( bool enable )
	{
		std::wstringstream sstr;
		sstr << L"\" (" << FsUtils::bytesToString2( _fileSize ) << L") to:";

		std::wstring caption = MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDC_SPLITFILE_CAPTION ),
			PathUtils::stripPath( _fileName ), L"Split \"", sstr.str() );

		SetDlgItemText( _hDlg, IDC_SPLITFILE_CAPTION, caption.c_str() );
		SetDlgItemText( _hDlg, IDE_SPLITFILE_OUTDIR, _outDir.c_str() );

		auto id = MiscUtils::getCheckedRadio( _hDlg, { IDC_SPLITFILE_RB_SIZE, IDC_SPLITFILE_RB_PARTS } );

		EnableWindow( GetDlgItem( _hDlg, IDE_SPLITFILE_OUTDIR ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_CHOOSEDIR ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_RB_SIZE ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_RB_PARTS ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ), enable && id == IDC_SPLITFILE_RB_SIZE );
		EnableWindow( GetDlgItem( _hDlg, IDE_SPLITFILE_PARTS ), enable && id == IDC_SPLITFILE_RB_PARTS );
		EnableWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_LASTSIZE_CAPTION ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDE_SPLITFILE_LASTSIZE ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDOK ), enable );
	}

	void CSplitFile::updateProgressStatus()
	{
		double donePercent = ( (double)_bytesProcessed / (double)_fileSize ) * 100.0;
		SendDlgItemMessage( _hDlg, IDC_SPLITFILE_PROGRESS, PBM_SETPOS, (int)donePercent, 0 );
	}

	bool CSplitFile::generateBatchFile( uLong& crc )
	{
		std::wstring fname = PathUtils::stripPath( _fileName );

		std::wostringstream outFileName;
		outFileName << _outDir << fname << L".bat";

		std::wstring product, version;
		SysUtils::getProductAndVersion( product, version );

		std::wostringstream sstr;
		sstr << L"@echo off\r\n";
		sstr << L"rem Generated by " << product << L" " << version << L"\r\n";
		sstr << L"rem name=" << fname << L"\r\n";
		sstr << L"rem crc32=" << StringUtils::formatCrc32( crc ) << L"\r\n";
		sstr << L"rem time=" << FsUtils::getFormatStrFileDate( &_wfd ) << L" " << FsUtils::getFormatStrFileTime( &_wfd ) << L"\r\n";
		sstr << L"echo This batch file will recreate the file '" << fname << L"'.\r\n";
		sstr << L"echo Press Ctrl-C to quit.\r\n";
		sstr << L"pause\r\n";

		for( ULONG i = 0; i < _numParts; i++ )
		{
			std::wostringstream fileNamePart;
			fileNamePart << fname << L"." << std::setfill( L'0' ) << std::setw( 3 ) << i + 1;

			if( i == 0 )
				sstr << L"copy /b \"" << fileNamePart.str() << L"\" \"" << fname << L"\"\r\n";
			else
				sstr << L"copy /b \"" << fname << L"\"+\"" << fileNamePart.str() << L"\" \"" << fname << L"\"\r\n";
		}

		return FsUtils::saveFileUtf8( outFileName.str(), sstr.str().c_str(), sstr.str().length() );
	}

	std::wstring getFilePartName( const std::wstring& fileName, ULONG partNum )
	{
		std::wostringstream sstr;
		sstr << PathUtils::stripPath( fileName ) << L".";
		sstr << std::setfill( L'0' ) << std::setw( 3 ) << partNum;

		return sstr.str();
	}

	bool CSplitFile::writeFilePart( std::ifstream& fStrIn, ULONG partNum, uLong& crc )
	{
		bool retVal = false;

		if( _autoDetect )
		{
			_freeSpace = FsUtils::getDiskFreeSpace( FsUtils::getPathRoot( _outDir ) );

			if( _freeSpace > 0ull )
				_partSize = min( _fileSize - _bytesProcessed, _freeSpace );
		}

		std::wstring filePartName = PathUtils::getExtendedPath( _outDir + getFilePartName( _fileName, partNum ) );
		HANDLE hFile = CreateFile( filePartName.c_str(), GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, 0, NULL );

		if( hFile != INVALID_HANDLE_VALUE )
		{
			LARGE_INTEGER pos; pos.QuadPart = min( _fileSize - _bytesProcessed, _partSize );
			if( !!SetFilePointer( hFile, pos.LowPart, &pos.HighPart, FILE_BEGIN ) && SetEndOfFile( hFile ) )
			{
				SetFilePointer( hFile, 0, NULL, FILE_BEGIN );
				std::streamsize written = 0ll, read = 0ll, remainder = _partSize;

				while( _worker.isRunning() && ( read = fStrIn.read( _dataBuf, min( sizeof( _dataBuf ), remainder ) ).gcount() ) )
				{
					DWORD bytesWritten = 0ul;
					if( !WriteFile( hFile, _dataBuf, static_cast<DWORD>( read ), &bytesWritten, NULL ) )
					{
						_errorMessage = SysUtils::getErrorMessage( GetLastError() );
						retVal = false;
						break;
					}

					crc = crc32( crc, reinterpret_cast<Bytef*>( _dataBuf ), static_cast<uInt>( read ) );

					written += static_cast<std::streamsize>( bytesWritten );
					_bytesProcessed += static_cast<ULONGLONG>( bytesWritten );

					if( written == _partSize )
						break; // finished writing this file part

					if( written + sizeof( _dataBuf ) > _partSize )
						remainder = _partSize - written;

					retVal = ( read == static_cast<std::streamsize>( bytesWritten ) );
				}
			}
			else
				_errorMessage = SysUtils::getErrorMessage( GetLastError() );

			CloseHandle( hFile );
		}
		else
			_errorMessage = SysUtils::getErrorMessage( GetLastError() );

		if( !retVal )
			DeleteFile( filePartName.c_str() );

		return retVal;
	}

	bool CSplitFile::_workerProc()
	{
		if( !_isInitialized )
		{
			HANDLE hFile = CreateFile( PathUtils::getExtendedPath( _fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

			if( hFile != INVALID_HANDLE_VALUE )
			{
				CloseHandle( hFile );

				FsUtils::getFileInfo( _fileName, _wfd );
				_fileSize = FsUtils::getFileSize( &_wfd );
				_freeSpace = FsUtils::getDiskFreeSpace( FsUtils::getPathRoot( _outDir ) );
			}
			else
			{
				DWORD errorId = GetLastError();

				if( errorId != NO_ERROR )
					_errorMessage = SysUtils::getErrorMessage( errorId );

				return false;
			}
		}
		else
		{
			std::ifstream fs( PathUtils::getExtendedPath( _fileName ), std::ios::binary );

			if( fs.is_open() )
			{
				uLong crc = crc32( 0L, Z_NULL, 0 );

				while( _worker.isRunning() && !fs.eof() && _bytesProcessed < _fileSize )
				{
					// report currently processing part number
					_worker.sendNotify( 2, static_cast<LPARAM>( _numParts + 1 ) );

					if( !writeFilePart( fs, _numParts + 1, crc ) )
					{
						if( !_errorMessage.empty() )
							MessageBox( _hDlg, _errorMessage.c_str(), _windowTitle.c_str(), MB_ICONEXCLAMATION | MB_OK );

						// ask user for another target directory on error
						bool success = false;
						_worker.sendMessage( 3, reinterpret_cast<LPARAM>( &success ) );

						if( !success )
							return false;
					}
					else
						_numParts++;
				}

				if( _worker.isRunning() && _bytesProcessed == _fileSize )
					generateBatchFile( crc );
			}
			else
				return false;
		}

		return true;
	}

	void CSplitFile::recalcPartSize( bool selectionChanged )
	{
		_ASSERTE( _fileSize );

		_fireEvent = false;
		_autoDetect = false;

		if( IsDlgButtonChecked( _hDlg, IDC_SPLITFILE_RB_PARTS ) == BST_CHECKED )
		{
			BOOL translated = FALSE;
			UINT partsCnt = GetDlgItemInt( _hDlg, IDE_SPLITFILE_PARTS, &translated, FALSE );

			if( translated )
			{
				if( partsCnt < 1 || (ULONGLONG)partsCnt > _fileSize )
				{
					if( partsCnt < 1 )
						partsCnt = 1;
					else
						partsCnt = (UINT)min( _fileSize, 999 );

					SetDlgItemInt( _hDlg, IDE_SPLITFILE_PARTS, partsCnt, FALSE );
				}

				UINT partsCntReq = partsCnt;
				_partSize = _fileSize / (ULONGLONG)partsCnt;
				_lastSize = _fileSize - (ULONGLONG)partsCnt * _partSize;

				if( _lastSize != 0ull )
				{
					do {
						partsCnt = (UINT)( _fileSize / ++_partSize );
					} while( partsCnt > 999 );

					_lastSize = _fileSize - (ULONGLONG)partsCnt * _partSize;

					if( _lastSize != 0ull )
						partsCnt++;

					if( partsCntReq != partsCnt )
					{
						partsCnt = partsCntReq - 1;
						_partSize = _fileSize / (ULONGLONG)partsCnt;
						_lastSize = _fileSize - (ULONGLONG)partsCnt * _partSize;
					}
				}
				else
					_lastSize = _partSize;

				std::wstringstream sstr;
				sstr << FsUtils::bytesToString2( _lastSize ) << L" bytes";
				if( _lastSize >= 1024ull )
					sstr << L" (" << FsUtils::bytesToString( _lastSize ) << L")";

				SetDlgItemText( _hDlg, IDC_SPLITFILE_SIZE, FsUtils::bytesToString2( _partSize ).c_str() );
				SetDlgItemText( _hDlg, IDE_SPLITFILE_LASTSIZE, sstr.str().c_str() );
			}
			else
			{
				// clear others on invalid input
				SetDlgItemText( _hDlg, IDC_SPLITFILE_SIZE, L"" );
				SetDlgItemText( _hDlg, IDE_SPLITFILE_LASTSIZE, L"" );
			}
		}
		else
		{
			std::wstring tempStr;
			bool success = false;

			if( selectionChanged )
			{
				int idx = CbUtils::getSelectedItem( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ) );
				if( idx == 0 ) // autodetect mode
				{
					_partSize = min( _fileSize, _freeSpace );
					_autoDetect = true;

					// clear others when in autodetect mode
					SetDlgItemText( _hDlg, IDE_SPLITFILE_PARTS, L"" );
					SetDlgItemText( _hDlg, IDE_SPLITFILE_LASTSIZE, L"" );
				}
				else
				{
					LRESULT res = SendDlgItemMessage( _hDlg, IDC_SPLITFILE_SIZE, CB_GETITEMDATA, (WPARAM)idx, 0 );
					_partSize = (ULONGLONG)res;
					success = ( res != CB_ERR );
				}
			}
			else
			{
				MiscUtils::getWindowText( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ), tempStr );
				success = convert2Num( _hDlg, tempStr, _partSize, true );
			}

			if( success )
			{
				UINT partsCnt = (UINT)( _fileSize / _partSize );
				_lastSize = _fileSize - (ULONGLONG)partsCnt * _partSize;

				if( _lastSize == 0ull )
					_lastSize = _partSize;
				else
					partsCnt++;

				std::wstringstream sstr;
				sstr << FsUtils::bytesToString2( _lastSize ) << L" bytes";
				if( _lastSize >= 1024ull )
					sstr << L" (" << FsUtils::bytesToString( _lastSize ) << L")";

				SetDlgItemInt( _hDlg, IDE_SPLITFILE_PARTS, partsCnt, FALSE );
				SetDlgItemText( _hDlg, IDE_SPLITFILE_LASTSIZE, sstr.str().c_str() );
			}
		}

		_fireEvent = true;
	}

	//
	// Message handler for text input box
	//
	INT_PTR CALLBACK CSplitFile::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_WORKERNOTIFY:
			if( wParam == 3 ) // ask user for directory
			{
				std::wstring dirName;
				bool *retVal = reinterpret_cast<bool*>( lParam );
				*retVal = MiscUtils::openFileDialog( _hDlg, _outDir, dirName, FOS_PICKFOLDERS );
				if( *retVal ) _outDir = PathUtils::addDelimiter( dirName );
				SetDlgItemText( _hDlg, IDE_SPLITFILE_OUTDIR, _outDir.c_str() );
				break;
			}
			else if( wParam == 2 ) // processing part number
			{
				std::wostringstream sstr;
				sstr << getFilePartName( _fileName, static_cast<ULONG>( lParam ) );
				sstr << L" - " << _windowTitle;

				updateWindowTitle( sstr.str() );
			}
			else if( wParam == 1 )
			{
				if( _isInitialized )
				{
					KillTimer( _hDlg, FC_TIMER_GUI_ID );
					updateProgressStatus();
					ShowWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_PROGRESS ), SW_HIDE );

					// split ok
					std::wostringstream sstr;
					sstr << L"File successfully split into " << _numParts << L" parts.";
					MessageBox( _hDlg, sstr.str().c_str(), _windowTitle.c_str(), MB_ICONINFORMATION | MB_OK );
					close();
				}
				else
				{
					// initialization completed
					SetWindowLongPtr( GetDlgItem( _hDlg, IDC_SPLITFILE_PROGRESS ), GWL_STYLE, _progressStyle );
					SendDlgItemMessage( _hDlg, IDC_SPLITFILE_PROGRESS, PBM_SETMARQUEE, 0, 0 );
					ShowWindow( GetDlgItem( _hDlg, IDC_SPLITFILE_PROGRESS ), SW_HIDE );

					_isInitialized = true;
					updateGuiStatus( true );

					SetFocus( GetDlgItem( _hDlg, IDC_SPLITFILE_SIZE ) );
					recalcPartSize( true );

					if( _fileSize < 2ull )
					{
						MessageBox( _hDlg, L"File is too small to be split.", _windowTitle.c_str(), MB_ICONEXCLAMATION | MB_OK );
						close();
					}
				}
			}
			else if( wParam == 0 )
			{
				if( !_isInitialized )
					MessageBox( _hDlg, _errorMessage.empty() ? L"Error reading file." : _errorMessage.c_str(), _windowTitle.c_str(), MB_ICONEXCLAMATION | MB_OK );

				close();
			}
			break;

		case WM_TIMER:
			updateProgressStatus();
			break;

		case WM_COMMAND:
			if( HIWORD( wParam ) == EN_CHANGE || HIWORD( wParam ) == CBN_EDITCHANGE || HIWORD( wParam ) == CBN_SELCHANGE )
			{
				switch( LOWORD( wParam ) )
				{
				case IDE_SPLITFILE_OUTDIR:
					EnableWindow( GetDlgItem( _hDlg, IDOK ), GetWindowTextLength( GetDlgItem( _hDlg, IDE_SPLITFILE_OUTDIR ) ) != 0 );
					break;
				case IDC_SPLITFILE_SIZE:
				case IDE_SPLITFILE_PARTS:
					if( _fireEvent )
					{
						recalcPartSize( HIWORD( wParam ) == CBN_SELCHANGE );
						updateGuiStatus( true );
					}
					break;
				}
			}
			else
			{
				switch( LOWORD( wParam ) )
				{
				case IDC_SPLITFILE_CHOOSEDIR:
					{
						// select directory
						std::wstring dirName, dirNameTemp = _outDir;
						MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_SPLITFILE_OUTDIR ), dirNameTemp );

						if( MiscUtils::openFileDialog( _hDlg, dirNameTemp, dirName, FOS_PICKFOLDERS ) )
						{
							_outDir = PathUtils::addDelimiter( dirName );
							updateGuiStatus( true );
						}
					}
					break;

				case IDC_SPLITFILE_RB_PARTS:
					recalcPartSize( false );
					// fall-through
				case IDC_SPLITFILE_RB_SIZE:
					updateGuiStatus( true );
					break;

				default:
					break;
				}
			}
			break;

		case WM_CTLCOLORSTATIC:
			if( reinterpret_cast<HWND>( lParam ) == GetDlgItem( _hDlg, IDE_SPLITFILE_LASTSIZE ) )
			{
				if( _lastSize > _partSize )
				{
					SetTextColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_REDFAIL );
					SetBkColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_DIRBOXACTIVEWARN );
					return reinterpret_cast<LRESULT>( FCS::inst().getApp().getActiveBrush( true ) );
				}
			}
			break;
		}

		return (INT_PTR)false;
	}
}
