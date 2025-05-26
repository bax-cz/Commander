#include "stdafx.h"

#include "Commander.h"
#include "CompareFiles.h"
#include "FileViewer.h"
#include "TextFileReader.h"
#include "IconUtils.h"
#include "MiscUtils.h"

#define INSERT_TOOLBAR_BUTTON( style, cmd, text, ibmp ) \
	tbb.push_back( TBBUTTON{ (ibmp), (cmd), TBSTATE_ENABLED, (style), { 0 }, 0, ( (text) ? SendMessage( _hToolBar, TB_ADDSTRING, 0, (LPARAM)(text) ) : 0 ) } );

#define TBCMD_DIFFPREVIOUS     101
#define TBCMD_DIFFNEXT         102
#define TBCMD_DIFFCURRENT      103
#define TBCMD_DIFFFIRST        104
#define TBCMD_DIFFLAST         105
#define TBCMD_IGNORECASE       106
#define TBCMD_IGNOREWHITESPACE 107
#define TBCMD_IGNOREEOL        108
#define TBCMD_REFRESH          109

namespace Commander
{
	CCompareFiles::CCompareFiles()
		: _pFindDlg( nullptr )
		, _hToolBar( nullptr )
		, _hEventHook( nullptr )
		, _hActiveEdit( nullptr )
		, _hRichEditLib( nullptr )
		, _imgListToolbar( nullptr )
		, _imgListToolbarHot( nullptr )
		, _imgListToolbarDisabled( nullptr )
		, _binaryMode( false )
		, _findParams{ false }
		, _findFromIdxBeg( 0 )
		, _findFromIdxEnd( 0 )
		, _linesCountPerPage( 0 )
		, _options{ 0 }
		, _curDiff( -1 )
		, _curZoom( 100 )
	{
		// initialize Rich Edit 2.0 class
		_hRichEditLib = LoadLibrary( L"Riched20.dll" );

		if( _hRichEditLib == nullptr )
			// initialize Rich Edit 1.0 class
			_hRichEditLib = LoadLibrary( L"Riched32.dll" );
	}

	CCompareFiles::~CCompareFiles()
	{
		ImageList_Destroy( _imgListToolbar );
		ImageList_Destroy( _imgListToolbarHot );
		ImageList_Destroy( _imgListToolbarDisabled );

		FreeLibrary( _hRichEditLib );
	}

	void CCompareFiles::createToolbar()
	{
		_hToolBar = CreateWindowEx( 0, TOOLBARCLASSNAME, NULL, TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | WS_CHILD, 0, 0, 0, 0, _hDlg, NULL, FCS::inst().getFcInstance(), NULL );

		TbUtils::setExtStyle( _hToolBar, TBSTYLE_EX_DOUBLEBUFFER );

		// initialize toolbar's image lists
		initImageLists();

		// load image list into toolbar
		SendMessage( _hToolBar, TB_SETIMAGELIST, 0, (LPARAM)_imgListToolbar );
		SendMessage( _hToolBar, TB_SETHOTIMAGELIST, 0, (LPARAM)_imgListToolbarHot );
		SendMessage( _hToolBar, TB_SETDISABLEDIMAGELIST, 0, (LPARAM)_imgListToolbarDisabled );

		// sending this ensures iString(s) of TBBUTTON structs become tooltips rather than button text
		SendMessage( _hToolBar, TB_SETMAXTEXTROWS, 0, 0 );

		// add buttons into toolbar
		addToolbarButtons();
	}

	//
	// Load image lists from resource
	//
	void CCompareFiles::initImageLists()
	{
		ULONG_PTR gdiplusToken;
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup( &gdiplusToken, &gdiplusStartupInput, NULL );

		// load .png image from resource
		HRSRC hResource = FindResource( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDB_COMMANDER_TOOLBAR ), L"PNG" );
		if( hResource )
		{
			DWORD imageSize = SizeofResource( FCS::inst().getFcInstance(), hResource );
			if( imageSize )
			{
				const void *pResourceData = LockResource( LoadResource( FCS::inst().getFcInstance(), hResource ) );
				if( pResourceData )
				{
					auto hBuffer = GlobalAlloc( GMEM_MOVEABLE, imageSize );
					if( hBuffer )
					{
						void *pBuffer = GlobalLock( hBuffer );
						if( pBuffer )
						{
							CopyMemory( pBuffer, pResourceData, imageSize );

							IStream *pStream = nullptr;
							if( CreateStreamOnHGlobal( hBuffer, FALSE, &pStream ) == S_OK )
							{
								auto pBitmap = Gdiplus::Bitmap::FromStream( pStream );
								if( pBitmap->GetLastStatus() == Gdiplus::Status::Ok )
								{
									// convert png into image-lists using appropriate matrices
									_imgListToolbar = IconUtils::createToolbarImageList( pBitmap, IconUtils::getColorMatrixGrayscale() );
									_imgListToolbarHot = IconUtils::createToolbarImageList( pBitmap );
									_imgListToolbarDisabled = IconUtils::createToolbarImageList( pBitmap, IconUtils::getColorMatrixMask() );

									delete pBitmap;
								}
								pStream->Release();
							}
							GlobalUnlock( hBuffer );
						}
						GlobalFree( hBuffer );
						hBuffer = nullptr;
					}
				}
			}
		}

		Gdiplus::GdiplusShutdown( gdiplusToken );
	}

	//
	// Add toolbar buttons
	//
	void CCompareFiles::addToolbarButtons()
	{
		// load the common controls images which seem to need an already loaded ImageList
		DWORD imgCount = static_cast<DWORD>( SendMessage( _hToolBar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL ) );

		// populate array of button describing structures
		std::vector<TBBUTTON> tbb;

		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, TBCMD_DIFFPREVIOUS, L"Previous Diff (F7)", 16 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, TBCMD_DIFFCURRENT, L"Current Diff (Alt+Enter)", 15 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, TBCMD_DIFFNEXT, L"Next Diff (F8)", 17 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, TBCMD_DIFFFIRST, L"First Diff (Alt+Home)", 7 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, TBCMD_DIFFLAST, L"Last Diff (Alt+End)", 8 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_CHECK, TBCMD_IGNORECASE, L"Ignore Case", 30 );
		INSERT_TOOLBAR_BUTTON( BTNS_CHECK, TBCMD_IGNOREWHITESPACE, L"Ignore Whitespaces", 73 );
		INSERT_TOOLBAR_BUTTON( BTNS_CHECK, TBCMD_IGNOREEOL, L"Ignore EOL", 27 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, TBCMD_REFRESH, L"Refresh (Ctrl+R)", 47 );

		// send the TB_BUTTONSTRUCTSIZE message, which is required for backwards compatibility
		SendMessage( _hToolBar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof( TBBUTTON ), 0 );

		// load the button structs into the toolbar to create the buttons
		SendMessage( _hToolBar, TB_ADDBUTTONS, (WPARAM)tbb.size(), (LPARAM)&tbb[0] );

		// resize the toolbar, and then show it
		SendMessage( _hToolBar, TB_AUTOSIZE, 0, 0 );
		ShowWindow( _hToolBar, SW_SHOWNORMAL );
	}

	void CCompareFiles::initFilePaths()
	{
		_path1 = FCS::inst().getApp().getCompareFile1();
		_path2 = FCS::inst().getApp().getCompareFile2();

		if( _path1.empty() || _path2.empty() )
		{
			auto dataMan = FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager();
			bool isFile = FCS::inst().getApp().getActivePanel().getActiveTab()->isFocusedEntryFile();
			bool isMarked = !dataMan->getMarkedEntries().empty();
			auto& entries = dataMan->getMarkedEntries();

			auto dataManOpp = FCS::inst().getApp().getOppositePanel().getActiveTab()->getDataManager();
			bool isFileOpp = FCS::inst().getApp().getOppositePanel().getActiveTab()->isFocusedEntryFile();
			bool isMarkedOpp = !dataManOpp->getMarkedEntries().empty();
			auto& entriesOpp = dataManOpp->getMarkedEntries();

			if( !isMarked && !isMarkedOpp && isFile && isFileOpp )
			{
				_path1 = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull()[0];
				_path2 = FCS::inst().getApp().getOppositePanel().getActiveTab()->getSelectedItemsPathFull()[0];
			}
			else if( entries.size() == 1 && entriesOpp.size() == 1 && dataMan->isEntryFile( entries[0] ) && dataManOpp->isEntryFile( entriesOpp[0] ) )
			{
				_path1 = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull()[0];
				_path2 = FCS::inst().getApp().getOppositePanel().getActiveTab()->getSelectedItemsPathFull()[0];
			}
			else if( entries.size() == 2 && dataMan->isEntryFile( entries[0] ) && dataMan->isEntryFile( entries[1] ) )
			{
				_path1 = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull()[0];
				_path2 = FCS::inst().getApp().getActivePanel().getActiveTab()->getSelectedItemsPathFull()[1];
			}
		}

		FCS::inst().getApp().getCompareFile1().clear();
		FCS::inst().getApp().getCompareFile2().clear();
	}

	int getLinesCount( HWND hRichedit )
	{
		RECT rct = { 0 };
		SendMessage( hRichedit, EM_GETRECT, 0, (LPARAM)&rct );

		LONG_PTR numerator = 0, denominator = 0;
		SendMessage( hRichedit, EM_GETZOOM, reinterpret_cast<WPARAM>( &numerator ), reinterpret_cast<LPARAM>( &denominator ) );

		// when zoom is not available (prior to Win10)
		if( numerator == 0 && denominator == 0 )
			numerator = denominator = 1;

		// get char format
		CHARFORMAT cf = { 0 }; cf.cbSize = sizeof( cf );
		SendMessage( hRichedit, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>( &cf ) );

		// convert twips to points: cf.yHeight / 20.0
		return (int)( (double)rct.bottom / ( ( (double)cf.yHeight / 20.0 ) * (double)numerator / (double)denominator ) );
	}

	void CCompareFiles::onInit()
	{
		initFilePaths();

		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getFromPathWithAttrib( PathUtils::stripPath( _path1 ).c_str(), FILE_ATTRIBUTE_NORMAL ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getFromPathWithAttrib( PathUtils::stripPath( _path1 ).c_str(), FILE_ATTRIBUTE_NORMAL, true ) ) );

		WCHAR title[MAX_WPATH];
		GetWindowText( _hDlg, title, _ARRAYSIZE( title ) );
		_dialogTitle = title;

		createToolbar();

		// make edit controls zoomable (only supported in Windows 10 1809 and later)
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_LEFT, EM_SETEXTENDEDSTYLE, ES_EX_ZOOMABLE, ES_EX_ZOOMABLE );
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_RIGHT, EM_SETEXTENDEDSTYLE, ES_EX_ZOOMABLE, ES_EX_ZOOMABLE );

		ShowWindow( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), SW_SHOWNORMAL );
		ShowWindow( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), SW_SHOWNORMAL );

		// set font for viewer
		SendDlgItemMessage( _hDlg, IDE_COMPAREPATH_LEFT, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getGuiFont() ), TRUE );
		SendDlgItemMessage( _hDlg, IDE_COMPAREPATH_RIGHT, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getGuiFont() ), TRUE );
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_LEFT, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_RIGHT, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getViewFont() ), TRUE );

		DragAcceptFiles( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), TRUE );
		DragAcceptFiles( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), TRUE );

		// subclass both rich-edit controls and store its default window procedure
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), wndProcRichEditSubclass, 0, reinterpret_cast<DWORD_PTR>( this ) );
		SetWindowSubclass( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), wndProcRichEditSubclass, 1, reinterpret_cast<DWORD_PTR>( this ) );

		// set rich-edit text upper limit
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_LEFT, EM_EXLIMITTEXT, 0, static_cast<LPARAM>( INT_MAX - 2 ) );
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_RIGHT, EM_EXLIMITTEXT, 0, static_cast<LPARAM>( INT_MAX - 2 ) );

		// hide vertical scrollbar on left rich-edit
		SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_LEFT, EM_SHOWSCROLLBAR, SB_VERT, static_cast<LPARAM>( FALSE ) );

		// create find dialog instance
		_pFindDlg = CBaseDialog::createModeless<CFindText>( _hDlg );

		// count how many rows can be displayed
		_hActiveEdit = GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT );
	//	_linesCountPerPage = getLinesCount( _hActiveEdit );
		_linesCountPerPage = MiscUtils::countVisibleLines( _hActiveEdit );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		_worker.init( [this] { return _workerProc(); }, _hDlg, UM_WORKERNOTIFY );

		refresh();

		show();  // show dialog
	}

	bool CCompareFiles::onOk()
	{
		refresh();

		return false; // do not close dialog
	}

	bool CCompareFiles::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		return true;
	}

	void CCompareFiles::onDestroy()
	{
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );

		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), wndProcRichEditSubclass, 0 );
		RemoveWindowSubclass( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), wndProcRichEditSubclass, 1 );
	}

	void CCompareFiles::updateWindowTitle()
	{
		std::wostringstream sstrTitle;
		sstrTitle << L"[" << PathUtils::stripPath( _path1 ) << L" - " << PathUtils::stripPath( _path2 ) << L"] - " << _dialogTitle;
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );
	}

	void CCompareFiles::updateGuiStatus()
	{
		bool enablePrev = _diffs.GetSize() > 0 && _curDiff > 0;
		bool enableNext = _diffs.GetSize() > 0 && _curDiff + 1 < _diffs.GetSize();

		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_DIFFPREVIOUS, MAKELPARAM( enablePrev, 0 ) );
		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_DIFFNEXT, MAKELPARAM( enableNext, 0 ) );
		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_DIFFCURRENT, MAKELPARAM( _curDiff != -1, 0 ) );
		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_DIFFFIRST, MAKELPARAM( enablePrev, 0 ) );
		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_DIFFLAST, MAKELPARAM( enableNext, 0 ) );

		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_IGNORECASE, MAKELPARAM( !_binaryMode, 0 ) );
		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_IGNOREWHITESPACE, MAKELPARAM( !_binaryMode, 0 ) );
		SendMessage( _hToolBar, TB_ENABLEBUTTON, TBCMD_IGNOREEOL, MAKELPARAM( !_binaryMode, 0 ) );
	}

	void CCompareFiles::updateOptions()
	{
		_options.bIgnoreCase = !!SendMessage( _hToolBar, TB_ISBUTTONCHECKED, TBCMD_IGNORECASE, 0 );
		_options.nIgnoreWhitespace = SendMessage( _hToolBar, TB_ISBUTTONCHECKED, TBCMD_IGNOREWHITESPACE, 0 ) ? WHITESPACE_IGNORE_ALL : WHITESPACE_COMPARE_ALL;
		_options.bIgnoreEol = !!SendMessage( _hToolBar, TB_ISBUTTONCHECKED, TBCMD_IGNOREEOL, 0 );
	}

	void CCompareFiles::refresh()
	{
		_worker.stop();

		SetDlgItemText( _hDlg, IDE_COMPAREPATH_LEFT, MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDE_COMPAREPATH_LEFT ), _path1 ).c_str() );
		SetDlgItemText( _hDlg, IDE_COMPAREPATH_RIGHT, MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDE_COMPAREPATH_RIGHT ), _path2 ).c_str() );

		SetDlgItemText( _hDlg, IDC_COMPARERICHEDIT_LEFT, L"" );
		SetDlgItemText( _hDlg, IDC_COMPARERICHEDIT_RIGHT, L"" );

		updateWindowTitle();

		_diffs.Clear();
		_curDiff = -1;

		_buff[0].clear();
		_buff[1].clear();

		// refresh
		_worker.start();
	}

	void CCompareFiles::setZoom( int value )
	{
		int zoom = _curZoom;

		if( ( value == 0 ) || ( value > 0 && zoom < 500 ) || ( value < 0 && zoom > 10 ) )
		{
			zoom += value > 0 ? 10 : value < 0 ? -10 : -zoom + 100;

			LRESULT res = 0;

			res += SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_LEFT, EM_SETZOOM, (WPARAM)( ( (double)zoom / 100. ) * 985. ), (LPARAM)1000 );
			res += SendDlgItemMessage( _hDlg, IDC_COMPARERICHEDIT_RIGHT, EM_SETZOOM, (WPARAM)( ( (double)zoom / 100. ) * 985. ), (LPARAM)1000 );

			if( res > 0 )
			{
				// set status-bar current zoom as text
			//	std::wstring zoomStr = std::to_wstring( zoom ) + L"%";
			//	SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 2, SBT_NOBORDERS ), (LPARAM)zoomStr.c_str() );

				_curZoom = zoom;
			}
			else
				_curZoom = 100;
		}
	}

	void CCompareFiles::findTextDialog()
	{
		std::wstring text = _textToFind;

		if( !_findParams.hexMode && !_findParams.regexMode )
			text = MiscUtils::getEditSelectionSingle( _hActiveEdit );

		_pFindDlg->updateText( text.empty() ? _textToFind : text );
		_pFindDlg->updateParams( _findParams );
		_pFindDlg->show();
	}

	void CCompareFiles::searchText( bool reverse )
	{
		if( !_textToFind.empty() )
		{
			SetCursor( LoadCursor( NULL, IDC_WAIT ) );

			std::wostringstream sstr;
			sstr << L"Searching" << ( _findParams.hexMode ? L" hex: " : L": " ) << _textToFind;
			//SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)sstr.str().c_str() );

			// this is needed to prevent when user presses right arrow key right after search,
			// the caret stays at the same position and the _findFromIdxBeg won't get updated
			SendMessage( _hActiveEdit, EM_GETSEL, (WPARAM)&_findFromIdxBeg, (LPARAM)&_findFromIdxEnd );

			//int idxBeg = -1, idxEnd = -1;
			std::wstring textToFind = _findParams.hexMode ? _textHexToFind : _textToFind;

			// intialize FINDTEXTEX search parameters
			FINDTEXTEX ft = { 0 };

			ft.chrg.cpMin = reverse ? _findFromIdxBeg : _findFromIdxEnd;
			ft.chrg.cpMax = -1;
			ft.lpstrText = textToFind.c_str();

			DWORD flags = FR_FINDNEXT;
			flags |= !reverse ? FR_DOWN : 0;
			flags |= _findParams.matchCase ? FR_MATCHCASE : 0;
			flags |= _findParams.wholeWords ? FR_WHOLEWORD : 0;

			if( SendMessage( _hActiveEdit, EM_FINDTEXTEXW, flags, (LPARAM)&ft ) != -1 )
			{
				SendMessage( _hActiveEdit, EM_SETSEL, (WPARAM)ft.chrgText.cpMin, (LPARAM)ft.chrgText.cpMax );
				SendMessage( _hActiveEdit, EM_SCROLLCARET, 0, 0 );

				SetCursor( NULL );
				//SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"" );

				//updateStatusBarCaretPos( true );
			}
			else
			{
				SetCursor( NULL );
				//SendMessage( _hStatusBar, SB_SETTEXT, MAKEWORD( 0, SBT_POPOUT ), (LPARAM)L"Not found" );

				std::wstringstream sstr;
				sstr << L"Cannot find " << ( _findParams.hexMode ? L"hex \"" : L"\"" ) << _textToFind << L"\".";
				_pFindDlg->showMessageBox( sstr.str() );
			}
		}
		else
			findTextDialog();
	}

	void CCompareFiles::moveLine( std::vector<std::pair<DWORD, std::string>>& buff, int line1, int line2, int newline1 )
	{
		int ldiff = newline1 - line1;
		if( ldiff > 0 )
		{
			for( int l = line2; l >= line1; l-- )
				buff[l + ldiff] = buff[l];
		}
		else if( ldiff < 0 )
		{
			for( int l = line1; l <= line2; l++ )
				buff[l + ldiff] = buff[l];
		}
	}

	void CCompareFiles::setLineFlag( std::vector<std::pair<DWORD, std::string>>& buff, int line, DWORD flag, bool set )
	{
		if( line < 0 || line >= static_cast<int>( buff.size() ) )
		{
			_ASSERTE( "line is out of range" && 1 != 1 );
			return;
		}

		DWORD dwNewFlags = buff[line].first;
		if( set )
		{
			if( flag == 0 )
				dwNewFlags = 0;
			else
				dwNewFlags |= flag;
		}
		else
			dwNewFlags &= ~flag;

		if( buff[line].first != dwNewFlags )
			buff[line].first = dwNewFlags;
	}

	void CCompareFiles::primeTextBuffers()
	{
	//	SetCurrentDiff(-1);
		_trivialDiffs = 0;

		const int cnt = 2;
		int nDiff;
		int nDiffCount = _diffs.GetSize();
		int file;

		// walk the diff list and calculate numbers of extra lines to add
		int extras[3] = { 0, 0, 0 };   // extra lines added to each view
		_diffs.GetExtraLinesCounts(cnt, extras);

		// resize m_aLines once for each view
		UINT lcount[3] = { 0, 0, 0 };
		UINT lcountnew[3] = { 0, 0, 0 };
		UINT lcountmax = 0;

		for (file = 0; file < cnt; file++)
		{
			lcount[file] = (UINT)_buff[file].size();//->GetLineCount();
			lcountnew[file] = lcount[file] + extras[file];
			lcountmax = max(lcountmax, lcountnew[file]);
		}
		for (file = 0; file < cnt; file++)
		{
		//	_buff[file]->m_aLines.resize(lcountmax);
			_buff[file].resize(lcountmax);
		}

		// walk the diff list backward, move existing lines to proper place,
		// add ghost lines, and set flags
		for (nDiff = nDiffCount - 1; nDiff >= 0; nDiff--)
		{
			DIFFRANGE curDiff;
			if(!_diffs.GetDiff(nDiff, curDiff))
			{
				_ASSERTE("_diffs.GetDiff()" && 1 != 1);
				continue;
			}
			// move matched lines after curDiff
			int nline[3] = { 0, 0, 0 };
			for (file = 0; file < cnt; file++)
				nline[file] = lcount[file] - curDiff.end[file] - 1; // #lines on left/middle/right after current diff
			// Matched lines should really match...
			// But matched lines after last diff may differ because of empty last line (see function's note)
			if (nDiff < nDiffCount - 1)
			{
				_ASSERTE(nline[0] == nline[1]);
			}

			int nmaxline = 0;
			for (file = 0; file < cnt; file++)
			{
				// Move all lines after current diff down as far as needed
				// for any ghost lines we're about to insert
			//	_buff[file]->MoveLine(curDiff.end[file] + 1, lcount[file] - 1, lcountnew[file] - nline[file]);
				moveLine(_buff[file], curDiff.end[file] + 1, lcount[file] - 1, lcountnew[file] - nline[file]);
				lcountnew[file] -= nline[file];
				lcount[file] -= nline[file];
				// move unmatched lines and add ghost lines
				nline[file] = curDiff.end[file] - curDiff.begin[file] + 1; // #lines in diff on left/middle/right
				nmaxline = max(nmaxline, nline[file]);
			}

			for (file = 0; file < cnt; file++)
			{
				DWORD dflag = LF_GHOST;
				if ((file == 0 && curDiff.op == OP_3RDONLY) || (file == 2 && curDiff.op == OP_1STONLY))
					dflag |= LF_SNP;
			//	_buff[file]->MoveLine(curDiff.begin[file], curDiff.end[file], lcountnew[file] - nmaxline);
				moveLine(_buff[file], curDiff.begin[file], curDiff.end[file], lcountnew[file] - nmaxline);
				int nextra = nmaxline - nline[file];
				if (nextra > 0)
				{
				//	_buff[file]->SetEmptyLine(lcountnew[file] - nextra, nextra);
					for (int i = 0; i < nextra; i++)
					{
						_buff[file][lcountnew[file] - nextra + i] = std::make_pair( 0, "" );
					}

					for (int i = 1; i <= nextra; i++)
					//	_buff[file]->SetLineFlag(lcountnew[file] - i, dflag, true, false, false);
						setLineFlag(_buff[file], lcountnew[file] - i, dflag, true);
				}
				lcountnew[file] -= nmaxline;

				lcount[file] -= nline[file];

			}
			// set dbegin, dend, blank, and line flags
			curDiff.dbegin = lcountnew[0];

			switch (curDiff.op)
			{
			case OP_TRIVIAL:
				++_trivialDiffs;
			//	[[fallthrough]];
			case OP_DIFF:
			case OP_1STONLY:
			case OP_2NDONLY:
			case OP_3RDONLY:
				// set curdiff
			{
				curDiff.dend = lcountnew[0] + nmaxline - 1;
				for (file = 0; file < cnt; file++)
				{
					curDiff.blank[file] = -1;
					int nextra = nmaxline - nline[file];
					if (nmaxline > nline[file])
					{
						// more lines on left, ghost lines on right side
						curDiff.blank[file] = curDiff.dend + 1 - nextra;
					}
				}
			}
			// flag lines
			{
				for (file = 0; file < cnt; file++)
				{
					// left side
					int i;
					for (i = curDiff.dbegin; i <= curDiff.dend; i++)
					{
						if (curDiff.blank[file] == -1 || (int)i < curDiff.blank[file])
						{
							// set diff or trivial flag
							DWORD dflag = (curDiff.op == OP_TRIVIAL) ? LF_TRIVIAL : LF_DIFF;
							if ((file == 0 && curDiff.op == OP_3RDONLY) || (file == 2 && curDiff.op == OP_1STONLY))
								dflag |= LF_SNP;
						//	_buff[file]->SetLineFlag(i, dflag, true, false, false);
							setLineFlag(_buff[file], i, dflag, true);
						//	_buff[file]->SetLineFlag(i, LF_INVISIBLE, false, false, false);
							setLineFlag(_buff[file], i, LF_INVISIBLE, false);
						}
						else
						{
							// ghost lines are already inserted (and flagged)
							// ghost lines opposite to trivial lines are ghost and trivial
							if (curDiff.op == OP_TRIVIAL)
							//	_buff[file]->SetLineFlag(i, LF_TRIVIAL, true, false, false);
								setLineFlag(_buff[file], i, LF_TRIVIAL, true);
						}
					}
				}
			}
			break;
			} // switch (curDiff.op)
			if(!_diffs.SetDiff(nDiff, curDiff))
			{
				_ASSERTE("_diffs.SetDiff()" && 1 != 1);
			}
		} // for (nDiff = nDiffCount; nDiff-- > 0; )

		_diffs.ConstructSignificantChain();

#ifdef _DEBUG
		// Note: By this point all `_buff[]` buffers must have the same  
		//		number of line entries; eventual buffer processing typically
		//		uses the line count from `_buff[0]` for all buffer processing.

		for (file = 0; file < cnt; file++)
		{
			_ASSERTE(_buff[0].size() == _buff[file].size());
		}
#endif
		// TODO: do we need reality blocks??
	//	for (file = 0; file < cnt; file++)
	//		_buff[file]->FinishLoading();
	}

	std::string escapeRtfControlChars( const std::string& str )
	{
		std::string strOut;
		strOut.reserve( str.length() );

		for( auto it = str.begin(); it != str.end(); ++it  )
		{
			if( *it == '\\' || *it == '{' || *it == '}' )
				strOut += '\\';

			strOut += *it;
		}

		return strOut;
	}

	std::string putAnsiCharsHex( const std::string& buf1, const std::string& buf2, size_t idx, size_t bytesPerLine )
	{
		std::string strOut;

		for( size_t i = 0; i < bytesPerLine; ++i )
		{
			if( buf1.size() <= idx + i ) // ghost char
				strOut += "\\cf4\\highlight3 ";
			else if( buf2.size() > idx + i && buf1[idx+i] != buf2[idx+i] )
				strOut += "\\cf4\\highlight2 ";

			char ch = ( buf1.size() > idx + i ? CFileViewer::getPrintableChar( buf1[idx+i] ) : ' ' );

			// escapeRtfControlChars
			if( ch == '\\' || ch == '{' || ch == '}' )
				strOut += '\\';

			strOut += ch;

			if( buf1.size() <= idx + i || ( buf2.size() > idx + i && buf1[idx+i] != buf2[idx+i] ) )
				strOut += "\\cf0\\highlight0 ";
		}

		return strOut;
	}

	void CCompareFiles::writeRtfDocumentHex( const std::string& buf1, const std::string& buf2, std::streamsize offset, std::string& outBuff )
	{
		// intialize RTF fonts and color table
		outBuff = "{\\rtf1\\ansi\\ansicpg1250\\deff0\\nouicompat\\deflang1029{\\fonttbl{\\f0\\fnil\\fcharset0 Consolas;}}";
		outBuff += "{\\colortbl ;\\red255\\green0\\blue0;\\red0\\green77\\blue187;\\red128\\green128\\blue128;\\red0\\green255\\blue0;}";

		LARGE_INTEGER fileSize; fileSize.QuadPart = offset;

		for( size_t idx = 0; ( idx < buf1.size() || idx < buf2.size() ) && _worker.isRunning(); idx += BYTES_ROW )
		{
			size_t bytesPerLine = min( max( buf1.size(), buf2.size() ) - idx, BYTES_ROW );

			outBuff += "\\fs21\\pard ";

			std::ostringstream sstr;
			sstr << std::hex << std::setfill( '0' ) << std::uppercase;

			// write out the offset number
			sstr << FsUtils::getOffsetNumber( fileSize, offset + idx );

			// write out extra space between every 4 bytes and ascii data at the end
			for( std::streamsize i = 1; i < BYTES_ROW + 1; ++i )
			{
				// write out one BYTE
				if( i < bytesPerLine + 1 )
				{
					if( buf1.size() <= idx + i - 1 )
					{
						// ghost byte
						sstr << "\\cf4\\highlight3  \r \\cf0\\highlight0  ";
					}
					else
					{
						if( buf2.size() > idx + i - 1 && buf1[idx+i-1] != buf2[idx+i-1] )
							sstr << "\\cf4\\highlight2 " << std::setw( 2 ) << ( buf1[idx+i-1] & 0xFF ) << "\\cf0\\highlight0  ";
						else
							sstr << std::setw( 2 ) << ( buf1[idx+i-1] & 0xFF ) << " ";
					}
				}
				else
					sstr << "   ";

				// spacing between bytes
				if( ( i % 4 ) == 0 )
					sstr << " ";
			}

			outBuff += sstr.str();

			// write out the ANSI representation of the data
			outBuff += putAnsiCharsHex( buf1, buf2, idx, bytesPerLine );
			outBuff += "\\par ";
		}

		outBuff += "}";
	}

	void CCompareFiles::writeRtfDocument( const std::vector<std::pair<DWORD, std::string>>& buff1, const std::vector<std::pair<DWORD, std::string>>& buff2, std::string& outBuff )
	{
		// intialize RTF fonts and color table
		outBuff = "{\\rtf1\\ansi\\ansicpg1250\\deff0\\nouicompat\\deflang1029{\\fonttbl{\\f0\\fnil\\fcharset0 Consolas;}}";
		outBuff += "{\\colortbl ;\\red255\\green0\\blue0;\\red0\\green77\\blue187;\\red128\\green128\\blue128;\\red0\\green255\\blue0;}";

		for( auto it = buff1.begin(); it != buff1.end(); ++it )
		{
			outBuff += "\\pard ";

			auto tmpLine = escapeRtfControlChars( it->second );

			if( it->first & LF_DIFF )
			{
				if( it->first & LF_MOVED )
					PrintDebug("TODO: moved");
				if( it->first & LF_SNP )
					PrintDebug("TODO: snp");
				if( it->first & LF_INVISIBLE )
					PrintDebug("TODO: invisible");

				outBuff += "\\cf4\\highlight2 " + tmpLine + " \\cf0\\highlight0";
			}
			else if( it->first & LF_GHOST )
			{
				outBuff += "\\cf4\\highlight3 ";
				outBuff.append( buff2[it - buff1.begin()].second.length(), ' ' );
				outBuff += "\r \\cf0\\highlight0";
			}
			else if( it->first & LF_TRIVIAL )
				outBuff += tmpLine;
			else
				outBuff += tmpLine;

			outBuff += "\\par ";
		}

		outBuff += "}";
	}

	bool CCompareFiles::compareFilesText()
	{
		// set up diff wrapper
		_diffWrapper.SetOptions( &_options );
		_diffWrapper.SetPaths( { _path1, _path2 }, false );
		_diffWrapper.SetDetectMovedBlocks( true );
		_diffWrapper.SetCreateDiffList( &_diffs );

		if( _diffWrapper.RunFileDiff() )
		{
			_diffWrapper.GetDiffStatus( &_status );

			primeTextBuffers();

			writeRtfDocument( _buff[0], _buff[1], _diffLeft );
			writeRtfDocument( _buff[1], _buff[0], _diffRight );

		//	_diffLeft = "{\\rtf1\\ansi\\ansicpg1250\\deff0\\nouicompat\\deflang1029{\\fonttbl{\\f0\\fnil\\fcharset1 Segoe UI Symbol;}{\\f1\\fnil\\fcharset0 Calibri;}}"
		//				"{\\*\\generator Riched20 10.0.19041}\\viewkind4\\uc1 "
		//				"\\pard\\sa200\\sl276\\slmult1\\f0\\fs22\\lang9\\u-10179?\\u-8703?\\u-10179?\\u-8702?\\u-10179?\\u-8701?\\f1\\par}";

		//	std::ofstream fs1( "d:\\temp\\out1.rtf", std::ios::binary );
		//	fs1 << _diffLeft;
		//
		//	std::ofstream fs2( "d:\\temp\\out2.rtf", std::ios::binary );
		//	fs2 << _diffRight;

			return true;
		}

		return false;
	}

	bool CCompareFiles::loadFilesAsText()
	{
		std::ifstream fs1( PathUtils::getExtendedPath( _path1 ), std::ios::binary );
		std::ifstream fs2( PathUtils::getExtendedPath( _path2 ), std::ios::binary );

		if( fs1.is_open() && fs2.is_open() )
		{
			{
				CTextFileReader reader( fs1, &_worker );

				if( reader.isText() )
				{
					while( _worker.isRunning() && reader.readLine() == ERROR_SUCCESS )
						_buff[0].push_back( std::make_pair( 0, StringUtils::convert2A( reader.getLineRef() ) ) );
				}
				else
					return false;
			}

			{
				CTextFileReader reader( fs2, &_worker );

				if( reader.isText() )
				{
					while( _worker.isRunning() && reader.readLine() == ERROR_SUCCESS )
						_buff[1].push_back( std::make_pair( 0, StringUtils::convert2A( reader.getLineRef() ) ) );
				}
				else
					return false;
			}

			return true;
		}

		return false;
	}

	std::streamoff findOffsetBinary( const std::string& buf1, const std::string& buf2, std::streamsize len )
	{
		_ASSERTE( buf1.size() >= len );
		_ASSERTE( buf2.size() >= len );

		std::streamoff off = 0;

		while( off < len )
		{
			if( buf1[off] != buf2[off] )
				break;
			else
				off++;
		}

		return off;
	}

	bool CCompareFiles::findDiffBinary( std::ifstream& fs1, std::ifstream& fs2, std::streamoff& offset )
	{
		if( fs1.is_open() && fs2.is_open() )
		{
			std::string buf1( 0x10000, 0 );
			std::string buf2( 0x10000, 0 );

			std::streamoff offAbs = 0;
			std::streamoff size1, size2;

			while( _worker.isRunning() )
			{
				// read chunks
				size1 = fs1.read( reinterpret_cast<char*>( &buf1[0] ), buf1.size() ).gcount();
				size2 = fs2.read( reinterpret_cast<char*>( &buf2[0] ), buf2.size() ).gcount();

				// compare chunks
				if( size1 == 0 || size1 != size2 || !!memcmp( &buf1[0], &buf2[0], size1 ) )
				{
					// find the offset of the first difference in both chunks
					offset = offAbs + findOffsetBinary( buf1, buf2, min( size1, size2 ) );
					return true;
				}

				offAbs += size1;
			}
		}

		return false;
	}

	bool CCompareFiles::compareFilesBinary()
	{
		std::ifstream fs1( PathUtils::getExtendedPath( _path1 ), std::ios::binary );
		std::ifstream fs2( PathUtils::getExtendedPath( _path2 ), std::ios::binary );

		std::streamoff offset = 0ll;

		// a difference has been found at offset
		if( findDiffBinary( fs1, fs2, offset ) )
		{
			offset = offset / BYTES_ROW * BYTES_ROW;
			offset = max( 0ll, offset - BYTES_ROW ); // one row before the difference

			fs1.clear(); fs1.seekg( offset, std::ios::beg );
			fs2.clear(); fs2.seekg( offset, std::ios::beg );

			std::string buf1; buf1.resize( BYTES_ROW * 256 );
			std::string buf2; buf2.resize( BYTES_ROW * 256 );

			// read chunks
			auto size1 = fs1.read( reinterpret_cast<char*>( &buf1[0] ), buf1.size() ).gcount();
			auto size2 = fs2.read( reinterpret_cast<char*>( &buf2[0] ), buf2.size() ).gcount();

			buf1.resize( size1 );
			buf2.resize( size2 );

			// write out rtf documents
			writeRtfDocumentHex( buf1, buf2, offset, _diffLeft );
			writeRtfDocumentHex( buf2, buf1, offset, _diffRight );

			//	std::ofstream fs1( "C:\\Users\\bax\\Documents\\out1.rtf", std::ios::binary );
			//	fs1 << _diffLeft;
				
			//	std::ofstream fs2( "C:\\Users\\bax\\Documents\\out2.rtf", std::ios::binary );
			//	fs2 << _diffRight;

			return true;
		}

		return false;
	}

	// TODO: move this function somewhere else
	bool replaceByte( const std::wstring& path, ULONGLONG offset, BYTE byte )
	{
		HANDLE hFile = CreateFile( PathUtils::getExtendedPath( path ).c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
		bool ret = false;

		if( hFile != INVALID_HANDLE_VALUE )
		{
			LARGE_INTEGER distanceToMove, newFilePointer;
			distanceToMove.QuadPart = offset;//0xAFDFBA00ll;

			if( SetFilePointerEx( hFile, distanceToMove, &newFilePointer, FILE_BEGIN ) != 0 )
			{
				DWORD written = 0;
				if( WriteFile( hFile, &byte, 1, &written, NULL ) )
				{
					ret = !!written;
				}
			}
			else
				PrintDebug("%ls", SysUtils::getErrorMessage( GetLastError() ).c_str());

			CloseHandle(hFile);
		}

		return ret;
	}

	//
	// Look for duplicates in two lists
	//
	bool CCompareFiles::_workerProc()
	{
		if( !_binaryMode && loadFilesAsText() )
		{
			return compareFilesText();
		}
		else
		{
			// TODO: binary compare
			_binaryMode = true;
			return compareFilesBinary();
		}

		return false;
	}

	void CCompareFiles::focusLine( int lineIdx )
	{
		auto idx1 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETFIRSTVISIBLELINE, 0, 0 );
		auto idx2 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETFIRSTVISIBLELINE, 0, 0 );

		LPARAM off1 = max( idx1, lineIdx ) - min( idx1, lineIdx );
		LPARAM off2 = max( idx2, lineIdx ) - min( idx2, lineIdx );

		if( idx1 > lineIdx )
			off1 *= -1;

		if( idx2 > lineIdx )
			off2 *= -1;

		SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_LINESCROLL, 0, off1 );
		SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_LINESCROLL, 0, off2 );

		updateGuiStatus();
	}

	void CCompareFiles::focusDiffPrev()
	{
		if( _curDiff > 0 )
		{
			DIFFRANGE dr;
			if( _diffs.GetDiff( --_curDiff, dr ) )
			{
				focusLine( dr.dbegin );
			}
		}
	}

	void CCompareFiles::focusDiffNext()
	{
		if( _curDiff + 1 < _diffs.GetSize() )
		{
			DIFFRANGE dr;
			if( _diffs.GetDiff( ++_curDiff, dr ) )
			{
				focusLine( dr.dbegin );
			}
		}
	}

	void CCompareFiles::focusDiffCurrent()
	{
		if( _curDiff != -1 )
		{
			DIFFRANGE dr;
			if( _diffs.GetDiff( _curDiff, dr ) )
			{
				focusLine( dr.dbegin );
			}
		}
	}

	void CCompareFiles::focusDiffFirst()
	{
		_curDiff = 0;

		DIFFRANGE dr;
		if( _diffs.GetDiff( _curDiff, dr ) )
		{
			focusLine( dr.dbegin );
		}
	}

	void CCompareFiles::focusDiffLast()
	{
		_curDiff = _diffs.GetSize() - 1;

		DIFFRANGE dr;
		if( _diffs.GetDiff( _curDiff, dr ) )
		{
			focusLine( dr.dbegin );
		}
	}

	void CCompareFiles::handleDropFiles( HDROP hDrop, bool leftRichEdit )
	{
		auto items = ShellUtils::getDroppedItems( hDrop );
	
		DragFinish( hDrop );

		if( !items.empty() )
		{
			if( items.size() == 1 )
			{
				if( leftRichEdit )
					_path1 = items[0];
				else
					_path2 = items[0];
			}
			else
			{
				_path1 = items[0];
				_path2 = items[1];
			}

			refresh();
		}
	}

	void CCompareFiles::onFindDialogNotify( int cmd )
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
			//_textToReplace = _pFindDlg->getReplaceText();
			break; // TODO: replace??
		case 1: // perform search text
			_textToFind = _pFindDlg->result();
			_findParams = _pFindDlg->getParams();

			if( cmd == 1 )
				searchText( _findParams.reverse );
			//else
			//	replaceText( cmd == 3 );
			break;
		default:
			PrintDebug("unknown command: %d", cmd);
			break;
		}
	}

	INT_PTR CALLBACK CCompareFiles::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
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

		case UM_WORKERNOTIFY:
			if( wParam == 1 )
			{
				SETTEXTEX stx;
				stx.codepage = _binaryMode ? CP_ACP : CP_UTF8;
				stx.flags = ST_DEFAULT;

				//SendMessage( hRich1, EM_SETTEXTMODE, (WPARAM)TM_RICHTEXT, 0 );
				//SendMessage( hRich2, EM_SETTEXTMODE, (WPARAM)TM_RICHTEXT, 0 );
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_SETTEXTEX, (WPARAM)&stx, (LPARAM)_diffLeft.c_str() );
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_SETTEXTEX, (WPARAM)&stx, (LPARAM)_diffRight.c_str() );

				updateGuiStatus();
			}
			else if( wParam == 0 )
			{
				MessageBox( _hDlg, L"Cannot read source files.", L"Compare Files", MB_ICONEXCLAMATION | MB_OK );
				close();
			}
			break;

		case WM_COMMAND:
			if( HIWORD( wParam ) == BN_CLICKED )
			{
				switch( LOWORD( wParam ) )
				{
				case TBCMD_DIFFPREVIOUS:
					focusDiffPrev();
					break;
				case TBCMD_DIFFNEXT:
					focusDiffNext();
					break;
				case TBCMD_DIFFCURRENT:
					focusDiffCurrent();
					break;
				case TBCMD_DIFFFIRST:
					focusDiffFirst();
					break;
				case TBCMD_DIFFLAST:
					focusDiffLast();
					break;
				case TBCMD_IGNORECASE:
				case TBCMD_IGNOREWHITESPACE:
				case TBCMD_IGNOREEOL:
					updateOptions();
					// fall-through and refresh
				case TBCMD_REFRESH:
					refresh();
					break;
				}
			}
			break;

		case WM_NOTIFY:
			break;

		case UM_REPORTMSGBOX:
			_pFindDlg->messageBoxClosed();
			break;

		case UM_FINDLGNOTIFY:
			if( wParam == IDE_FINDTEXT_TEXTFIND ) // from CFindText dialog
				onFindDialogNotify( static_cast<int>( lParam ) );
			break;

		case UM_CARETPOSCHNG:
			{
				int idx1 = (int)SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETFIRSTVISIBLELINE, 0, 0 );
				int idx2 = (int)SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETFIRSTVISIBLELINE, 0, 0 );

				int idxBeg1, idxEnd1, idxBeg2, idxEnd2;
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETSEL, (WPARAM)&idxBeg1, (LPARAM)&idxEnd1 );
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETSEL, (WPARAM)&idxBeg2, (LPARAM)&idxEnd2 );

				bool left  = ( GetFocus() == GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ) );
				bool right = ( GetFocus() == GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ) );

				_findFromIdxBeg = left ? idxBeg1 : idxBeg2;
				_findFromIdxEnd = left ? idxEnd1 : idxEnd2;

				if( idx1 != idx2 || idxBeg1 != idxBeg2 || idxEnd1 != idxEnd2 )
				{
					if( left || right )
					{
						PrintDebug("left: %d, right: %d", left, right);

						focusLine( left ? idx1 : idx2 );

						if( left )
							SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_SETSEL, (WPARAM)idxBeg1, (LPARAM)idxEnd1 );
						else
							SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_SETSEL, (WPARAM)idxBeg2, (LPARAM)idxEnd2 );
					}
				}
			}
			break;

		case WM_CTLCOLORSTATIC:
			SetTextColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_TEXT );
			SetBkColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_DIRBOXBKGND );
			return reinterpret_cast<LRESULT>( FCS::inst().getApp().getBkgndBrush() );

		case WM_SETCURSOR:
			if( _worker.isRunning() )
			{
				SetCursor( LoadCursor( NULL, IDC_WAIT ) );
				return 0;
			}
			break;

		case WM_SIZE:
		{
			int scrollHeight = GetSystemMetrics( SM_CYHSCROLL ) + GetSystemMetrics( SM_CYSIZEFRAME ) + 28;
			MoveWindow( GetDlgItem( _hDlg, IDE_COMPAREPATH_LEFT ), 0, 28, LOWORD( lParam ) / 2, 20, TRUE );
			MoveWindow( GetDlgItem( _hDlg, IDE_COMPAREPATH_RIGHT ), LOWORD( lParam ) / 2, 28, LOWORD( lParam ) / 2, 20, TRUE );
			MoveWindow( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), 0, 49, LOWORD( lParam ) / 2 - 1, HIWORD( lParam ) - scrollHeight, TRUE );
			MoveWindow( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), LOWORD( lParam ) / 2 + 1, 49, LOWORD( lParam ) / 2, HIWORD( lParam ) - scrollHeight, TRUE );

			SetDlgItemText( _hDlg, IDE_COMPAREPATH_LEFT, MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDE_COMPAREPATH_LEFT ), _path1 ).c_str() );
			SetDlgItemText( _hDlg, IDE_COMPAREPATH_RIGHT, MiscUtils::makeCompactPath( GetDlgItem( _hDlg, IDE_COMPAREPATH_RIGHT ), _path2 ).c_str() );

			SendMessage( _hToolBar, TB_AUTOSIZE, 0, 0 );
			break;
		}
		}

		return (INT_PTR)false;
	}

	LRESULT CALLBACK CCompareFiles::wndProcRichEdit( HWND hWnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR subclassId )
	{
		switch( message )
		{
		case WM_SETFOCUS:
			_hActiveEdit = hWnd;
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 )
			{
				switch( wp )
				{
				case VK_UP:
					focusDiffPrev();
					return 0;
				case VK_DOWN:
					focusDiffNext();
					return 0;
				case VK_RETURN:
					focusDiffCurrent();
					return 0;
				case VK_HOME:
					focusDiffFirst();
					return 0;
				case VK_END:
					focusDiffLast();
					return 0;
				}
			}
			else if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				switch( wp )
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
				case 0x46: // "F - find text"
					_hActiveEdit = hWnd;
					findTextDialog();
					return 0;
				case 0x52: // "R - refresh"
					refresh();
					return 0;
				}
			}
			else switch( wp )
			{
			case VK_F3:
			{
				// invert direction when holding shift key
				bool shift = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
				searchText( ( _findParams.reverse && !shift ) || ( !_findParams.reverse && shift ) );
				return 0;
			}
			case VK_F5:
				refresh();
				return 0;
			case VK_F7:
				focusDiffPrev();
				return 0;
			case VK_F8:
				focusDiffNext();
				return 0;
			}
			break;

		case WM_DROPFILES:
			handleDropFiles( reinterpret_cast<HDROP>( wp ), subclassId == 0 );
			break;

		case WM_MBUTTONDOWN: // disable middle-button scrolling
			return 0;

		case WM_KEYUP: // TODO FIXME: quick hack to scroll both edits
		case WM_LBUTTONUP:
		case WM_VSCROLL:
			/*{
				SCROLLINFO si = { 0 };
				si.cbSize = sizeof( si );
				si.fMask = SIF_PAGE | SIF_TRACKPOS | SIF_RANGE;
				//auto pos = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETTHUMB, 0, 0 );
				//PrintDebug("pos: %d", pos);
				if( GetScrollInfo( hWnd, SB_VERT, &si ) )
				{
					auto idx1 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETFIRSTVISIBLELINE, 0, 0 );
					auto idx2 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETFIRSTVISIBLELINE, 0, 0 );

					int cnt = (int)SendMessage( hWnd, EM_GETLINECOUNT, 0, 0 );
					int idx = ( cnt / si.nMax ) * si.nTrackPos;

					SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_LINESCROLL, 0, (LPARAM)abs( idx - idx1 ) );
					SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_LINESCROLL, 0, (LPARAM)abs( idx - idx2 ) );
				}
			}
			break;*/

		case WM_HSCROLL:
			if( subclassId == 0 ) // left rich-edit scroll
			{
				POINT pt;
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETSCROLLPOS, 0, (LPARAM)&pt );
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_SETSCROLLPOS, 0, (LPARAM)&pt );
			}
			else // right rich-edit scroll
			{
				POINT pt;
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETSCROLLPOS, 0, (LPARAM)&pt );
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_SETSCROLLPOS, 0, (LPARAM)&pt );
			}
			break;

		case WM_MOUSEWHEEL:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				setZoom( GET_WHEEL_DELTA_WPARAM( wp ) );
			else
			{
				auto idx1 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETFIRSTVISIBLELINE, 0, 0 );
				auto idx2 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETFIRSTVISIBLELINE, 0, 0 );

				LPARAM off1, off2;
				off1 = off2 = GET_WHEEL_DELTA_WPARAM( wp ) / 40 * -1;

				if( idx1 != idx2 )
				{
					auto off = max( idx1, idx2 ) - min( idx1, idx2 );

					if( subclassId == 0 )
						off2 += ( idx1 > idx2 ) ? off : -off;
					else
						off1 += ( idx2 > idx1 ) ? off : -off;
				}

				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_LINESCROLL, 0, (LPARAM)off1 );
				SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_LINESCROLL, 0, (LPARAM)off2 );

				// check the scroll position
				idx1 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_GETFIRSTVISIBLELINE, 0, 0 );
				idx2 = SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_GETFIRSTVISIBLELINE, 0, 0 );

				if( idx1 != idx2 )
				{
					// try to correct the scroll position
					if( subclassId == 0 )
						SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_RIGHT ), EM_LINESCROLL, 0, (LPARAM)( idx1 - idx2 ) );
					else
						SendMessage( GetDlgItem( _hDlg, IDC_COMPARERICHEDIT_LEFT ), EM_LINESCROLL, 0, (LPARAM)( idx2 - idx1 ) );

					PrintDebug("[%d:%d]", idx1, idx2);
				}
			}
			return 0;

		default:
		//	PrintDebug("msg 0x%04X: 0x%04X, 0x%04X", message, LOWORD(wp), HIWORD(wp));
			break;
		}

		return DefSubclassProc( hWnd, message, wp, lp ) & ( ( message == WM_GETDLGCODE ) ? ~DLGC_HASSETSEL : ~0 );
	}
}
