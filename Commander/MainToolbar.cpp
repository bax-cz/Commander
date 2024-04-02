#include "stdafx.h"

#include "Commander.h"
#include "IconUtils.h"
#include "MainToolbar.h"

#include <gdiplus.h>

#define TOOLBARBUTTONS_COUNT   24
#define INSERT_TOOLBAR_BUTTON( style, cmd, text, ibmp ) \
	tbb[id].iBitmap = (ibmp); \
	tbb[id].fsState = TBSTATE_ENABLED; \
	tbb[id].fsStyle = (style); \
	tbb[id].idCommand = (cmd); \
	tbb[id++].iString = ((text) ? SendMessage( _hToolBar, TB_ADDSTRING, 0, (LPARAM)(text) ) : 0 );

namespace Commander
{
	//
	// Constructor/destructor
	//
	CMainToolbar::CMainToolbar()
	{
		_hToolBar = nullptr;
		_imgListToolbar = nullptr;
		_imgListToolbarHot = nullptr;
		_imgListToolbarDisabled = nullptr;
	}

	CMainToolbar::~CMainToolbar()
	{
		ImageList_Destroy( _imgListToolbar );
		ImageList_Destroy( _imgListToolbarHot );
		ImageList_Destroy( _imgListToolbarDisabled );
	}

	//
	// Load image lists from resource
	//
	void CMainToolbar::initImageLists()
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
	// Create and init toolbar
	//
	void CMainToolbar::init()
	{
		_hToolBar = CreateWindowEx( 0, TOOLBARCLASSNAME, NULL, TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | WS_CHILD,
			0, 0, 0, 0, FCS::inst().getFcWindow(), (HMENU)IDM_MAINMENU_COMMANDER, FCS::inst().getFcInstance(), NULL );

		TbUtils::setExtStyle( _hToolBar, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_DOUBLEBUFFER );

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
	// Add toolbar buttons
	//
	void CMainToolbar::addToolbarButtons()
	{
		// load the common controls images which seem to need an already loaded ImageList
		DWORD imgCount = static_cast<DWORD>( SendMessage( _hToolBar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL ) );

		// populate array of button describing structures
		TBBUTTON tbb[TOOLBARBUTTONS_COUNT] = { 0 };
		UINT id = 0; // id is incremented inside INSERT_TOOLBAR_BUTTON macro

		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_GOTO_PARENTDIRECTORY, L"Parent Directory (Backspace)", 13 );
		INSERT_TOOLBAR_BUTTON( BTNS_DROPDOWN, IDM_GOTO_BACK, L"Back (Alt+Left Arrow)", 16 );
		INSERT_TOOLBAR_BUTTON( BTNS_DROPDOWN, IDM_GOTO_FORWARD, L"Forward (Alt+Right Arrow)", 17 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_EDIT_SELECTALL, L"Select All (Ctrl+A)", 48 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_EDIT_UNSELECTALL, L"Unselect All (Ctrl+D)", 52 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_EDIT_CUT, L"Clipboard Cut (Ctrl+X)", 32 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_EDIT_COPY, L"Clipboard Copy (Ctrl+C)", 33 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_EDIT_PASTE, L"Clipboard Paste (Ctrl+V)", 34 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_FILES_PACK, L"Pack (Alt+F5)", 49 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_FILES_UNPACK, L"Unpack (Alt+F9)", 50 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_FILES_PROPERTIES, L"Properties (Alt+Enter)", 36 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_FILES_CHANGECASE, L"Change Case (Ctrl+F7)", 30 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_FILES_CHANGEATTRIBUTES, L"Change Attributes (Ctrl+F2)", 20 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_COMMANDS_FINDFILES, L"Find Files (Alt+F7)", 6 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_COMMANDS_COMPAREDIRECTORIES, L"Compare Directories (Ctrl+F10)", 37 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_COMMANDS_COMMANDSHELL, L"Command Shell (Num /)", 22 );
		INSERT_TOOLBAR_BUTTON( BTNS_SEP, 0, 0, 0 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_OPENFOLDER_ACTIVEFOLDER, L"Open Active Folder (Shift+F3)", 74 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_OPENFOLDER_CONTROLPANEL, L"Open Control Panel", 75 );
		INSERT_TOOLBAR_BUTTON( BTNS_BUTTON, IDM_OPENFOLDER_RECYCLEBIN, L"Open Recycle Bin", 76 );

		_ASSERTE( id == ARRAYSIZE( tbb ) );

		// send the TB_BUTTONSTRUCTSIZE message, which is required for backwards compatibility
		SendMessage( _hToolBar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof( TBBUTTON ), 0 );

		// load the button structs into the toolbar to create the buttons
		SendMessage( _hToolBar, TB_ADDBUTTONS, ARRAYSIZE( tbb ), (LPARAM)&tbb );

		// resize the toolbar, and then show it
		SendMessage( _hToolBar, TB_AUTOSIZE, 0, 0 );
		ShowWindow( _hToolBar, SW_SHOWNORMAL );
	}
}
