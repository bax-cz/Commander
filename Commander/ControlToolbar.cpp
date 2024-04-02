#include "stdafx.h"

#include "Commander.h"

#include "ControlToolbar.h"
#include "ToolBarUtils.h"
#include "IconUtils.h"
#include "ShellUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CToolBar::CToolBar()
	{
		_imageIndex = 0;
	}

	CToolBar::~CToolBar()
	{
		DestroyWindow( _hWnd );
	}


	//
	// Create toolbar control and intialize it
	//
	bool CToolBar::init( CFcPanel *const panel, CPanelTab *const panelTab )
	{
		_hWnd = CreateWindowEx(
			0,
			TOOLBARCLASSNAME,
			NULL,
			TBSTYLE_TOOLTIPS | WS_CHILD | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER,
			_rect.left, _rect.top, _rect.right - _rect.left, _rect.bottom - _rect.top,
			FCS::inst().getFcWindow(),
			NULL,
			FCS::inst().getFcInstance(),
			NULL );

		// store parent panel and tab pointers
		_pParentPanel = panel;

		// register this window procedure
		FCS::inst().getApp().getControls()[_hWnd] = this;

		TbUtils::setExtStyle( _hWnd, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_DOUBLEBUFFER );
		SendMessage( _hWnd, TB_SETIMAGELIST, 0, (LPARAM)FCS::inst().getApp().getSystemImageList() );

		// Sending this ensures iString(s) of TBBUTTON structs become tooltips rather than button text
		SendMessage( _hWnd, TB_SETMAXTEXTROWS, 0, 0 );

		addToolbarButton();
		ShowWindow( _hWnd, TRUE );

		return _hWnd != NULL;
	}

	//
	// Add toolbar change drive button
	//
	void CToolBar::addToolbarButton()
	{
		TBBUTTON tbb[1] = { 0 };
		tbb[0].iBitmap = 0;
		tbb[0].fsState = TBSTATE_ENABLED;
		tbb[0].fsStyle = BTNS_WHOLEDROPDOWN;
		tbb[0].idCommand = IDM_TOOLBAR_CHANGEDRIVE;
		tbb[0].iString = SendMessage( _hWnd, TB_ADDSTRING, 0, (LPARAM)L"Current drive" );

		// Send the TB_BUTTONSTRUCTSIZE message, which is required for backward compatibility.
		SendMessage( _hWnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof( TBBUTTON ), 0 );

		// Load the button structs into the toolbar to create the buttons
		SendMessage( _hWnd, TB_ADDBUTTONS, ARRAYSIZE( tbb ), (LPARAM)&tbb );
	}

	//
	// Update toolbar button image
	//
	void CToolBar::updateToolbarImage( const std::wstring& reqDrive )
	{
		TBBUTTONINFO tbbInfo = { 0 };
		tbbInfo.cbSize = sizeof( tbbInfo );
		tbbInfo.dwMask = TBIF_IMAGE;

		if( _pParentPanel->getActiveTab()->getDataManager()->isInArchiveMode() ) {
			tbbInfo.iImage = IconUtils::getStockIndex( SIID_ZIPFILE );
		}
		else if( _pParentPanel->getActiveTab()->getDataManager()->isInNetworkMode() ) {
			tbbInfo.iImage = IconUtils::getStockIndex( SIID_MYNETWORK );
		}
		else if( _pParentPanel->getActiveTab()->getDataManager()->isInFtpMode() ) {
			tbbInfo.iImage = IconUtils::getStockIndex( SIID_INTERNET );
		}
		else if( _pParentPanel->getActiveTab()->getDataManager()->isInPuttyMode() ) {
			tbbInfo.iImage = IconUtils::getSpecialIndex( _hWnd, CSIDL_CONNECTIONS );
		}
		else if( _pParentPanel->getActiveTab()->getDataManager()->isInRegedMode() ) { // TODO
			tbbInfo.iImage = IconUtils::getFromPathIndex( ( FsUtils::getSystemDirectory() + L"\\regedit.exe" ).c_str() );
		}
		else if( PathUtils::isUncPath( reqDrive ) ) {
			tbbInfo.iImage = IconUtils::getStockIndex( SIID_DRIVENET );
		}
		else
			tbbInfo.iImage = IconUtils::getFromPathIndex( PathUtils::getFullPath( reqDrive ).c_str() );

		_imageIndex = tbbInfo.iImage;

		SendMessage( _hWnd, TB_SETBUTTONINFO, (WPARAM)IDM_TOOLBAR_CHANGEDRIVE, (LPARAM)&tbbInfo );
	}

	//
	// Show change drives menu
	//
	void CToolBar::showDropDownList()
	{
		RECT rct;
		GetWindowRect( _hWnd, &rct );

		POINT pt; pt.x = rct.left; pt.y = rct.bottom;
		ScreenToClient( _hWnd, &pt );

		SendMessage( _hWnd, TB_PRESSBUTTON, (WPARAM)IDM_TOOLBAR_CHANGEDRIVE, (LPARAM)MAKELONG( TRUE, 0 ) );

		NMTOOLBAR tbHdr;
		tbHdr.iItem = IDM_TOOLBAR_CHANGEDRIVE;
		tbHdr.rcButton.left = pt.x;
		tbHdr.rcButton.bottom = pt.y + 1;

		showDropDownMenu( &tbHdr );

		SendMessage( _hWnd, TB_PRESSBUTTON, (WPARAM)IDM_TOOLBAR_CHANGEDRIVE, (LPARAM)MAKELONG( FALSE, 0 ) );
	}

	void CToolBar::showDropDownMenu( LPNMTOOLBAR tbHdr )
	{
		if( tbHdr->iItem == IDM_TOOLBAR_CHANGEDRIVE )
		{
			_pParentPanel->getActiveTab()->setPanelFocus();

			HMENU hMenu = FCS::inst().getApp().getDrivesMenu().getHandle();
			if( hMenu )
			{
				POINT pt;
				pt.x = tbHdr->rcButton.left;
				pt.y = tbHdr->rcButton.bottom;
				ClientToScreen( _hWnd, &pt );

				auto itemId = TrackPopupMenu( hMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, 0, _hWnd, NULL );

				handleCommands( itemId );
			}
		}
	}

	void CToolBar::handleCommands( UINT cmdId )
	{
		switch( cmdId )
		{
		case IDM_OPENFOLDER_MYDOCUMENTS:
			_pParentPanel->getActiveTab()->changeDirectory( ShellUtils::getKnownFolderPath( FOLDERID_Documents ) );
			break;

		case IDM_TOOLBAR_FTPCLIENT:
			_pParentPanel->getActiveTab()->processCommand( EFcCommand::GoFtp );
			break;

		case IDM_TOOLBAR_PUTTY:
			_pParentPanel->getActiveTab()->processCommand( EFcCommand::GoPutty );
			break;

		case IDM_OPENFOLDER_NETWORK:
			_pParentPanel->getActiveTab()->processCommand( EFcCommand::GoNetwork );
			break;

		case IDM_TOOLBAR_REGED:
			_pParentPanel->getActiveTab()->processCommand( EFcCommand::GoReged );
			break;

		case IDM_LEFT_GOTO_PATHOTHERPANEL:
			_pParentPanel->getActiveTab()->processCommand( EFcCommand::GoPathOtherPanel );
			break;

		default: // quick-change current drive (A-Z)
			if( cmdId >= 0x41 && cmdId <= 0x5A )
			{
				WCHAR driveName[] = { (WCHAR)cmdId, L':', L'\\', L'\0' };
				_pParentPanel->getActiveTab()->changeDrive( driveName );
			}
			break;
		}
	}

	//
	// Subclassed window procedure
	//
	LRESULT CALLBACK CToolBar::wndProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_NOTIFY:
			switch( reinterpret_cast<LPNMHDR>( lParam )->code )
			{
			case TBN_DROPDOWN:
				showDropDownMenu( reinterpret_cast<LPNMTOOLBAR>( lParam ) );
				return TBDDRET_DEFAULT;
			}
			break;

		default:
			break;
		}

		return (LRESULT)false;
	}
}
