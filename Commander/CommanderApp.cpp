#include "stdafx.h"

#include "Commander.h"
#include "CommanderApp.h"

#include "IconUtils.h"
#include "MenuUtils.h"
#include "MiscUtils.h"
#include "NetworkUtils.h"
#include "StringUtils.h"
#include "ToolBarUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CCommanderApp::CCommanderApp()
		: _panelLeft( 0 )
		, _panelRight( 1 )
		, _findFlags( 0 )
	{
		_activePanelId = _panelLeft.id();
		_isInitialized = false;
		_isSwaped = false;

		_neighborsFinished = false;

		_imgListSystem = nullptr;
		_imgListReged = nullptr;
		_hMenuBar = nullptr;
		_hGuiFont = nullptr;
		_hViewFont = nullptr;
		_pTaskbarList = nullptr;
	}

	CCommanderApp::~CCommanderApp()
	{
		cleanUp();
	}

	//
	// Init UI objects - fonts
	//
	void CCommanderApp::initGdiObjects()
	{
		_hGuiFont = CreateFont(
			16, 0, 0, 0,
			FW_REGULAR,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			TEXT( "Segoe UI" ) );

		_hViewFont = CreateFont(
			16, 0, 0, 0,
			FW_REGULAR,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			TEXT( "Consolas" ) );

		_hBrushBkgnd = CreateSolidBrush( FC_COLOR_DIRBOXBKGND );
		_hBrushBkgndWarn = CreateSolidBrush( FC_COLOR_DIRBOXBKGNDWARN );
		_hBrushActive = CreateSolidBrush( FC_COLOR_DIRBOXACTIVE );
		_hBrushActiveWarn = CreateSolidBrush( FC_COLOR_DIRBOXACTIVEWARN );
		_hBrushGreenOk = CreateSolidBrush( FC_COLOR_GREENOK );
	}

	//
	// Init system menu - insert Always on top item
	//
	void CCommanderApp::initSystemMenu()
	{
		auto hSysMenu = GetSystemMenu( FCS::inst().getFcWindow(), FALSE );
		auto cnt = GetMenuItemCount( hSysMenu );

		InsertMenu( hSysMenu, cnt - 2, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );
		InsertMenu( hSysMenu, cnt - 1, MF_BYPOSITION | MF_STRING, IDM_OPTIONS_ALWAYSONTOP, L"Always on Top" );
	}

	//
	// Init UI objects - image lists
	//
	void CCommanderApp::initImageLists()
	{
		// get system image list
		SHFILEINFO shfi = { 0 };

		_imgListSystem = (HIMAGELIST)SHGetFileInfo( L"", 0, &shfi, sizeof( SHFILEINFO ), SHGFI_SYSICONINDEX | SHGFI_SMALLICON );
		_imgListReged = ImageList_Create( 16, 16, ILC_COLOR32, 3, 0 );

		auto hIcon = IconUtils::getStock( SIID_FOLDER );

		ImageList_AddIcon( _imgListReged, hIcon );
		ImageList_AddIcon( _imgListReged, LoadIcon( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_REGEDTEXT ) ) );
		ImageList_AddIcon( _imgListReged, LoadIcon( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDI_REGEDBIN ) ) );

		DestroyIcon( hIcon );
	}

	//
	// Init OS version info
	//
	void CCommanderApp::initOsVersion()
	{
		_ovi.dwOSVersionInfoSize = sizeof( _ovi );
		GetVersionEx( (OSVERSIONINFO*)&_ovi );

		// windows 7 or later
		if( _ovi.dwMajorVersion >= 7 || _ovi.dwMajorVersion == 6 && _ovi.dwMinorVersion >= 1 )
			_findFlags = FIND_FIRST_EX_LARGE_FETCH;
		else
			_findFlags = 0;
	}

	//
	// Deinit UI objects - fonts, image lists, etc.
	//
	void CCommanderApp::cleanUp()
	{
		KillTimer( FCS::inst().getFcWindow(), FC_TIMER_NEIGHBORS_ID );

		_workerNeighbors.stop();

		_panelLeft.cleanUp();
		_panelRight.cleanUp();

		DeleteObject( _hGuiFont );
		DeleteObject( _hViewFont );
		DeleteObject( _hBrushBkgnd );
		DeleteObject( _hBrushBkgndWarn );
		DeleteObject( _hBrushActive );
		DeleteObject( _hBrushActiveWarn );
		DeleteObject( _hBrushGreenOk );

		ImageList_Destroy( _imgListSystem ); // delete or not??
		ImageList_Destroy( _imgListReged );

		ShellUtils::safeRelease( &_pTaskbarList );

		bcb::PuttyFinalize();

		OleUninitialize(); // calls the CoUninitialize function internally
	}

	//
	// Load neighbourhood network computers
	//
	bool CCommanderApp::_loadNetNeighbors()
	{
		LPNETRESOURCE pNetResource = NULL;
		auto ret = NetUtils::enumNetworkResources( pNetResource, &_workerNeighbors );

		// when WNetEnumResource for some reason doesn't work, try it other way
		if( _neighborsTemp.empty() )
		{
			auto neighbors = ShellUtils::getNeighborhoodList();
			for( auto& neighbor : neighbors )
			{
				_workerNeighbors.sendMessage( RESOURCEDISPLAYTYPE_SERVER, reinterpret_cast<LPARAM>( neighbor.c_str() ) );

				DWORD result = 0;
				auto folders = NetUtils::getSharedFolders( &neighbor[0], true, &result );
				for( auto& folder : folders )
				{
					folder = PathUtils::addDelimiter( neighbor ) + folder;
					_workerNeighbors.sendMessage( RESOURCEDISPLAYTYPE_SHARE, reinterpret_cast<LPARAM>( folder.c_str() ) );
				}
			}
		}

		return ret;
	}

	//
	// Updates all Commander's child controls placement
	//
	void CCommanderApp::updateLayout()
	{
		RECT rctFc = { 0 };
		GetClientRect( FCS::inst().getFcWindow(), &rctFc );

		RECT rctLeft = { 1, 1, rctFc.right / 2, rctFc.bottom - 1 };
		RECT rctRight = { rctFc.right / 2 + 1, 1, rctFc.right - 1, rctFc.bottom - 1 };

		_panelLeft.updateLayout( _isSwaped ? rctRight : rctLeft );
		_panelRight.updateLayout( _isSwaped ? rctLeft : rctRight );

		_toolbarMain.updateLayout();
	}

	//
	// Set FC's main windows as Topmost
	//
	void CCommanderApp::setAlwaysOnTop()
	{
		auto hSysMenu = GetSystemMenu( FCS::inst().getFcWindow(), FALSE );

		if( MenuUtils::checkItem( _hMenuBar, IDM_OPTIONS_ALWAYSONTOP ) )
		{
			SetWindowPos( FCS::inst().getFcWindow(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
			CheckMenuItem( hSysMenu, GetMenuItemCount( hSysMenu ) - 3, MF_BYPOSITION | MF_CHECKED );
		}
		else
		{
			SetWindowPos( FCS::inst().getFcWindow(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
			MenuUtils::checkItem( _hMenuBar, IDM_OPTIONS_ALWAYSONTOP, false );
			CheckMenuItem( hSysMenu, GetMenuItemCount( hSysMenu ) - 3, MF_BYPOSITION | MF_UNCHECKED );
		}
	}

	//
	// Process context menu with custom commands
	//
	UINT CCommanderApp::processContextMenu( CShellContextMenu& ctx, HWND hWnd, const std::vector<std::wstring>& items, const POINT& pt, bool isFile )
	{
		if( isFile )
		{
			if( items.size() == 1 )
			{
				// insert custom commands into the context menu
				ctx.addCustomCommand( L"Compare file #1", 1 );

				if( !_compareFile1.empty() )
					ctx.addCustomCommand( L"Compare file #2", 2 );
			}
			else if( items.size() == 2 )
			{
				// insert custom command into the context menu
				ctx.addCustomCommand( L"Compare 2 files", 2 );
			}
		}

		// show context menu
		UINT ret = ctx.show( hWnd, items, pt );

		// check if custom command has been selected
		switch( ret )
		{
		case 1:
			_compareFile1 = items[0];
			break;
		case 2:
			if( items.size() == 2 )
			{
				_compareFile1 = items[0];
				_compareFile2 = items[1];
			}
			else
				_compareFile2 = items[0];
			break;
		default:
			break;
		}

		return ret;
	}

	//
	// Swap left and right panels
	//
	void CCommanderApp::swapPanels()
	{
		_isSwaped = !_isSwaped;

		updateLayout();

		_panelLeft.getActiveTab()->show( SW_HIDE );
		_panelLeft.getActiveTab()->show();

		_panelRight.getActiveTab()->show( SW_HIDE );
		_panelRight.getActiveTab()->show();

		FCS::inst().getApp().getActivePanel().setPanelFocus();
	}

	//
	// Set focus to active panel
	//
	void CCommanderApp::onActivate( bool activated )
	{
		_isActivated = activated;

		if( _isInitialized )
		{
			// set focus to active panel
			FCS::inst().getApp().getActivePanel().setPanelFocus( activated );
		}
	}

	//
	// On system change notification - media changed, drive added, etc.
	//
	void CCommanderApp::onSysChangeNotify( HANDLE hChange, DWORD dwProcId )
	{
		LPITEMIDLIST *pidl;
		LONG eventId;
		HANDLE hLock = SHChangeNotification_Lock( hChange, dwProcId, &pidl, &eventId );

		auto path = ShellUtils::getPathFromPidl( *pidl );
		bool added = ( eventId == SHCNE_DRIVEADD || eventId == SHCNE_MEDIAINSERTED || eventId == SHCNE_NETSHARE );

		switch( eventId )
		{
		case SHCNE_DRIVEADD:
		case SHCNE_DRIVEREMOVED:
			_drivesMenu.driveAdded( path, added );
			break;
		case SHCNE_MEDIAINSERTED:
		case SHCNE_MEDIAREMOVED:
			_drivesMenu.mediaInserted( path, added );
			break;
		case SHCNE_NETSHARE:
		case SHCNE_NETUNSHARE:
			_drivesMenu.driveAdded( path, added );
			break;
		}

		SHChangeNotification_Unlock( hLock );

		FCS::inst().getApp().getPanelLeft().getActiveTab()->driveChanged( path, added );
		FCS::inst().getApp().getPanelRight().getActiveTab()->driveChanged( path, added );
	}

	//
	// Neighbors enumeration update - resource found
	//
	void CCommanderApp::onNeigborsUpdate( int cmd, WCHAR *resourceName )
	{
		switch( cmd )
		{
		case 0:
		case 1:
			neighborsFinished( !!cmd );
			break;
		default:
			if( resourceName ) {
				_neighborsTemp.push_back(
					StringUtils::join( { std::to_wstring( cmd ), resourceName } ) );
			}
			break;
		}
	}

	//
	// Loading neighbors finished - restart timer procedure
	//
	void CCommanderApp::neighborsFinished( bool success )
	{
		if( success )
		{
			if( !MiscUtils::compareValues( _neighbors, _neighborsTemp ) )
			{
				_neighbors = _neighborsTemp;

				if( _panelLeft.getActiveTab()->getDataManager()->isInNetworkMode() )
					_panelLeft.refresh();

				if( _panelRight.getActiveTab()->getDataManager()->isInNetworkMode() )
					_panelRight.refresh();
			}

			PrintDebug("Neighbors finished loading..");
		}
		else
			PrintDebug("Neighbors canceled..");

		_neighborsFinished = true;

		// periodically read neighbors in timer procedure
		SetTimer( FCS::inst().getFcWindow(), FC_TIMER_NEIGHBORS_ID, FC_TIMER_NEIGHBORS_TICK, NULL );
	}

	//
	// Update neighbours every 20 seconds
	//
	void CCommanderApp::onTimer( int timerId )
	{
		KillTimer( FCS::inst().getFcWindow(), FC_TIMER_NEIGHBORS_ID );

		_neighborsTemp.clear();
		_workerNeighbors.start();
	}

	//
	// Creates all Commander's child controls
	//
	void CCommanderApp::initApp()
	{
		// load network resources
		_workerNeighbors.init( [this] { return _loadNetNeighbors(); }, FCS::inst().getFcWindow(), UM_NEIGHBORDONE );
		_workerNeighbors.start();

		// init FastCopy stuff
		InitInstanceForLoadStr( FCS::inst().getFcInstance() );
		TSetDefaultLCID();

		// seeding pseudo-random number generator
		srand( (unsigned int)time( NULL ) );

		// init PuTTY
		bcb::PuttyInitialize();

		// init UI elements
		initGdiObjects();
		initSystemMenu();
		initImageLists();
		initOsVersion();
		_toolbarMain.init();
		_drivesMenu.init();
		updateLayout();

		CoCreateInstance( CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList4, (void**)&_pTaskbarList );
		_hMenuBar = GetMenu( FCS::inst().getFcWindow() );

		// TODO: read from settings manager ??
		// TEST: L"D:\\temp\\emulator\\xenia"
		_panelLeft.init( FsUtils::getClosestExistingDir( ShellUtils::getKnownFolderPath( FOLDERID_Documents ) ) );
		_panelRight.init( FsUtils::getClosestExistingDir( ShellUtils::getKnownFolderPath( FOLDERID_Downloads ) ) );

		_isInitialized = true;
	}
}
