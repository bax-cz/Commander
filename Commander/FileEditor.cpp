#include "stdafx.h"

#include "Commander.h"
#include "FileEditor.h"
#include "FileViewer.h"
#include "TextFileReader.h"
#include "FileUnpacker.h"
#include "FtpTransfer.h"
#include "SshTransfer.h"
#include "IconUtils.h"
#include "MenuUtils.h"
#include "MiscUtils.h"
#include "EditUtils.h"

namespace Commander
{
	void CFileEditor::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getFromPathWithAttrib( L".txt", FILE_ATTRIBUTE_NORMAL ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getFromPathWithAttrib( L".txt", FILE_ATTRIBUTE_NORMAL, true ) ) );

		_hEventHook = nullptr;
		_hEditData = nullptr;
		_wordWrap = false;
		_sortLines = false;
		_hiliteText = false;
		_findParams = { false };

		// set dialog menu handle
		_hMenu = LoadMenu( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDM_MAINMENU_FILEVIEWER ) );
		SetMenu( _hDlg, _hMenu );

		// create edit control
		_hActiveEdit = createEditControl();

		// create status-bar
		_hStatusBar = createStatusBar();

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );
		ShowWindow( _hActiveEdit, SW_SHOWNORMAL );

		// tell windows that we can handle drag and drop
		DragAcceptFiles( _hDlg, TRUE );

		// add editor specific menu items
		MenuUtils::insertItem( GetSubMenu( _hMenu, 1 ), L"&Undo\tCtrl+Z", 0, IDM_VIEWER_EDIT_UNDO );
		MenuUtils::insertItem( GetSubMenu( _hMenu, 1 ), L"", 1, 0, MF_SEPARATOR );
		MenuUtils::insertItem( GetSubMenu( _hMenu, 1 ), L"Cu&t\tCtrl+X", 2, IDM_VIEWER_EDIT_CUT );
		MenuUtils::insertItem( GetSubMenu( _hMenu, 1 ), L"&Paste\tCtrl+V", 4, IDM_VIEWER_EDIT_PASTE );
		MenuUtils::insertItem( GetSubMenu( _hMenu, 1 ), L"&Delete\tDel", 5, IDM_VIEWER_EDIT_DELETE );
		MenuUtils::insertItem( GetSubMenu( _hMenu, 2 ), L"&Replace...\tCtrl+H", 3, IDM_VIEWER_SEARCH_REPLACE );

		// remove viewer specific menu items (view hex/text)
		MenuUtils::removeItem( GetSubMenu( _hMenu, 3 ), 2 );
		MenuUtils::removeItem( GetSubMenu( _hMenu, 3 ), 1 );
		MenuUtils::removeItem( GetSubMenu( _hMenu, 3 ), 0 );

		// create find dialog instance
		_pFindDlg = CBaseDialog::createModeless<CFindText>( _hDlg, true );

		_tripleClickPos = { 0, 0 };
		_tripleClickTime = 0;
		_currentZoom = 100;
		_fileSize = 0ll;
		_fileBom = StringUtils::NOBOM;
		_useEncoding = StringUtils::NOBOM;
		_useCodePage = GetACP(); // system default codepage

		// show status-bar by default
		showStatusBar();

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );
	}

	bool CFileEditor::onClose()
	{
		bool modified = !!SendMessage( _hActiveEdit, EM_GETMODIFY, 0, 0 );

		return askSaveChanges( modified );
	}

	void CFileEditor::onDestroy()
	{
		if( !_tempFileName.empty() )
			DeleteFile( _tempFileName.c_str() );

		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );

		RemoveWindowSubclass( _hActiveEdit, wndProcEditSubclass, 0 );
	}

	void CFileEditor::updateWindowTitle()
	{
		bool modified = !!SendMessage( _hActiveEdit, EM_GETMODIFY, 0, 0 );

		std::wostringstream sstrTitle;
		sstrTitle << ( modified ? L"*" : L"" ) << ( _tempFileName.empty() ? _fileName : _tempFileName ) << L" - Edit";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );
	}

	HWND CFileEditor::createEditControl()
	{
		RECT rct;
		GetClientRect( _hDlg, &rct );

		DWORD dwStyle = WS_CHILD | ES_MULTILINE | ES_NOHIDESEL | ES_WANTRETURN | WS_VSCROLL | ( _wordWrap ? 0 : WS_HSCROLL );

		HWND hEdit = CreateWindowEx( 0, WC_EDIT, L"", dwStyle,
			rct.left, rct.top, rct.right - rct.left, rct.bottom - rct.top,
			_hDlg, NULL, FCS::inst().getFcInstance(), NULL );

		// make edit controls zoomable (only supported in Windows 10 1809 and later)
		SendMessage( hEdit, EM_SETEXTENDEDSTYLE, ES_EX_ZOOMABLE, ES_EX_ZOOMABLE );

		// make the edit control to handle ENTER key (same as DLGC_WANTALLKEYS in WM_GETDLGCODE)
	//	LONG_PTR style = GetWindowLongPtr( hEdit, GWL_STYLE );
	//	SetWindowLongPtr( hEdit, GWL_STYLE, style | ES_WANTRETURN );

		// subclass edit control and store its default window procedure
		SetWindowSubclass( hEdit, wndProcEditSubclass, 0, reinterpret_cast<DWORD_PTR>( this ) );

		// set font for viewer
		SendMessage( hEdit, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );

		// determine whether edit control supports zooming
		LONG_PTR numerator = 0, denominator = 0;
		SendMessage( hEdit, EM_GETZOOM, reinterpret_cast<WPARAM>( &numerator ), reinterpret_cast<LPARAM>( &denominator ) );

		// set limit text to the maximum possible value
		SendMessage( hEdit, EM_SETLIMITTEXT, 0, 0 );

		MenuUtils::enableItemByPos( GetSubMenu( _hMenu, 3 ), 3, numerator && denominator );

		return hEdit;
	}

	HWND CFileEditor::createStatusBar()
	{
		HWND hStatusBar = CreateWindowEx( 0, STATUSCLASSNAME, (PCTSTR)NULL, SBARS_SIZEGRIP | WS_CHILD | WS_CLIPSIBLINGS,
			0, 0, 0, 0, _hDlg, (HMENU)104, FCS::inst().getFcInstance(), NULL );

		// initialize status-bar parts
		RECT rct;
		GetClientRect( _hDlg, &rct );

		int sbParts[5] = { rct.right - 390, rct.right - 250, rct.right - 200, rct.right - 100, rct.right };
		SendMessage( hStatusBar, SB_SETPARTS, 5, (LPARAM)sbParts );

		return hStatusBar;
	}

	bool CFileEditor::getEditCaretPosStr( HWND hEdit, std::wstring& pos )
	{
		int idxBeg = 0, idxEnd = 0;
		SendMessage( hEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		// do not update when selection has not been changed
		if( idxBeg == _findFromIdxBeg && idxEnd == _findFromIdxEnd )
			return false;

		int idx = ( idxEnd == _findFromIdxEnd ) ? idxBeg : idxEnd;

		auto lineIdx = static_cast<int>( SendMessage( hEdit, EM_LINEFROMCHAR, idx, 0 ) );
		auto colIdx = idx - static_cast<int>( SendMessage( hEdit, EM_LINEINDEX, lineIdx, 0 ) );

		std::wostringstream sstr;
		sstr << L"Ln " << lineIdx + 1 << L", Col " << colIdx + 1;

		pos = sstr.str();

		_findFromIdxBeg = idxBeg;
		_findFromIdxEnd = idxEnd;

		return true;
	}

	void CFileEditor::updateStatusBarCaretPos( bool force )
	{
		if( force )
			_findFromIdxBeg = _findFromIdxEnd = -1;

		// set status-bar current line and column
		std::wstring pos;
		if( getEditCaretPosStr( _hActiveEdit, pos ) )
		{
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 1, SBT_NOBORDERS ), (LPARAM)pos.c_str() );
			PrintDebug("start: %d, end: %d", _findFromIdxBeg, _findFromIdxEnd);
		}
	}

	void CFileEditor::updateMenuEdit()
	{
		// disable menu when there is no text selected
		DWORD idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

		// try to get clipboard text
		std::wstring clipText = MiscUtils::getClipboardText( FCS::inst().getFcWindow() );

		// check if undo is present
		bool canUndo = !!SendMessage( _hActiveEdit, EM_CANUNDO, 0, 0 );

		MenuUtils::enableItem( GetSubMenu( _hMenu, 1 ), IDM_VIEWER_EDIT_UNDO, canUndo );
		MenuUtils::enableItem( GetSubMenu( _hMenu, 1 ), IDM_VIEWER_EDIT_CUT, ( idxBeg != idxEnd ) );
		MenuUtils::enableItem( GetSubMenu( _hMenu, 1 ), IDM_VIEWER_EDIT_COPY, ( idxBeg != idxEnd ) );
		MenuUtils::enableItem( GetSubMenu( _hMenu, 1 ), IDM_VIEWER_EDIT_PASTE, !clipText.empty() );
		MenuUtils::enableItem( GetSubMenu( _hMenu, 1 ), IDM_VIEWER_EDIT_DELETE, ( idxBeg != idxEnd ) );
	}

	void CFileEditor::updateMenuView()
	{
		// dis/enable zoom submenu items
		MenuUtils::enableItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_ZOOM_IN, ( _currentZoom < 500 ) );
		MenuUtils::enableItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_ZOOM_OUT, ( _currentZoom > 10 ) );
	}

	bool CFileEditor::viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel )
	{
		_fileName = PathUtils::addDelimiter( dirName ) + fileName;
		_findFromIdxBeg = _findFromIdxEnd = 0;
		_useEncoding = static_cast<StringUtils::EUtfBom>( _dlgUserDataPtr );

		if( spPanel )
		{
			_localDirectory = spPanel->getDataManager()->getCurrentDirectory();
			_spPanel = spPanel;
		}

		SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"Loading file.." );

		updateWindowTitle();

		// delete temp file when reading different file
		if( !_tempFileName.empty() )
		{
			DeleteFile( _tempFileName.c_str() );
			_tempFileName.clear();
		}

		// read file data
		if( _spPanel )
		{
			return _spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, _spPanel, _hDlg );
		}
		else
		{
			if( _spPanel == nullptr && ArchiveType::isKnownType( dirName ) )
			{
				_tempFileName = FCS::inst().getTempPath() + PathUtils::stripPath( fileName );

				// file extractor sends UM_READERNOTIFY notification when done
				CBaseDialog::createModeless<CFileUnpacker>( _hDlg )->unpack(
					dirName, fileName, FCS::inst().getTempPath(), CArchiver::EExtractAction::Overwrite );
			}
			else // read file directly
				SendMessage( _hDlg, UM_READERNOTIFY, FC_ARCHDONEOK, 0 );
		}

		return true;
	}

	void CFileEditor::setParams( const CFindText::CParams& params, const std::wstring& text )
	{
		_findParams = params;
		_textToFind = text;

		_hiliteText = true;
	}

	bool CFileEditor::_workerProc()
	{
		// read data from file
		return readFromDisk( _tempFileName.empty() ? _fileName : _tempFileName );
	}

	void CFileEditor::viewFileCore( StringUtils::EUtfBom encoding )
	{
		bool modified = !!SendMessage( _hActiveEdit, EM_GETMODIFY, 0, 0 );

		if( !askSaveChanges( modified ) )
			return;

		// reset starting index/offset when changing modes/encodings
		if( modified || _useEncoding != encoding )
		{
			_findFromIdxBeg = _findFromIdxEnd = 0;
		}
		else // store caret position and current selection
			SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&_findFromIdxBeg, (LPARAM)&_findFromIdxEnd );

		_fileEol = StringUtils::NONE;
		_useEncoding = encoding;

		updateWindowTitle();
		updateMenuEncoding();

		_worker.start();
	}

	void CFileEditor::hiliteText()
	{
		if( preSearchText() )
			searchText( _findParams.reverse );
	}

	void CFileEditor::onReaderFinished( bool retVal )
	{
		if( retVal )
		{
			std::wstring result = _fileName;

			if( _spPanel )
				result = _spPanel->getDataManager()->getDataProvider()->getResult();

			if( result != _fileName )
				_tempFileName = result;

			viewFileCore( _useEncoding );
		}
		else
		{
			std::wstringstream sstr;
			sstr << L"Error opening \"" << ( _tempFileName.empty() ? _fileName : _tempFileName ) << L"\" file.";
			MessageBox( _hDlg, sstr.str().c_str(), L"View File - Error", MB_OK | MB_ICONEXCLAMATION );
			close(); // close dialog
		}
	}

	void CFileEditor::onWorkerFinished( bool retVal )
	{
		if( retVal )
		{
			{
				CEditControlHelper editHelper( _hActiveEdit );
				editHelper.swapDataHandle( _hEditData );
			}

			// restore caret position and current selection
			SendMessage( _hActiveEdit, EM_SETSEL, (WPARAM)_findFromIdxBeg, (LPARAM)_findFromIdxEnd );
			SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

			// set status-bar current line and column and update current encoding in menu
			updateStatusBarCaretPos( true );
			updateMenuEncoding();
			updateWindowTitle();

			// clean up status text
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"" );

			// set status-bar current zoom as text
			std::wstring zoom = std::to_wstring( _currentZoom ) + L"%";
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 2, SBT_NOBORDERS ), (LPARAM)zoom.c_str() );

			// set status-bar End-of-line as text
			std::wstring eol = StringUtils::eolToStr( _fileEol );
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 3, SBT_NOBORDERS ), (LPARAM)eol.c_str() );

			// set status-bar current encoding as text
			std::wstring bom = StringUtils::encodingToStr( _useEncoding );
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 4, SBT_NOBORDERS ), (LPARAM)bom.c_str() );

			// hilite text just once after opening (when called from filefinder)
			if( _hiliteText )
			{
				_hiliteText = false;

				if( !_textToFind.empty() )
					hiliteText();
			}

			SetCursor( NULL );
		}
		else
		{
			SetCursor( NULL );

			if( !_errorMessage.empty() )
				MessageBox( _hDlg, _errorMessage.c_str(), L"Cannot Read File", MB_ICONEXCLAMATION | MB_OK );

			close(); // close dialog
		}
	}

	bool CFileEditor::readFromDisk( const std::wstring& fileName )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( fileName ).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

		if( hFile != INVALID_HANDLE_VALUE )
		{
			CloseHandle( hFile );

			std::ifstream ifs( PathUtils::getExtendedPath( fileName ), std::ios::binary );

			PrintDebug("Opening file '%ls'(size: %zd)", PathUtils::stripPath( fileName ).c_str(), _fileSize);

			return readFileText( ifs );
		}
		else
		{
			DWORD errorId = GetLastError();
			std::wostringstream sstr;
			sstr << L"Path: \"" << fileName << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( errorId );

			_errorMessage = sstr.str();

			return false;
		}
	}

	SIZE_T getAllocSize( StringUtils::EUtfBom encoding, StringUtils::EEol eol, LONGLONG size )
	{
		SIZE_T allocSize = size;

		switch( encoding )
		{
		case StringUtils::ANSI:
		case StringUtils::UTF8:
			allocSize = size * 2;
			break;
		case StringUtils::UTF16LE:
		case StringUtils::UTF16BE:
			allocSize = size;
			break;
		case StringUtils::UTF32LE:
		case StringUtils::UTF32BE:
			allocSize = size / 2;
			break;
		default:
			allocSize = size * 2;
		}

		if( eol != StringUtils::CRLF )
			allocSize += allocSize / 10;

		return max( 64, allocSize ); // allocate some even for empty files
	}

	bool CFileEditor::readFileText( std::istream& inStream )
	{
		CTextFileReader reader( inStream, &_worker, _useEncoding, _useCodePage, CTextFileReader::EOptions::forceAnsi );

		// ask user what to do when text file is too big (over 16MB)
		if( !reader.isText() && _fileSize > 0xFFFFFFll && MessageBox( _hDlg,
				L"File appears to be a binary file.\nContinue displaying as text?", L"Binary File",
				MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2 ) == IDNO )
			return false;

		_useEncoding = reader.getEncoding();
		_fileBom = reader.getByteOrderMarker();
		_fileEol = reader.getEol();
		_fileSize = reader.getFileSize();

		PrintDebug("Encoding: %ls, EOL: %ls", StringUtils::encodingToStr( _useEncoding ).c_str(), StringUtils::eolToStr( _fileEol ).c_str());

		// read file content as text
		DWORD errorId = ERROR_SUCCESS;
		SIZE_T offset = 0ull, allocSize = getAllocSize( _useEncoding, _fileEol, _fileSize );

		HLOCAL hLocal = LocalAlloc( ( !_fileSize ? LMEM_MOVEABLE | LMEM_ZEROINIT : LMEM_MOVEABLE ), allocSize );

		if( hLocal )
		{
			wchar_t *buf = reinterpret_cast<wchar_t*>( LocalLock( hLocal ) );
			if( buf )
			{
				while( ( errorId = reader.readLine() ) == ERROR_SUCCESS )
				{
					// the 2 means CRLF characters
					SIZE_T sizeNeeded = ( offset + 2 + reader.getLineRef().length() ) * sizeof( wchar_t );

					if( sizeNeeded >= allocSize )
					{
						PrintDebug("Reallocating: %zu(%zu)", allocSize, (sizeNeeded+64));

						allocSize = sizeNeeded + 64; // TODO: size vs avoid too many reallocs??

						LocalUnlock( hLocal );
						HLOCAL hLocalNew = LocalReAlloc( hLocal, allocSize, LMEM_MOVEABLE );
						if( hLocalNew ) {
							hLocal = hLocalNew;
							buf = reinterpret_cast<wchar_t*>( LocalLock( hLocal ) );
						}

						errorId = GetLastError();

						if( LocalSize( hLocal ) < allocSize )
							break;
					}

					wcsncpy( buf + offset, &reader.getLineRef()[0], reader.getLineRef().length() );
					offset += reader.getLineRef().length();

					if( reader.isEolEnding() )
					{
						wcscpy( buf + offset, L"\r\n" ); // append CRLF+'\0' at the end of each line
						offset += 2;
					}
					else
						*(buf + offset) = L'\0'; // append terminating zero
				}

				DWORD errorIdUnlock = ERROR_SUCCESS;
				if( LocalUnlock( hLocal ) == FALSE )
					errorIdUnlock = GetLastError();

				if( errorIdUnlock != ERROR_SUCCESS && errorIdUnlock != ERROR_NOT_LOCKED )
				{
					PrintDebug("LocalUnlock Error: %u", errorIdUnlock);
				}
				else
					_hEditData = hLocal;
			}
		}

		if( errorId != ERROR_SUCCESS && errorId != ERROR_HANDLE_EOF && errorId != ERROR_BAD_FORMAT )
		{
			std::wostringstream sstr;
			sstr << L"Path: \"" << _fileName << L"\"\r\n";
			sstr << L"Encoding: \"" << StringUtils::encodingToStr( _useEncoding ) << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( errorId );

		//	MessageBox( _hDlg, sstr.str().c_str(), L"Cannot Read File", MB_ICONEXCLAMATION | MB_OK );
			_errorMessage = sstr.str();
		}

		return errorId != ERROR_BAD_FORMAT;

		/*CTextFileReader reader( iStream, &_worker, _useEncoding, _useCodePage );

		_useEncoding = reader.getEncoding();
		_fileEol = reader.getEol();

		// ask user what to do when text file is too big (over 16MB)
		if( ( reader.isText() || _useEncoding != StringUtils::NOBOM ) && _fileSize.QuadPart > 0xFFFFFFll &&
			MessageBox(
				_hDlg,
				L"File is too big to display as text.\nOperation may take a long time.\nContinue displaying as text?", L"File too large",
				MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2 ) == IDNO )
			return false;

		CheckMenuRadioItem( GetSubMenu( _hMenu, 4 ), IDM_VIEWER_ENCODING_AUTO, IDM_VIEWER_ENCODING_ANSI, CFileViewer::getEncodingId( _useEncoding ), MF_BYCOMMAND | MF_CHECKED );

		// read file content as text
		DWORD errorId;

		if( _sortLines ) // TODO: false by default
		{
			std::vector<std::wstring> lines;

			while( ( errorId = reader.readLine() ) == ERROR_SUCCESS )
				lines.push_back( reader.getLineRef() );

			std::sort( lines.begin(), lines.end(), []( const auto& a, const auto& b ) { return StrCmpLogicalW( &a[0], &b[0] ) < 0; } );

			for( auto it = lines.begin(); it != lines.end(); ++it )
				_outText += *it + L"\r\n";
		}
		else
		{
			while( ( errorId = reader.readLine() ) == ERROR_SUCCESS )
			{
				_outText += reader.getLineRef() + L"\r\n";
			}
		}

		if( errorId != ERROR_SUCCESS && errorId != ERROR_HANDLE_EOF && errorId != ERROR_BAD_FORMAT )
		{
			std::wostringstream sstr;
			sstr << L"Path: \"" << _fileName << L"\"\r\n";
			sstr << L"Encoding: \"" << StringUtils::encodingToStr( _useEncoding ) << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( errorId );

			MessageBox( _hDlg, sstr.str().c_str(), L"Cannot Read File", MB_ICONEXCLAMATION | MB_OK );
		}

		return errorId != ERROR_BAD_FORMAT;*/
	}

	bool CFileEditor::searchText( bool reverse )
	{
		if( !_textToFind.empty() )
		{
			SetCursor( LoadCursor( NULL, IDC_WAIT ) );

			std::wostringstream sstr;
			sstr << L"Searching" << ( _findParams.hexMode ? L" hex: " : L": " ) << _textToFind;
			SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)sstr.str().c_str() );

			// this is needed to prevent when user presses right arrow key right after search,
			// the caret stays at the same position and the _findFromIdxBeg won't get updated
			SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&_findFromIdxBeg, (LPARAM)&_findFromIdxEnd );

			try {
				CEditControlHelper edh( _hActiveEdit );

				int idxBeg = -1, idxEnd = -1;
				std::wstring textToFind = _findParams.hexMode ? _textHexToFind : _textToFind;
				if( findText( edh.getText(), edh.getTextLength(), textToFind, reverse ? _findFromIdxBeg : _findFromIdxEnd, reverse, idxBeg, idxEnd ) )
				{
					SendMessage( _hActiveEdit, EM_SETSEL, (WPARAM)idxBeg, (LPARAM)idxEnd );
					SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

					SetCursor( NULL );
					SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"" );

					updateStatusBarCaretPos( true );

					return true;
				}
				else
				{
					SetCursor( NULL );
					SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"Not found" );

					std::wstringstream sstr;
					sstr << L"Cannot find " << ( _findParams.hexMode ? L"hex \"" : L"\"" ) << _textToFind << L"\".";
					_pFindDlg->showMessageBox( sstr.str() );
				}
			}
			catch( const std::regex_error& e )
			{
				CFileViewer::reportRegexError( _hDlg, e, _textToFind );
			}
		}
		else
			findTextDialog();

		return false;
	}

	void CFileEditor::replaceText( bool replaceAll )
	{
		_ASSERTE( !_textToFind.empty() );

		if( preSearchText() )
		{
			std::wstring textToFind = _findParams.hexMode ? _textHexToFind : _textToFind;
			std::wstring textToReplace = _textToReplace;

			if( replaceAll )
			{
				CEditControlHelper edh( _hActiveEdit );

				// search from the beginning
				SendMessage( _hActiveEdit, EM_SETSEL, 0, 0 );

				std::vector<std::tuple<int, size_t, std::wstring>> indices;

				bool found = searchText( false );
				int idxBeg = _findFromIdxBeg, idxEnd = _findFromIdxEnd;

				SetCursor( LoadCursor( NULL, IDC_WAIT ) );

				try {
					// make list of found occurrences first
					while( found )
					{
						if( _findParams.regexMode )
							textToReplace = std::regex_replace( edh.getSubstr( idxBeg, idxEnd - idxBeg ), *_regex, _textToReplace );

						indices.push_back( std::make_tuple( idxBeg, idxEnd - idxBeg, textToReplace ) );

						// look for another occurrence
						found = findText( edh.getText(), edh.getTextLength(), textToFind, idxEnd, false, idxBeg, idxEnd );
					}
				}
				catch( const std::regex_error& e )
				{
					CFileViewer::reportRegexError( _hDlg, e, _textToFind );
				}

				if( indices.size() > 1 ) // more than one occurrence has been found
				{
					std::wstring outText = edh.getText();

					for( auto it = indices.rbegin(); it != indices.rend(); ++it )
					{
						outText.replace( std::get<0>( *it ), std::get<1>( *it ), std::get<2>( *it ) );
					}

					// update all the text in edit control at once - so we can undo
					SendMessage( _hActiveEdit, EM_SETSEL, 0, (LPARAM)-1 );
					SendMessage( _hActiveEdit, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)&outText[0] );
				}
				else if( !indices.empty() ) // only one occurrence found
				{
					SendMessage( _hActiveEdit, EM_SETSEL, std::get<0>( indices[0] ), std::get<0>( indices[0] ) + std::get<1>( indices[0] ) );
					SendMessage( _hActiveEdit, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)textToReplace.c_str() );
				}

				SetCursor( NULL );

				// reset caret position after replace-all
				SendMessage( _hActiveEdit, EM_SETSEL, 0, 0 );
				SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

				updateStatusBarCaretPos( true );
			}
			else
			{
				int idxBeg = 0, idxEnd = 0;
				SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

				if( idxBeg != idxEnd )
				{
					SetCursor( LoadCursor( NULL, IDC_WAIT ) );

					try {
						CEditControlHelper edh( _hActiveEdit );

						// when text has already been found, just replace it and look for next
						int idxBeg2 = 0, idxEnd2 = 0;
						if( findText( edh.getText(), edh.getTextLength(), textToFind, idxBeg, false, idxBeg2, idxEnd2 ) && idxBeg == idxBeg2 && idxEnd == idxEnd2 )
						{
							if( _findParams.regexMode )
								textToReplace = std::regex_replace( edh.getSubstr( idxBeg, idxEnd - idxBeg ), *_regex, _textToReplace );

							SendMessage( _hActiveEdit, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)&textToReplace[0] );
						}
					}
					catch( const std::regex_error& e )
					{
						CFileViewer::reportRegexError( _hDlg, e, _textToFind );
					}

					SetCursor( NULL );
				}

				searchText( _findParams.reverse );
			}
		}
	}

	bool CFileEditor::preSearchText()
	{
		if( _findParams.regexMode )
		{
			auto flags = std::regex_constants::ECMAScript;

			if( !_findParams.matchCase )
				flags |= std::regex_constants::icase;

			try {
				_regex = std::make_unique<std::wregex>( _textToFind, flags );
			}
			catch( const std::regex_error& e )
			{
				CFileViewer::reportRegexError( _hDlg, e, _textToFind );

				return false;
			}
		}
		else if( _findParams.hexMode )
		{
			std::string hexData;
			if( StringUtils::convertHex( _textToFind, hexData, _useCodePage ) )
			{
				// try to convert hex-data to text
				if( !StringUtils::convert2Utf16( hexData, _textHexToFind, _useEncoding == StringUtils::ANSI ? _useCodePage : _useEncoding ) )
				{
					std::wstringstream sstr;
					sstr << L"Error converting \"" << _textHexToFind << L"\" to text ";

					if( _useEncoding == StringUtils::ANSI )
						sstr << L"(Codepage: " << _useCodePage << L").";
					else
						sstr << L"(" << StringUtils::encodingToStr( _useEncoding ) << L").";

					MessageBox( _hDlg, sstr.str().c_str(), L"Conversion Failed", MB_ICONEXCLAMATION | MB_OK );

					return false;
				}
			}
			else
			{
				std::wstringstream sstr;
				sstr << L"Error converting hexadecimal string.\r\n'" << _textToFind << L"'";

				MessageBox( _hDlg, sstr.str().c_str(), L"Conversion Failed", MB_ICONEXCLAMATION | MB_OK );

				return false;
			}
		}

		return true;
	}

	bool CFileEditor::findText( wchar_t *buf, size_t buf_len, const std::wstring& text, int offset, bool reverse, int& idxBeg, int& idxEnd )
	{
		_ASSERTE( !text.empty() );

		bool ret = false;
		idxBeg = idxEnd = -1;

		if( buf )
		{
			if( _findParams.regexMode )
			{
				// TODO: reverse search very inefficient
				auto it = std::wcregex_iterator( buf + ( reverse ? 0 : offset ), ( reverse ? buf + offset : buf + buf_len ), *_regex );

				for( ; it != std::wcregex_iterator(); ++it )
				{
					auto idx = it->position() + ( reverse ? 0 : offset );
					auto len = it->length();

					if( !_findParams.wholeWords || ( ( idx == 0 || !StringUtils::isAlphaNum( buf[idx] ) || !StringUtils::isAlphaNum( buf[idx - 1] ) ) &&
						( !len || buf_len == idx + len || !StringUtils::isAlphaNum( buf[idx + len - 1] ) || !StringUtils::isAlphaNum( buf[idx + len] ) ) ) )
					{
						idxBeg = static_cast<int>( idx );
						idxEnd = idxBeg + static_cast<int>( len );

						ret = true;

						if( !reverse )
							break;
					}
				}
			}
			else
			{
				bool bounds[2], wholeWords = _findParams.wholeWords;

				bounds[0] = StringUtils::isAlphaNum( text[0] );
				bounds[1] = StringUtils::isAlphaNum( text[text.length() - 1] );

				// when there are non-alpha-numeric characters (bounds left and right) in the source string, we don't need wholewords flag
				if( wholeWords && !bounds[0] && !bounds[1] )
					wholeWords = false;

				const wchar_t *p = StringUtils::findStr( buf, buf_len, text, _findParams.matchCase, wholeWords, bounds, (size_t)offset, reverse );

				if( p )
				{
					idxBeg = static_cast<int>( p - buf );
					idxEnd = idxBeg + static_cast<int>( text.length() );
					ret = true;
				}
			}
		}

		return ret;
	}

	void CFileEditor::handleDropFiles( HDROP hDrop )
	{
		auto items = ShellUtils::getDroppedItems( hDrop );
	
		DragFinish( hDrop );

		if( !items.empty() )
			viewFile( PathUtils::stripFileName( items[0] ), PathUtils::stripPath( items[0] ) );
	}

	void CFileEditor::gotoLineDialog()
	{
		int lineCount = (int)SendMessage( _hActiveEdit, EM_GETLINECOUNT, 0, 0 );
		int curLineIdx = (int)SendMessage( _hActiveEdit, EM_LINEFROMCHAR, (WPARAM)-1, 0 );

		CTextInput::CParams params(
				L"Go To Line", FORMAT( L"Line number (1 - %d):", lineCount ),
				std::to_wstring( curLineIdx + 1 ) );

		auto text = MiscUtils::showTextInputBox( params, _hDlg );

		StringUtils::trim( text );

		// parse text and scroll into requested position
		if( !text.empty() )
		{
			int pos = 0;

			try {
				pos = std::stol( text ) - 1;

				pos = pos < 0 ? 0 : pos >= lineCount ? lineCount - 1: pos;

				auto idx = SendMessage( _hActiveEdit, EM_LINEINDEX, pos, 0 );
				SendMessage( _hActiveEdit, EM_SETSEL, (WPARAM)idx, (LPARAM)idx );
				SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );
			}
			catch( std::invalid_argument ) {
				MessageBox( _hDlg, FORMAT( L"Cannot convert \"%ls\" to number.",
					text.c_str() ).c_str(), L"Invalid Number", MB_ICONEXCLAMATION | MB_OK );
			}
			catch( std::out_of_range ) {
				MessageBox( _hDlg, FORMAT( L"Number \"%ls\" is out of range.",
					text.c_str() ).c_str(), L"Number Out of Range", MB_ICONEXCLAMATION | MB_OK );
			}
		}
	}

	void CFileEditor::findTextDialog( bool replace )
	{
		std::wstring text = _textToFind;

		if( !_findParams.hexMode && !_findParams.regexMode )
			text = EdUtils::getSelection( _hActiveEdit );

		_pFindDlg->updateText( text.empty() ? _textToFind : text );
		_pFindDlg->updateParams( _findParams, replace );
		_pFindDlg->show();
	}

	void CFileEditor::swapActiveEdit()
	{
		SetCursor( LoadCursor( NULL, IDC_WAIT ) );

		_wordWrap = !_wordWrap;

		// update word wrap menu item state
		MenuUtils::checkItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_WRAP, _wordWrap );

		// store client rect, caret position, current selection and modified status
		RECT rct;
		GetClientRect( _hDlg, &rct );

		DWORD idxBeg = 0, idxEnd = 0;
		SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );
		bool modified = !!SendMessage( _hActiveEdit, EM_GETMODIFY, 0, 0 );

		// disable soon to be inactive edit control and remove its subclass procedure
		EnableWindow( _hActiveEdit, FALSE );
		RemoveWindowSubclass( _hActiveEdit, wndProcEditSubclass, 0 );

		// create new edit control and switch active
		HWND hInactiveEdit = _hActiveEdit;
		_hActiveEdit = createEditControl();

		// update newly active edit-box size
		updateLayout( rct.left + rct.right, rct.top + rct.bottom );

		// update text in newly active (no)wrap edit-box
		std::wstring outText;
		if( MiscUtils::getWindowText( hInactiveEdit, outText ) )
			SetWindowText( _hActiveEdit, outText.c_str() );

		// restore current zoom
		SendMessage( _hActiveEdit, EM_SETZOOM, (WPARAM)( ( (double)_currentZoom / 100. ) * 985. ), (LPARAM)1000 );

		// restore modified status (text has been changed)
		SendMessage( _hActiveEdit, EM_SETMODIFY, (WPARAM)modified, 0 );

		// swap edit-boxes at once
		HDWP hdwp = BeginDeferWindowPos( 2 );
		if( hdwp ) hdwp = DeferWindowPos( hdwp, _hActiveEdit,  NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
		if( hdwp ) hdwp = DeferWindowPos( hdwp, hInactiveEdit, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW );
		if( hdwp ) EndDeferWindowPos( hdwp );

		// restore selection and caret position from previously active edit-box
		SendMessage( _hActiveEdit, EM_SETSEL, static_cast<WPARAM>( idxBeg ), static_cast<LPARAM>( idxEnd ) );
		SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

		DestroyWindow( hInactiveEdit );
		SetFocus( _hActiveEdit );

		SetCursor( NULL );
	}

	void CFileEditor::showStatusBar()
	{
		bool isVisible = !!IsWindowVisible( _hStatusBar );
		ShowWindow( _hStatusBar, isVisible ? SW_HIDE : SW_SHOWNORMAL );

		MenuUtils::checkItem( GetSubMenu( _hMenu, 3 ), IDM_VIEWER_VIEW_STATUSBAR, !isVisible );

		RECT rct;
		GetClientRect( _hDlg, &rct );

		// update layout
		updateLayout( rct.left + rct.right, rct.top + rct.bottom );
	}

	void CFileEditor::setZoom( int value )
	{
		int zoom = _currentZoom;

		if( ( value == 0 ) || ( value > 0 && zoom < 500 ) || ( value < 0 && zoom > 10 ) )
		{
			zoom += value > 0 ? 10 : value < 0 ? -10 : -zoom + 100;

			if( SendMessage( _hActiveEdit, EM_SETZOOM, (WPARAM)( ( (double)zoom / 100. ) * 985. ), (LPARAM)1000 ) )
			{
				// set status-bar current zoom as text
				std::wstring zoomStr = std::to_wstring( zoom ) + L"%";
				SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 2, SBT_NOBORDERS ), (LPARAM)zoomStr.c_str() );

				_currentZoom = zoom;
			}
			else
				_currentZoom = 100;
		}
	}

	bool CFileEditor::selectCurrentWord( LONG x, LONG y )
	{
		CEditControlHelper editHelper( _hActiveEdit );

		_tripleClickPos.x = x;
		_tripleClickPos.y = y;
		_tripleClickTime = GetTickCount();

		// select current word at caret position
		return editHelper.selectCurrentWord();
	}

	bool CFileEditor::selectCurrentLine( LONG x, LONG y )
	{
		CEditControlHelper editHelper( _hActiveEdit );

		// check for a triple-click
		if( _tripleClickTime && ( _tripleClickPos.x == x && _tripleClickPos.y == y )
			&& GetTickCount() - _tripleClickTime < GetDoubleClickTime() )
		{
			_tripleClickTime = 0;
			_tripleClickPos = { 0, 0 };

			// select entire line at the caret position
			return editHelper.selectCurrentLine();
		}
		else
		{
			_tripleClickTime = 0;
			_tripleClickPos = { 0, 0 };
		}

		return false;
	}

	LRESULT CALLBACK CFileEditor::wndProcEdit( HWND hWnd, UINT message, WPARAM wp, LPARAM lp )
	{
	//	PrintDebug("msg: 0x%04X, wpar: 0x%04X, lpar: 0x%04X", message, wp, lp);
		switch( message )
		{
		case WM_GETDLGCODE:
			return DLGC_WANTARROWS | DLGC_WANTTAB | DLGC_WANTALLKEYS;

		case WM_CHAR:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				// disable edit control's default shortcuts handling - prevents system beep sound
				switch( wp + 'A' - 1 )
				{
				case 'H': if( lp == 0 ) break; // after Undo shortcut (Ctrl+Z), Ctrl+H is sent - do default
					// fall-through otherwise
				case 'A': case 'B': case 'F': case 'G': case 'N': case 'O': case 'P': case 'R': case 'S': case 'W':
					return 0;
				}
			}
			break;

		case WM_KEYDOWN:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				if( handleKeyboardShortcuts( wp ) )
					return 0;
			}
			else switch( wp )
			{
			case VK_F3:
			{
				// invert direction when holding shift key
				bool shift = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
				searchText( ( _findParams.reverse && !shift ) || ( !_findParams.reverse && shift ) );
				break;
			}
			}
			break;

		case WM_MOUSEWHEEL:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				setZoom( GET_WHEEL_DELTA_WPARAM( wp ) );
			break;

		case WM_LBUTTONDBLCLK:
			if( selectCurrentWord( GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ) ) )
				return 0;
			break;

		case WM_LBUTTONDOWN:
			if( selectCurrentLine( GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ) ) )
				return 0;
			break;

		case WM_SETCURSOR:
			if( _worker.isRunning() )
			{
				SetCursor( LoadCursor( NULL, IDC_WAIT ) );
				return 0;
			}
			break;
		}

		return DefSubclassProc( hWnd, message, wp, lp );
	}

	bool CFileEditor::handleKeyboardShortcuts( WPARAM wParam )
	{
		switch( wParam )
		{
		case VK_NUMPAD0: // 0 - restore default zoom
		case 0x30:
			setZoom( 0 );
			break;
		case VK_ADD: // [+] - zoom-in
		case 0xBB:
			setZoom( 1 );
			break;
		case VK_SUBTRACT: // [-] - zoom-out
		case 0xBD:
			setZoom( -1 );
			break;
		case 0x41: // A - select all
			selectAll();
			break;
		case 0x42: // B - show status-bar
			showStatusBar();
			break;
		case 0x43: // C - copy to file
			if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
				saveFileAs( true ); // Ctrl+Shift+C
			break;
		case 0x46: // F - show find dialog
			findTextDialog();
			break;
		case 0x47: // G - goto line dialog
			gotoLineDialog();
			break;
		case 0x48: // H - show find & replace dialog
			findTextDialog( true );
			break;
		case 0x4E: // N - find next
			searchText( _findParams.reverse );
			break;
		case 0x4F: // O - open file
			openFileDialog();
			break;
		case 0x50: // P - find previous
			searchText( !_findParams.reverse );
			break;
		case 0x52: // R - refresh
			viewFileCore( _useEncoding );
			break;
		case 0x53: // S - save file dialog
			if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
				saveFileAs(); // SaveAs: Ctrl+Shift+S
			else
				preSaveFile();
			break;
		case 0x57: // W - word wrap
			swapActiveEdit();
			break;
		default:
		//	PrintDebug("button: 0x%02X", wParam);
			return false;
		}

		return true;
	}

	void CFileEditor::selectAll()
	{
		SendMessage( _hActiveEdit, EM_SETSEL, 0, (LPARAM)-1 );
		SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );
	}

	void CFileEditor::openFileDialog()
	{
		std::wstring fileName = PathUtils::stripPath( _fileName );

		if( MiscUtils::openFileDialog( _hDlg, PathUtils::stripFileName( _fileName ), fileName ) )
		{
			viewFile( PathUtils::stripFileName( fileName ), PathUtils::stripPath( fileName ) );
		}
	}

	bool CFileEditor::saveFileDialog( std::wstring& fileName, DWORD& encodingId )
	{
		CComPtr<IFileSaveDialog> pDlg;
		HRESULT hr = pDlg.CoCreateInstance( __uuidof( FileSaveDialog ) );

		if( SUCCEEDED( hr ) )
		{
			COMDLG_FILTERSPEC fileTypes[] = {
				{ L"Text Files (*.txt)", L"*.txt" },
				{ L"All Files (*.*)", L"*.*" }
			};

			pDlg->SetFileTypes( _countof( fileTypes ), fileTypes );

			FILEOPENDIALOGOPTIONS pfos;
			pDlg->GetOptions( &pfos );
			pDlg->SetOptions( pfos | FOS_OVERWRITEPROMPT );

			CComPtr<IShellItem> pFolder;
			auto dirname = PathUtils::stripFileName( fileName );
			hr = SHCreateItemFromParsingName( dirname.c_str(), nullptr, IID_PPV_ARGS( &pFolder ) );
			pDlg->SetFolder( pFolder );

			auto fname = PathUtils::stripPath( fileName );
			pDlg->SetFileName( fname.c_str() );
			pDlg->SetDefaultExtension( L"txt" );

			// get an IFileDialogCustomize interface and add text encoding combo-box
			CComQIPtr<IFileDialogCustomize> pfdc = pDlg;
			if( pfdc )
			{
				// setting label to combo-box only works through visual groups
				pfdc->StartVisualGroup( IDC_SAVEDIALOG_COMBO - 1, L"&Encoding:" );
				pfdc->AddComboBox( IDC_SAVEDIALOG_COMBO );
				pfdc->EndVisualGroup();
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_ANSI, L"ANSI" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF8, L"UTF-8" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF8BOM, L"UTF-8 with BOM" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF16LE, L"UTF-16 LE" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF16BE, L"UTF-16 BE" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF32LE, L"UTF-32 LE" );
				pfdc->AddControlItem( IDC_SAVEDIALOG_COMBO, IDM_VIEWER_ENCODING_UTF32BE, L"UTF-32 BE" );
				pfdc->SetSelectedControlItem( IDC_SAVEDIALOG_COMBO, encodingId );
			}

			hr = pDlg->Show( _hDlg ); // show the dialog
			if( SUCCEEDED( hr ) )
			{
				CComPtr<IShellItem> pItem;
				hr = pDlg->GetResult( &pItem );

				if( SUCCEEDED( hr ) )
				{
					LPOLESTR pwsz = NULL;
					hr = pItem->GetDisplayName( SIGDN_FILESYSPATH, &pwsz );

					if( SUCCEEDED( hr ) )
					{
						fileName = pwsz;

						if( pfdc )
							pfdc->GetSelectedControlItem( IDC_SAVEDIALOG_COMBO, &encodingId );

						CoTaskMemFree( pwsz );

						return true;
					}
				}
			}
		}

		return false;
	}

	bool CFileEditor::preSaveFile()
	{
		if( !saveFile( _tempFileName.empty() ? _fileName : _tempFileName ) )
			return saveFileAs();

		return true;
	}

	bool CFileEditor::saveFile( const std::wstring& fileName, DWORD encodingId, bool selectionOnly )
	{
		bool retVal = false;

		if( !fileName.empty() )
		{
			CEditControlHelper editHelper( _hActiveEdit );

			wchar_t *text = editHelper.getText();
			size_t len = editHelper.getTextLength();

			if( selectionOnly )
			{
				// check if there is some selected text
				DWORD idxBeg = 0, idxEnd = 0;
				SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&idxBeg, (LPARAM)&idxEnd );

				if( idxBeg != idxEnd )
				{
					text += idxBeg; // selected text only
					len = idxEnd - idxBeg;
				}
			}

			if( encodingId == IDM_VIEWER_ENCODING_AUTO )
				encodingId = CFileViewer::getEncodingId( _useEncoding, _fileBom );

			switch( encodingId )
			{
			case IDM_VIEWER_ENCODING_ANSI:
				retVal = FsUtils::saveFileAnsi( fileName, text, len );
				break;
			case IDM_VIEWER_ENCODING_UTF8:
				retVal = FsUtils::saveFileUtf8( fileName, text, len );
				break;
			case IDM_VIEWER_ENCODING_UTF8BOM:
				retVal = FsUtils::saveFileUtf8( fileName, text, len, true );
				break;
			case IDM_VIEWER_ENCODING_UTF16LE:
				retVal = FsUtils::saveFileUtf16( fileName, text, len );
				break;
			case IDM_VIEWER_ENCODING_UTF16BE:
				retVal = FsUtils::saveFileUtf16( fileName, text, len, true );
				break;
			case IDM_VIEWER_ENCODING_UTF32LE:
				retVal = FsUtils::saveFileUtf32( fileName, text, len );
				break;
			case IDM_VIEWER_ENCODING_UTF32BE:
				retVal = FsUtils::saveFileUtf32( fileName, text, len, true );
				break;
			default:
				retVal = FsUtils::saveFileUtf8( fileName, text, len );
				break;
			}
		}

		if( retVal )
		{
			// clear modified status and reread file with given encoding
			SendMessage( _hActiveEdit, EM_SETMODIFY, (WPARAM)FALSE, 0 );
			_dlgUserDataPtr = static_cast<LPARAM>( CFileViewer::getEncodingFromId( encodingId ) );
			viewFile( PathUtils::stripFileName( fileName ), PathUtils::stripPath( fileName ) );
		}

		return retVal;
	}

	bool CFileEditor::saveFileAs( bool selectionOnly )
	{
		std::wstring fileName = _tempFileName.empty() ? _fileName : _tempFileName;

		bool retVal = false;

		DWORD dwEncodingId = CFileViewer::getEncodingId( _useEncoding, _fileBom );

		if( saveFileDialog( fileName, dwEncodingId ) )
		{
			retVal = saveFile( PathUtils::getFullPath( fileName ), dwEncodingId, selectionOnly );
		}

		return retVal;
	}

	bool CFileEditor::askSaveChanges( bool modified )
	{
		if( modified )
		{
			int ret = MessageBox( _hDlg, L"File has been modified. Save changes?", L"Editor", MB_YESNOCANCEL | MB_ICONQUESTION );

			switch( ret )
			{
			case IDYES:
				return preSaveFile();
			case IDNO:
				// discard changes
				break;
			case IDCANCEL:
			default:
				return false;
			}
		}

		return true;
	}

	void CFileEditor::handleMenuCommands( WORD menuId )
	{
		switch( menuId )
		{
		case IDM_VIEWER_FILE_OPEN:
			openFileDialog();
			break;
		case IDM_VIEWER_FILE_SAVE:
			preSaveFile();
			break;
		case IDM_VIEWER_FILE_SAVEAS:
			saveFileAs();
			break;
		case IDM_VIEWER_FILE_REFRESH:
			viewFileCore( _useEncoding );
			break;
		case IDM_VIEWER_FILE_EXIT:
			close(); // close dialog
			break;
		case IDM_VIEWER_EDIT_UNDO:
			SendMessage( _hActiveEdit, WM_UNDO, 0, 0 );
			break;
		case IDM_VIEWER_EDIT_CUT:
			SendMessage( _hActiveEdit, WM_CUT, 0, 0 );
			break;
		case IDM_VIEWER_EDIT_COPY:
			SendMessage( _hActiveEdit, WM_COPY, 0, 0 );
			break;
		case IDM_VIEWER_EDIT_PASTE:
			SendMessage( _hActiveEdit, WM_PASTE, 0, 0 );
			break;
		case IDM_VIEWER_EDIT_DELETE:
			SendMessage( _hActiveEdit, WM_CLEAR, 0, 0 );
			break;
		case IDM_VIEWER_EDIT_COPYTOFILE:
			saveFileAs( true );
			break;
		case IDM_VIEWER_EDIT_SELECTALL:
			selectAll();
			break;
		case IDM_VIEWER_SEARCH_FIND:
			findTextDialog();
			break;
		case IDM_VIEWER_SEARCH_FINDNEXT:
			searchText( _findParams.reverse );
			break;
		case IDM_VIEWER_SEARCH_FINDPREVIOUS:
			searchText( !_findParams.reverse );
			break;
		case IDM_VIEWER_SEARCH_REPLACE:
			findTextDialog( true );
			break;
		case IDM_VIEWER_SEARCH_GOTO:
			gotoLineDialog();
			break;
		case IDM_VIEWER_VIEW_ZOOM_IN:
			setZoom( 1 );
			break;
		case IDM_VIEWER_VIEW_ZOOM_OUT:
			setZoom( -1 );
			break;
		case IDM_VIEWER_VIEW_ZOOM_RESTORE:
			setZoom( 0 );
			break;
		case IDM_VIEWER_VIEW_WRAP:
			swapActiveEdit();
			break;
		case IDM_VIEWER_VIEW_STATUSBAR:
			showStatusBar();
			break;
		case IDM_VIEWER_ENCODING_AUTO:
			viewFileCore( StringUtils::NOBOM );
			break;
		case IDM_VIEWER_ENCODING_UTF8:
			viewFileCore( StringUtils::UTF8 );
			break;
		case IDM_VIEWER_ENCODING_UTF16LE:
			viewFileCore( StringUtils::UTF16LE );
			break;
		case IDM_VIEWER_ENCODING_UTF16BE:
			viewFileCore( StringUtils::UTF16BE );
			break;
		case IDM_VIEWER_ENCODING_UTF32LE:
			viewFileCore( StringUtils::UTF32LE );
			break;
		case IDM_VIEWER_ENCODING_UTF32BE:
			viewFileCore( StringUtils::UTF32BE );
			break;
		case IDM_VIEWER_ENCODING_ANSI:
			viewFileCore( StringUtils::ANSI );
			break;
		case IDM_VIEWER_ENCODING_CP1250:
		case IDM_VIEWER_ENCODING_ISO88592:
		case IDM_VIEWER_ENCODING_CP1252:
		case IDM_VIEWER_ENCODING_ISO88591:
		case IDM_VIEWER_ENCODING_CP1251:
		case IDM_VIEWER_ENCODING_ISO88595:
		case IDM_VIEWER_ENCODING_CP866:
		case IDM_VIEWER_ENCODING_KOI8:
		case IDM_VIEWER_ENCODING_IBM437:
			viewFileAnsi( menuId );
			break;
		}
	}

	void CFileEditor::updateMenuEncoding()
	{
		// update encodings state
		HMENU hMenu4 = GetSubMenu( _hMenu, 4 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP1250, _useCodePage == 1250 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP1251, _useCodePage == 1251 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP1252, _useCodePage == 1252 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_ISO88591, _useCodePage == 28591 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_ISO88592, _useCodePage == 28592 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_ISO88595, _useCodePage == 28595 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_CP866, _useCodePage == 866 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_KOI8, _useCodePage == 20866 );
		MenuUtils::checkItem( hMenu4, IDM_VIEWER_ENCODING_IBM437, _useCodePage == 437 );

		// check current encoding
		CheckMenuRadioItem( hMenu4, IDM_VIEWER_ENCODING_AUTO, IDM_VIEWER_ENCODING_ANSI,
			CFileViewer::getEncodingId( _useEncoding ), MF_BYCOMMAND | MF_CHECKED );
	}

	void CFileEditor::viewFileAnsi( WORD menuId )
	{
		switch( menuId )
		{
		case IDM_VIEWER_ENCODING_CP1250:
			_useCodePage = 1250;
			break;
		case IDM_VIEWER_ENCODING_ISO88592:
			_useCodePage = 28592;
			break;
		case IDM_VIEWER_ENCODING_CP1252:
			_useCodePage = 1252;
			break;
		case IDM_VIEWER_ENCODING_ISO88591:
			_useCodePage = 28591;
			break;
		case IDM_VIEWER_ENCODING_CP1251:
			_useCodePage = 1251;
			break;
		case IDM_VIEWER_ENCODING_ISO88595:
			_useCodePage = 28595;
			break;
		case IDM_VIEWER_ENCODING_CP866:
			_useCodePage = 866;
			break;
		case IDM_VIEWER_ENCODING_KOI8:
			_useCodePage = 20866;
			break;
		case IDM_VIEWER_ENCODING_IBM437:
			_useCodePage = 437;
			break;
		}

		viewFileCore( StringUtils::ANSI );
	}

	void CFileEditor::onFindDialogNotify( int cmd )
	{
		switch( cmd )
		{
		case 0: // CFindText dialog is being destroyed
			_pFindDlg = nullptr;
			break;
		case 5: // search previous
		case 4: // search next
			searchText( cmd == 4 ? _findParams.reverse : !_findParams.reverse );
			break;
		case 3: // perform replace-all text
		case 2: // perform replace text
			_textToReplace = _pFindDlg->getReplaceText();
			// fall-through
		case 1: // perform search text
			_textToFind = _pFindDlg->result();
			_findParams = _pFindDlg->getParams();

			if( cmd == 1 )
				hiliteText();
			else
				replaceText( cmd == 3 );
			break;
		default:
			PrintDebug("unknown command: %d", cmd);
			break;
		}
	}

	INT_PTR CALLBACK CFileEditor::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_ACTIVATE:
			if( LOWORD( wParam ) != WA_INACTIVE )
			{
				// monitor caret location change event
				_hEventHook = SetWinEventHook( EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, eventProcEditControl, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT );
			}
			else
			{
				if( _hEventHook ) UnhookWinEvent( _hEventHook );
			}
			break;

		case WM_COMMAND:
			switch( HIWORD( wParam ) )
			{
			case EN_ERRSPACE:
				MessageBox( _hDlg, L"Edit control cannot allocate enough memory.", L"Editor", MB_OK | MB_ICONERROR );
				PrintDebug("EN_ERRSPACE");
				break;
			case EN_CHANGE:
				updateWindowTitle();
				break;
			default:
				handleMenuCommands( LOWORD( wParam ) );
				break;
			}
			break;

		case WM_MENUSELECT:
			switch( LOWORD( wParam ) )
			{
			case 1:
				updateMenuEdit();
				break;
			case 3:
				updateMenuView();
				break;
			}
			break;

		case UM_REPORTMSGBOX:
			_pFindDlg->messageBoxClosed();
			break;

		case UM_CARETPOSCHNG:
			updateStatusBarCaretPos();
			break;

		case UM_FINDLGNOTIFY:
			if( wParam == IDE_FINDTEXT_TEXTFIND ) // from CFindText dialog
				onFindDialogNotify( static_cast<int>( lParam ) );
			break;

		case UM_WORKERNOTIFY:
			onWorkerFinished( !!wParam );
			break;

		case UM_READERNOTIFY:
			onReaderFinished( !!wParam );
			break;

		case WM_DROPFILES:
			handleDropFiles( (HDROP)wParam );
			break;

		case WM_SIZE:
			updateLayout( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		}

		return (INT_PTR)false;
	}

	void CFileEditor::updateLayout( int width, int height )
	{
		int statusBarHeight = 0;

		// get status-bar height when visible
		if( IsWindowVisible( _hStatusBar ) )
		{
			int borderWidths[3];
			RECT rct;

			if( SendMessage( _hStatusBar, SB_GETRECT, 0, (LPARAM)&rct ) &&
				SendMessage( _hStatusBar, SB_GETBORDERS, 0, (LPARAM)borderWidths ) )
			{
				statusBarHeight = rct.bottom - rct.top + borderWidths[1];
			}

			// initialize status-bar parts
			const int numParts = 5;
			const int partWidths[numParts] = { 410, 270, 220, 120, 0 };

			// calculate right edges of parts according to current width
			int statusBarParts[numParts] = {
				width > partWidths[0] ? width - partWidths[0] : 1,
				width > partWidths[1] ? width - partWidths[1] : 1,
				width > partWidths[2] ? width - partWidths[2] : 1,
				width > partWidths[3] ? width - partWidths[3] : 1,
				width > partWidths[4] ? width - partWidths[4] : 1 };

			// update status-bar window placement
			SendMessage( _hStatusBar, SB_SETPARTS, numParts, (LPARAM)statusBarParts );
			SendMessage( _hStatusBar, WM_SIZE, 0, 0 );
		}

		// update layout accordingly
		MoveWindow( _hActiveEdit, 0, 0, width, height - statusBarHeight, TRUE );
	}
}
