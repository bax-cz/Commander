//
// Commander.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <objbase.h>
#include <shlobj.h>

#include "Commander.h"
#include "CommandLineParser.h"
#include "ShellUtils.h"
#include "StringUtils.h"
#include "MiscUtils.h"

// Forward declarations of functions included in this code module:
ATOM registerFcClass( HINSTANCE hInstance );

//
// Global Variables
//

const int MAX_LOADSTRING = 100;

// Commander's temporary data storage directory
std::wstring Commander::g_tempPath = Commander::FsUtils::getTempDirectory() + L"_fcommander\\";

// Parse command line parameters (does parsing in ctor), currently only tempdir is supported
Commander::CCommandLineParser parseCmdLine;

#ifdef FC_ENABLE_LOGGING
Commander::CLogLite g_logger; // global lite-logger instance - needs to be destroyed last
#endif

TCHAR sFcTitle[MAX_LOADSTRING];  // main window title bar text
TCHAR sFcClass[MAX_LOADSTRING];  // main window class name

ULONG _mediaChangeNotifyId = 0;

using namespace Commander;

#if defined(_DEBUG) || defined(FC_ENABLE_LOGGING)
// Write debugging information into the debug output window and/or a log file
// (Called through PrintDebug macro)
void _printDebugImpl( const wchar_t *funcName, const wchar_t *fmt, ... )
{
	std::wstring strOut = L"[";
	strOut += funcName;
	strOut += L"] ";

	va_list args;
	va_start( args, fmt );

	// get characters count including trailing '\0'
	int fmtLen = _vscwprintf( fmt, args ) + 1;
	int strLen = (int)strOut.length();

	strOut.resize( fmtLen + strLen );

	// extract string
	int ret = vswprintf( &strOut[0] + strLen, fmtLen, fmt, args );
	*strOut.rbegin() = L'\n';

	va_end( args );

#ifdef FC_ENABLE_LOGGING
	// log to the file
	g_logger.write( strOut.c_str() );
	g_logger.flush();
#endif
	// write to debug output
	OutputDebugString( strOut.c_str() );
}
#endif // _DEBUG || FC_ENABLE_LOGGING

// Format string - printf style
// (Called through FORMAT macro)
std::wstring Commander::formatStrImpl( const wchar_t *fmt, ... )
{
	va_list args;
	va_start( args, fmt );

	// get characters count including trailing '\0'
	int strLen = _vscwprintf( fmt, args ) + 1;

	std::wstring strOut;
	strOut.resize( strLen );

	// extract string
	int ret = vswprintf( &strOut[0], strLen, fmt, args );

	if( !strOut.empty() && strLen == ret + 1 )
		strOut.pop_back(); // remove trailing '\0'

	va_end( args );

	return strOut;
}

//
//  Commander's Main Entry point
//
int APIENTRY _tWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow )
{
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );
	UNREFERENCED_PARAMETER( nCmdShow );

	OleInitialize( NULL ); // calls CoInitializeEx internally to initialize the COM library (single-thread apartment)

	//SetProcessDPIAware(); // TODO: do some testing

	// initialize global strings
	LoadString( hInstance, IDS_COMMANDER_TITLE, sFcTitle, MAX_LOADSTRING );
	LoadString( hInstance, IDS_COMMANDER_CLASS, sFcClass, MAX_LOADSTRING );

	registerFcClass( hInstance );

	// create the main Commander window
	CreateWindowEx(
		WS_EX_CONTROLPARENT | WS_EX_APPWINDOW,
		sFcClass,
		sFcTitle,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		0,
		CW_USEDEFAULT,
		0,
		NULL,
		NULL,
		hInstance,
		NULL );

	HACCEL hAccelTable = LoadAccelerators( hInstance, MAKEINTRESOURCE( IDA_COMMANDER_ACCEL ) );
	MSG msg;

	// Main message loop
	while( GetMessage( &msg, NULL, 0, 0 ) )
	{
		if( !TranslateAccelerator( msg.hwnd, hAccelTable, &msg )
		 // forward ENTER and TAB keys to the active list-view's edit-box (for quick find)
		 && !( FCS::inst().getApp().getActivePanel().getActiveTab()->isQuickFindMessage( &msg ) )
		 // send messages to currently active modeless dialog
		 && !( FCS::inst().getActiveDialog() && IsDialogMessage( FCS::inst().getActiveDialog(), &msg ) )
		 // send messages to main Commander's controls
		 && !IsDialogMessage( FCS::inst().getFcWindow(), &msg ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}

	PrintDebug("WinMain return code: %d", (int)msg.wParam);

	return (int)msg.wParam;
}

//
//  Registers the Commander window class.
//
ATOM registerFcClass( HINSTANCE hInstance )
{
	// store handle instance in singleton
	FCS::inst().setFcInstance( hInstance );

	WNDCLASSEX wcex;
	wcex.cbSize        = sizeof( WNDCLASSEX );
	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = Commander::wndProcFcMain;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = hInstance;
	wcex.hIcon         = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_COMMANDER_ICON ) );
	wcex.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wcex.hbrBackground = (HBRUSH)( COLOR_WINDOWFRAME );
	wcex.lpszMenuName  = MAKEINTRESOURCE( IDM_MAINMENU_COMMANDER );
	wcex.lpszClassName = sFcClass;
	wcex.hIconSm       = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_COMMANDER_ICON ) );

	return RegisterClassEx( &wcex );
}

//
//  Registers file system shell notification for removable media change.
//
bool registerMediaChangeNotify()
{
	LPITEMIDLIST pidl;

	HRESULT hr = SHGetFolderLocation( FCS::inst().getFcWindow(), CSIDL_DESKTOP, NULL, 0, &pidl );

	if( SUCCEEDED( hr ) )
	{
		SHChangeNotifyEntry const shCNE[] = { pidl, FALSE };

		// SHCNRF_NewDelivery flag
		// MSDN: We recommend this flag because it provides a more robust delivery method. All clients should specify this flag.
		_mediaChangeNotifyId = SHChangeNotifyRegister(
			FCS::inst().getFcWindow(),
			SHCNRF_ShellLevel | SHCNRF_NewDelivery,
			SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED | SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED | SHCNE_NETSHARE | SHCNE_NETUNSHARE,
			UM_CHANGENOTIFY,
			ARRAYSIZE( shCNE ),
			shCNE );

		// MSDN: When using Windows 2000 or later, use CoTaskMemFree rather than ILFree.
		CoTaskMemFree( pidl );

		return _mediaChangeNotifyId != 0;
	}

	return false;
}

//
//  Deregisters file system shell notification.
//
bool deregisterMediaChangeNotify()
{
	if( _mediaChangeNotifyId )
	{
		bool ret = !!SHChangeNotifyDeregister( _mediaChangeNotifyId );
		_mediaChangeNotifyId = 0;

		return ret;
	}

	return false;
}

//
//  Loads Commander's window controls layout.
//
void loadFcLayout( HWND hWndFC )
{
	// TODO
	SetWindowPos( hWndFC, NULL, 0, 0, 800, 600, SWP_NOMOVE );
}

//
//  Saves Commander's window controls layout.
//
void saveFcLayout( HWND hWndFC )
{
	// TODO
	WINDOWPLACEMENT wndplc = { 0 };
	GetWindowPlacement( hWndFC, &wndplc );
}

//
//  In this function, we save main window handle in a singleton object and
//  initialize and display the main program window.
//
void onInitCommander( HWND hWndFc )
{
	// store window handle in singleton
	FCS::inst().setFcWindow( hWndFc );

	// needed for list-views, status-bars and other controls
	INITCOMMONCONTROLSEX icx = { 0 };
	icx.dwSize = sizeof( INITCOMMONCONTROLSEX );
	icx.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES;
	InitCommonControlsEx( &icx );

	// initialize Commander's controls layout
	FCS::inst().getApp().initApp();

	registerMediaChangeNotify();

	loadFcLayout( hWndFc );

	ShowWindow( hWndFc, SW_SHOWDEFAULT );
	UpdateWindow( hWndFc );
}

//
//  Show MessageBox with given info and report back its closure
//
void onReportMessage( HWND hSender, WCHAR *text )
{
	auto info = StringUtils::split( text );

	MessageBox( hSender, info[1].c_str(), info[0].c_str(), MB_OK | MB_ICONEXCLAMATION );

	if( IsWindow( hSender ) )
		PostMessage( hSender, UM_REPORTMSGBOX, 0, 0 );
}

//
//  Stop current fast copy operations and exit.
//
bool onCloseCommander()
{
	// close dialogs in reverse order
	auto& dialogs = FCS::inst().getApp().getDialogs();
	for( auto it = dialogs.rbegin(); it != dialogs.rend(); )
	{
		(*it)->close( true ); // force close
		it = dialogs.rbegin();
	}

	return FCS::inst().getFastCopyManager().stop();
}

//
//  Handles Commander application finish and cleans-up.
//
void onDestroyCommander( HWND hWnd )
{
	saveFcLayout( hWnd );
	deregisterMediaChangeNotify();
	PostQuitMessage( 0 );
}

//
//  Processes messages for the main window.
//
LRESULT CALLBACK Commander::wndProcFcMain( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	HDC hdc;

//	PrintDebug("0x%08X, 0x%08X", message, hWnd);

	switch( message )
	{
	case WM_CREATE:
		onInitCommander( hWnd );
		break;

	case WM_ACTIVATE:
		FCS::inst().getApp().onActivate( wParam ? true : false );
		break;

	case WM_NOTIFY:
		// forward notification messages to particular child window
		return CControl::controlsWndProcMain( ( reinterpret_cast<LPNMHDR>( lParam ) )->hwndFrom, message, wParam, lParam );

	case UM_REPORTMSGBOX:
		onReportMessage( reinterpret_cast<HWND>( wParam ), reinterpret_cast<WCHAR*>( lParam ) );
		break;

	case UM_CHANGENOTIFY:
		FCS::inst().getApp().onSysChangeNotify( reinterpret_cast<HANDLE>( wParam ), static_cast<DWORD>( lParam ) );
		break;

	case UM_FSPACENOTIFY:
		FCS::inst().getApp().getDrivesMenu().updateFreeSpace( static_cast<WCHAR>( wParam ), static_cast<ULONGLONG>( lParam ) );
		break;

	case UM_NEIGHBORDONE:
		FCS::inst().getApp().onNeigborsUpdate( static_cast<int>( wParam ), reinterpret_cast<WCHAR*>( lParam ) );
		break;

	case WM_TIMER:
		FCS::inst().getApp().onTimer( static_cast<int>( wParam ) );
		break;

	case WM_CTLCOLORSTATIC:
		// forward notification messages to particular child window
		return CControl::controlsWndProcMain( reinterpret_cast<HWND>( lParam ), message, wParam, lParam );

	case WM_COMMAND:
		if( HIWORD( wParam ) == EN_SETFOCUS ) { // forward notification message to particular child window
			return CControl::controlsWndProcMain( reinterpret_cast<HWND>( lParam ), message, wParam, lParam );
		}
		else if( !processMenuCommand( LOWORD( wParam ) ) ) {
			return DefWindowProc( hWnd, message, wParam, lParam );
		}
		break;

	case WM_SYSCOMMAND:
		if( wParam == IDM_OPTIONS_ALWAYSONTOP ) {
			FCS::inst().getApp().setAlwaysOnTop();
			return 0;
		}
		return DefWindowProc( hWnd, message, wParam, lParam );

	case WM_MENUCHAR:
		// disable system "ding" sound when Alt+Enter or Alt+[0-9] keys are pressed
		if( LOWORD( wParam ) == VK_RETURN || ( LOWORD( wParam ) > 0x2F && LOWORD( wParam ) < 0x3A ) ) // && HIWORD( wParam ) == MF_SYSMENU )
			return MAKELONG( 0, MNC_CLOSE );
		break;

	case WM_SIZING:
	case WM_SIZE:
		// update each child control's layout
		FCS::inst().getApp().updateLayout();
		break;

	case WM_PAINT:
		hdc = BeginPaint( hWnd, &ps );
		// TODO - user drawing code
		EndPaint( hWnd, &ps );
		break;

	case WM_QUERYENDSESSION: // TODO
		PrintDebug("WM_QUERYENDSESSION: %ls, %ls, %ls",
			((lParam & ENDSESSION_CLOSEAPP) ? L"ENDSESSION_CLOSEAPP" : L"-"),
			((lParam & ENDSESSION_CRITICAL) ? L"ENDSESSION_CRITICAL" : L"-"),
			((lParam & ENDSESSION_LOGOFF)   ? L"ENDSESSION_LOGOFF"   : L"-"));
		return TRUE;

	case WM_ENDSESSION: // TODO
		PrintDebug("WM_ENDSESSION(%d): %ls, %ls, %ls", wParam,
			((lParam & ENDSESSION_CLOSEAPP) ? L"ENDSESSION_CLOSEAPP" : L"-"),
			((lParam & ENDSESSION_CRITICAL) ? L"ENDSESSION_CRITICAL" : L"-"),
			((lParam & ENDSESSION_LOGOFF)   ? L"ENDSESSION_LOGOFF"   : L"-"));
		//ShutdownBlockReasonCreate( hWnd, L"Don't kill me yet!" );
		//ShutdownBlockReasonDestroy( hWnd );
		break;

	case WM_CLOSE:
		if( onCloseCommander() )
		{
			return DefWindowProc( hWnd, message, wParam, lParam );
		}
		break;

	case WM_DESTROY:
		onDestroyCommander( hWnd );
		break;

	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}
