#include "stdafx.h"

#include "Commander.h"
#include "CombineFiles.h"
#include "TextFileReader.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Text input with params initialization
	//
	void CCombineFiles::onInit()
	{
		// get focused item name full
		_items = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull();
		_outDir = FCS::inst().getApp().getOppositePanel().getActiveTab()->getDataManager()->getCurrentDirectory();

		if( !_items.empty() )
			initItems( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

		SetDlgItemText( _hDlg, IDE_COMBINEFILES_TARGETNAME, ( _outDir + _fileName ).c_str() );

		// set progress-bar style as indifferent
		_progressStyle = GetWindowLongPtr( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), GWL_STYLE );
		SetWindowLongPtr( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), GWL_STYLE, _progressStyle | PBS_MARQUEE );

		SendDlgItemMessage( _hDlg, IDC_COMBINEFILES_PROGRESS, PBM_SETMARQUEE, 1, 0 );
		SendDlgItemMessage( _hDlg, IDC_COMBINEFILES_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM( 0, 100 ) );
		SendDlgItemMessage( _hDlg, IDC_COMBINEFILES_PROGRESS, PBM_SETPOS, 0, 0 );

		_isInitialized = false;
		_calcCrcOnly = false;
		_batchFileParsed = false;
		_operationCanceled = false;
		_fileSize = 0ull;
		_crcOrig = _crcCalc = 0ul;
		_bytesProcessed = 0ull;

		ZeroMemory( &_stLocal, sizeof( SYSTEMTIME ) );

		MiscUtils::getWindowText( _hDlg, _windowTitle );
		MiscUtils::centerOnWindow( _hDlgParent, _hDlg );

		updateGuiStatus( false );
		show();

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );
		_worker.start();
	}

	void CCombineFiles::initItems( HWND hWndListbox )
	{
		_inDir = PathUtils::addDelimiter( PathUtils::stripFileName( _items[0] ) );
		_fileName = PathUtils::stripFileExtension( PathUtils::stripPath( _items[0] ) );

		// check if there is a .bat file in the list
		auto it = std::find_if( _items.begin(), _items.end(), [this]( const std::wstring& item ) {
			return StringUtils::endsWith( item, _fileName + L".bat" );
		} );

		// maybe ask user first before removing .bat file from the list
		if( it != _items.end() )
			_items.erase( it );

		// check if there are numbered extensions (.000 - .999) only and sort items accordingly
		bool numberedExtOnly = true;

		for( const auto& item : _items )
		{
			std::wstring ext = PathUtils::getFileNameExtension( item );
			if( ext.length() != 3 || !StringUtils::endsWith( item, _fileName + L"." + ext )
				|| !iswdigit( ext[0] ) || !std::iswdigit( ext[1] ) || !std::iswdigit( ext[2] ) )
			{
				numberedExtOnly = false;
				break;
			}
		}

		if( numberedExtOnly )
		{
			std::sort( _items.begin(), _items.end(), []( const std::wstring& a, const std::wstring& b ) {
				return SortUtils::byPath( PathUtils::stripPath( a ), PathUtils::stripPath( b ) );
			} );
		}

		int extent = 0;

		// fill in list-box partial items
		for( const auto& item : _items )
		{
			std::wstring itemName = PathUtils::stripPath( item );
			extent = max( MiscUtils::getTextExtent( hWndListbox, itemName ), extent );

			int idx = ListBox_AddString( hWndListbox, itemName.c_str() );
			ListBox_SetItemData( hWndListbox, idx, SysAllocString( item.c_str() ) );
			ListBox_SetItemHeight( hWndListbox, idx, 18 );
		}

		if( ListBox_GetHorizontalExtent( hWndListbox ) < extent )
			ListBox_SetHorizontalExtent( hWndListbox, extent + 4 );

		// select the first item
		ListBox_SetCurSel( hWndListbox, 0 );
	}

	//
	// Text input with params when Ok button pressed
	//
	bool CCombineFiles::onOk()
	{
		updateGuiStatus( false );

		SendDlgItemMessage( _hDlg, IDC_COMBINEFILES_PROGRESS, PBM_SETPOS, 0, 0 );
		ShowWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), SW_SHOWNORMAL );

		MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_COMBINEFILES_TARGETNAME ), _dlgResult );
		StringUtils::trim( _dlgResult );

		_outDir = PathUtils::addDelimiter( PathUtils::stripFileName( _dlgResult ) );
		_fileName = PathUtils::stripPath( _dlgResult );

		// get date and time from datetime picker control
		SYSTEMTIME st = { 0 };
		if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_DATE ), &st ) == GDT_VALID )
			_stLocal = st;

		// reconstruct items back from listbox
		_items.clear();
		_errorMessage.clear();

		int cnt = ListBox_GetCount( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

		for( int i = 0; i < cnt; i++ )
		{
			_items.push_back( (BSTR)ListBox_GetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), i ) );
		}

		// set timer and start the combine process
		SetTimer( _hDlg, FC_TIMER_GUI_ID, FC_TIMER_GUI_TICK, NULL );
		_worker.start();

		return false;
	}

	bool CCombineFiles::onClose()
	{
		KillTimer( _hDlg, FC_TIMER_GUI_ID );
		_worker.stop();

		return true;
	}

	void CCombineFiles::onDestroy()
	{
		int cnt = ListBox_GetCount( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

		for( int i = 0; i < cnt; i++ )
		{
			SysFreeString( (BSTR)ListBox_GetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), i ) );
		}
	}

	void CCombineFiles::updateGuiStatus( bool enable )
	{
		int cnt = ListBox_GetCount( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );
		int idx = ListBox_GetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

		EnableWindow( GetDlgItem( _hDlg, IDE_COMBINEFILES_TARGETNAME ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_CHOOSEFILE ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_DATE ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_TIME ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDE_COMBINEFILES_CRC ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_UP ), enable && cnt > 1 && idx > 0 );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_DOWN ), enable && cnt > 1 && idx < cnt - 1 );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_ADD ), enable );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_REMOVE ), enable && cnt > 0 );
		EnableWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_CALCCRC ), enable && cnt > 0 );
		EnableWindow( GetDlgItem( _hDlg, IDOK ), enable && cnt > 0 );
	}

	void CCombineFiles::updateProgressStatus()
	{
		double donePercent = ( (double)_bytesProcessed / (double)_fileSize ) * 100.0;
		SendDlgItemMessage( _hDlg, IDC_COMBINEFILES_PROGRESS, PBM_SETPOS, (int)donePercent, 0 );
	}

	void CCombineFiles::updateWindowTitle( const std::wstring& title )
	{
		SetWindowText( _hDlg, title.empty() ? _windowTitle.c_str() : title.c_str() );
	}

	bool CCombineFiles::parseBatchFile( const std::wstring& fileName )
	{
		std::ifstream fs( PathUtils::getExtendedPath( fileName ), std::ios::binary );
		CTextFileReader reader( fs, &_worker );

		while( _worker.isRunning() && reader.readLine() == ERROR_SUCCESS )
		{
			std::wstring& line = reader.getLineRef();

			StringUtils::trimComments( line );

			if( line.empty() )
				continue;

			if( StringUtils::startsWith( line, L"rem name=" ) )
			{
				std::wstring fname = line.substr( 9 );
				StringUtils::trim( fname );
				_fileName = fname.empty() ? _fileName : fname;
			}
			else if( StringUtils::startsWith( line, L"rem crc32=" ) )
			{
				std::wstring crcStr = line.substr( 10 );
				StringUtils::trim( crcStr );
				try {
					_crcOrig = std::stoul( crcStr, nullptr, 16 );
				}
				catch( std::invalid_argument )
				{
					_crcOrig = 0ul;
				}
				catch( std::out_of_range )
				{
					_crcOrig = 0ul;
				}
			}
			else if( StringUtils::startsWith( line, L"rem time=" ) )
			{
				std::wstring timeStr = line.substr( 9 );

				SYSTEMTIME st = { 0 };
				if( _snwscanf( &timeStr[0], timeStr.length(), L"%hu.%hu.%4hu %hu:%2hu:%2hu",
					&st.wDay, &st.wMonth, &st.wYear, &st.wHour, &st.wMinute, &st.wSecond ) == 6 ||
					_snwscanf( &timeStr[0], timeStr.length(), L"%4hu-%hu-%hu %hu:%2hu:%2hu",
					&st.wYear, &st.wMonth, &st.wDay, &st.wHour, &st.wMinute, &st.wSecond ) == 6 )
				{
					_stLocal = st;
					return true;
				}
			}
		}

		return false;
	}

	bool CCombineFiles::writeFilePart( std::ofstream& fStrOut, const std::wstring& fileName, uLong& crc )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

		if( hFile != INVALID_HANDLE_VALUE )
		{
			DWORD read = 0ul;
			while( ReadFile( hFile, _dataBuf, sizeof( _dataBuf ), &read, NULL ) && _worker.isRunning() && read != 0 )
			{
				crc = crc32( crc, reinterpret_cast<Bytef*>( _dataBuf ), static_cast<uInt>( read ) );

				if( !_calcCrcOnly )
					fStrOut.write( _dataBuf, read );

				_bytesProcessed += read;
			}

			CloseHandle( hFile );

			return true;
		}
		else
		{
			DWORD errorId = GetLastError();
			if( errorId != NO_ERROR )
			{
				_errorMessage = L"File: \"" + fileName + L"\"\r\n\r\n";
				_errorMessage += SysUtils::getErrorMessage( errorId );
			}
		}

		return false;
	}

	bool CCombineFiles::_workerProc()
	{
		if( !_isInitialized )
		{
			// try to parse .bat file if it exists
			std::wstring batchFileName = _fileName + L".bat";

			WIN32_FIND_DATA wfd;
			if( FsUtils::getFileInfo( _inDir + batchFileName, wfd ) )
			{
				return parseBatchFile( _inDir + batchFileName );
			}
		}
		else
		{
			std::ofstream fstr;
			_fileSize = _bytesProcessed = 0ull;
			_operationCanceled = false;

			if( !_calcCrcOnly )
			{
				bool overwrite = true;

				WIN32_FIND_DATA wfd;
				if( _fileNameOverwrite != _outDir + _fileName && FsUtils::getFileInfo( _outDir + _fileName, wfd ) )
				{
					_fileNameOverwrite.clear();

					// ask user whether file can be overwritten
					_worker.sendMessage( 2, reinterpret_cast<LPARAM>( &overwrite ) );
					_operationCanceled = !overwrite;
				}

				if( overwrite )
					fstr.open( PathUtils::getExtendedPath( _outDir + _fileName ), std::ios::binary );
			}

			if( _calcCrcOnly || fstr.is_open() )
			{
				// calculate target file size
				for( const auto& item : _items )
				{
					WIN32_FIND_DATA wfd;
					if( FsUtils::getFileInfo( item, wfd ) )
						_fileSize += FsUtils::getFileSize( &wfd ); // TODO: report error otherwise
				}

				uLong crc = crc32( 0L, Z_NULL, 0 );

				for( auto it = _items.begin(); it != _items.end() && _worker.isRunning(); ++it )
				{
					_worker.sendMessage( 3, reinterpret_cast<LPARAM>( PathUtils::stripPath( *it ).c_str() ) );

					if( !writeFilePart( fstr, *it, crc ) )
					{
						if( !_calcCrcOnly )
						{
							fstr.close();
							DeleteFile( PathUtils::getExtendedPath( _outDir + _fileName ).c_str() );
						}
						else
							_crcCalc = 0ul;

						return false;
					}
				}

				_crcCalc = crc;

				if( !_calcCrcOnly )
				{
					fstr.close();

					// set file date/time
					SYSTEMTIME stUtc;
					TzSpecificLocalTimeToSystemTime( NULL, &_stLocal, &stUtc );

					FILETIME ft;
					SystemTimeToFileTime( &stUtc, &ft );

					HANDLE hFile = CreateFile( PathUtils::getExtendedPath( _outDir + _fileName ).c_str(),
						FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
					SetFileTime( hFile, &ft, &ft, &ft );
					CloseHandle( hFile );
				}

				return _calcCrcOnly || ( _crcCalc == _crcOrig || !_batchFileParsed );
			}
		}

		return false;
	}

	void CCombineFiles::handleDropFiles( HDROP hDrop )
	{
		POINT pt;
		auto items = ShellUtils::getDroppedItems( hDrop, &pt );

		if( !items.empty() )
		{
			int extent = 0;

			for( const auto& item : items )
			{
				std::wstring itemName = PathUtils::stripPath( item );

				int idx = ListBox_AddString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), itemName.c_str() );
				if( idx != LB_ERR && idx != LB_ERRSPACE )
				{
					ListBox_SetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx, SysAllocString( item.c_str() ) );
					ListBox_SetItemHeight( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx, 18 );

					extent = max( MiscUtils::getTextExtent( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), itemName ), extent );
				}
			}

			if( ListBox_GetHorizontalExtent( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) ) < extent )
				ListBox_SetHorizontalExtent( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), extent + 4 );

			int cnt = ListBox_GetCount( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );
			ListBox_SetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), cnt - 1 );

			updateGuiStatus( true );
		}

		DragFinish( hDrop );
	}

	void CCombineFiles::onDateTimeChange( LPNMDATETIMECHANGE pDtChange )
	{
		// keep both (date and time) DateTimePickers synced on change
		if( pDtChange->dwFlags == GDT_VALID )
		{
			switch( pDtChange->nmhdr.idFrom )
			{
			case IDC_COMBINEFILES_DATE:
			{
				SYSTEMTIME st;
				if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_DATE ), &st ) == GDT_VALID )
					DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_TIME ), GDT_VALID, &st );
				break;
			}
			case IDC_COMBINEFILES_TIME:
			{
				SYSTEMTIME st;
				if( DateTime_GetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_TIME ), &st ) == GDT_VALID )
					DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_DATE ), GDT_VALID, &st );
				break;
			}
			}
		}
	}

	void CCombineFiles::onChooseFileName()
	{
		std::wstring fname = _fileName;
		std::wstring outdir = _outDir;

		if( MiscUtils::getWindowText( GetDlgItem( _hDlg, IDE_COMBINEFILES_TARGETNAME ), _dlgResult ) )
		{
			StringUtils::trim( _dlgResult );
			outdir = PathUtils::addDelimiter( PathUtils::stripFileName( _dlgResult ) );
			fname = PathUtils::stripPath( _dlgResult );
		}

		if( MiscUtils::saveFileDialog( _hDlg, outdir, fname, FOS_OVERWRITEPROMPT ) )
		{
			_fileNameOverwrite = fname;
			SetDlgItemText( _hDlg, IDE_COMBINEFILES_TARGETNAME, fname.c_str() );
		}
	}

	void CCombineFiles::onMoveItemUp()
	{
		int idx = ListBox_GetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

		if( idx != LB_ERR && idx > 0 )
		{
			BSTR data = (BSTR)ListBox_GetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );
			ListBox_DeleteString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );

			idx = ListBox_InsertString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx - 1, PathUtils::stripPath( data ).c_str() );
			ListBox_SetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx, data );

			ListBox_SetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );

			updateGuiStatus( true );
		}
	}

	void CCombineFiles::onMoveItemDown()
	{
		int cnt = ListBox_GetCount( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );
		int idx = ListBox_GetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

		if( idx != LB_ERR && idx < cnt - 1 )
		{
			BSTR data = (BSTR)ListBox_GetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );
			ListBox_DeleteString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );

			idx = ListBox_InsertString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx + 1, PathUtils::stripPath( data ).c_str() );
			ListBox_SetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx, data );

			ListBox_SetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );

			updateGuiStatus( true );
		}
	}

	void CCombineFiles::onAddItem()
	{
		std::wstring fname;

		if( MiscUtils::openFileDialog( _hDlg, _inDir, fname ) )
		{
			int idx = ListBox_AddString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), PathUtils::stripPath( fname ).c_str() );
			if( idx != LB_ERR && idx != LB_ERRSPACE )
			{
				ListBox_SetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx, SysAllocString( fname.c_str() ) );
				ListBox_SetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );

				// calculate string extemt and update listbox's extent if necessary
				int extent = MiscUtils::getTextExtent( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), PathUtils::stripPath( fname ) );

				ListBox_SetItemHeight( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx, 18 );

				if( ListBox_GetHorizontalExtent( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) ) < extent )
					ListBox_SetHorizontalExtent( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), extent + 4 );

				updateGuiStatus( true );
			}
		}
	}

	void CCombineFiles::onRemoveItem()
	{
		int idx = ListBox_GetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );
		if( idx != LB_ERR )
		{
			SysFreeString( (BSTR)ListBox_GetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx ) );
			ListBox_DeleteString( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );

			int cnt = ListBox_GetCount( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );

			if( cnt > 0 )
				ListBox_SetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx < cnt - 1 ? idx : cnt - 1 );

			updateGuiStatus( true );
		}
	}

	void CCombineFiles::focusSelectedItem()
	{
		int idx = ListBox_GetCurSel( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ) );
		if( idx != LB_ERR )
		{
			BSTR path = (BSTR)ListBox_GetItemData( GetDlgItem( _hDlg, IDC_COMBINEFILES_LIST ), idx );
			std::vector<std::wstring> items{ path };
			ShellUtils::selectItemsInExplorer( PathUtils::stripFileName( path ), items );
		}
	}

	//
	// Message handler for text input box
	//
	INT_PTR CALLBACK CCombineFiles::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_WORKERNOTIFY:
			if( _calcCrcOnly )
			{
				if( wParam > 1 )
				{
					if( wParam == 3 )
					{
						std::wostringstream sstr;
						sstr << reinterpret_cast<wchar_t*>( lParam ) << L" - " << _windowTitle;

						// currently processed file name
						updateWindowTitle( sstr.str() );
					}

					break;
				}

				_calcCrcOnly = false;

				KillTimer( _hDlg, FC_TIMER_GUI_ID );
				updateProgressStatus();
				ShowWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), SW_HIDE );

				if( wParam == 0 )
				{
					MessageBox( _hDlg, _errorMessage.empty() ? L"Error checking CRC of files." : _errorMessage.c_str(),
						_windowTitle.c_str(), MB_ICONEXCLAMATION | MB_OK );
				}
				else if( wParam == 1 && _crcOrig == 0ul )
					SetDlgItemText( _hDlg, IDE_COMBINEFILES_CRC, StringUtils::formatCrc32( _crcCalc ).c_str() );

				updateWindowTitle();
				updateGuiStatus( true );
			}
			else if( _isInitialized )
			{
				if( wParam == 1 )
				{
					KillTimer( _hDlg, FC_TIMER_GUI_ID );
					updateProgressStatus();
					ShowWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), SW_HIDE );

					if( !_batchFileParsed )
						SetDlgItemText( _hDlg, IDE_COMBINEFILES_CRC, StringUtils::formatCrc32( _crcCalc ).c_str() );

					// combine ok
					MessageBox( _hDlg, L"Files successfully combined.", _windowTitle.c_str(), MB_ICONINFORMATION | MB_OK );
					close();
				}
				else if( wParam == 2 ) // ask user if filename can be overwritten
				{
					bool *retVal = reinterpret_cast<bool*>( lParam );
					std::wostringstream sstr; sstr << L"Overwrite file \"" << _fileName << "\"?";
					*retVal = MessageBox( _hDlg, sstr.str().c_str(), _windowTitle.c_str(), MB_ICONQUESTION | MB_YESNO ) == IDYES;
				}
				else if( wParam == 3 )
				{
					std::wostringstream sstr;
					sstr << reinterpret_cast<wchar_t*>( lParam ) << L" - " << _windowTitle;

					// currently processed file name
					updateWindowTitle( sstr.str() );
				}
				else if( wParam == 0 )
				{
					KillTimer( _hDlg, FC_TIMER_GUI_ID );
					updateProgressStatus();
					ShowWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), SW_HIDE );

					if( !_operationCanceled )
					{
						if( _errorMessage.empty() && _batchFileParsed && _crcOrig != _crcCalc )
							_errorMessage = L"CRC Mismatch.\r\n\r\nOriginal CRC: " + StringUtils::formatCrc32( _crcOrig )
											+ L"\r\nCalculated CRC: " + StringUtils::formatCrc32( _crcCalc );

						MessageBox( _hDlg, _errorMessage.empty() ? L"Error combining files." : _errorMessage.c_str(),
							_windowTitle.c_str(), MB_ICONEXCLAMATION | MB_OK );
					}
					updateWindowTitle();
					updateGuiStatus( true );
				}
			}
			else
			{
				// initialization completed
				SetWindowLongPtr( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), GWL_STYLE, _progressStyle );
				SendDlgItemMessage( _hDlg, IDC_COMBINEFILES_PROGRESS, PBM_SETMARQUEE, 0, 0 );
				ShowWindow( GetDlgItem( _hDlg, IDC_COMBINEFILES_PROGRESS ), SW_HIDE );

				if( wParam == 1 )
				{
					SetDlgItemText( _hDlg, IDE_COMBINEFILES_TARGETNAME, ( _outDir + _fileName ).c_str() );
					SetDlgItemText( _hDlg, IDE_COMBINEFILES_CRC, StringUtils::formatCrc32( _crcOrig ).c_str() );

					DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_DATE ), GDT_VALID, &_stLocal );
					DateTime_SetSystemtime( GetDlgItem( _hDlg, IDC_COMBINEFILES_TIME ), GDT_VALID, &_stLocal );

					_batchFileParsed = true;
				}

				updateGuiStatus( true );

				_isInitialized = true;
			}
			break;

		case WM_DROPFILES:
			handleDropFiles( reinterpret_cast<HDROP>( wParam ) );
			break;

		case WM_TIMER:
			updateProgressStatus();
			break;

		case WM_NOTIFY:
			switch( reinterpret_cast<LPNMHDR>( lParam )->code )
			{
			case DTN_DATETIMECHANGE:
				onDateTimeChange( reinterpret_cast<LPNMDATETIMECHANGE>( lParam ) );
				break;
			}
			break;

		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDE_COMBINEFILES_TARGETNAME:
				if( HIWORD( wParam ) == EN_CHANGE )
				{
					auto len = GetWindowTextLength( GetDlgItem( _hDlg, IDE_COMBINEFILES_TARGETNAME ) );
					EnableWindow( GetDlgItem( _hDlg, IDOK ), len != 0 );
				}
				break;

			case IDC_COMBINEFILES_CHOOSEFILE:
				onChooseFileName();
				break;

			case IDC_COMBINEFILES_LIST:
				switch( HIWORD( wParam ) )
				{
				case LBN_SELCHANGE:
					updateGuiStatus( true );
					break;
				case LBN_DBLCLK:
					focusSelectedItem();
					break;
				}
				break;

			case IDC_COMBINEFILES_UP:
				onMoveItemUp();
				break;

			case IDC_COMBINEFILES_DOWN:
				onMoveItemDown();
				break;

			case IDC_COMBINEFILES_ADD:
				onAddItem();
				break;

			case IDC_COMBINEFILES_REMOVE:
				onRemoveItem();
				break;

			case IDC_COMBINEFILES_CALCCRC:
				_calcCrcOnly = true;
				onOk();
				break;

			default:
				break;
			}
			break;

		case WM_CTLCOLORSTATIC:
			if( reinterpret_cast<HWND>( lParam ) == GetDlgItem( _hDlg, IDE_COMBINEFILES_CRC ) )
			{
				if( _batchFileParsed && _crcCalc != 0ul )
				{
					SetTextColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_TEXT );
					SetBkColor( reinterpret_cast<HDC>( wParam ), _crcOrig == _crcCalc ? FC_COLOR_GREENOK : FC_COLOR_DIRBOXACTIVEWARN );
					return reinterpret_cast<LRESULT>( _worker.isRunning() ?
						FCS::inst().getApp().getBkgndBrush() : _crcOrig == _crcCalc ?
						FCS::inst().getApp().getGreenOkBrush() :
						FCS::inst().getApp().getActiveBrush( true ) );
				}
			}
			break;
		}

		return (INT_PTR)false;
	}
}
