#include "stdafx.h"

#include "Commander.h"
#include "WebView2Browser.h"

namespace Commander
{
	bool CWebView2Browser::initWebView2Loader()
	{
		pCompareBrowserVersions = nullptr;
		pCreateCoreWebView2Environment = nullptr;
		pCreateCoreWebView2EnvironmentWithOptions = nullptr;
		pGetAvailableCoreWebView2BrowserVersionString = nullptr;

		_hWebView2Inst = LoadLibrary( L"WebView2Loader.dll" );

		if( _hWebView2Inst )
		{
			// get functions pointers
			pCompareBrowserVersions = reinterpret_cast<decltype(pCompareBrowserVersions)>( GetProcAddress( _hWebView2Inst, "CompareBrowserVersions" ) );
			pCreateCoreWebView2Environment = reinterpret_cast<decltype(pCreateCoreWebView2Environment)>( GetProcAddress( _hWebView2Inst, "CreateCoreWebView2Environment" ) );
			pCreateCoreWebView2EnvironmentWithOptions = reinterpret_cast<decltype(pCreateCoreWebView2EnvironmentWithOptions)>( GetProcAddress( _hWebView2Inst, "CreateCoreWebView2EnvironmentWithOptions" ) );
			pGetAvailableCoreWebView2BrowserVersionString = reinterpret_cast<decltype(pGetAvailableCoreWebView2BrowserVersionString)>( GetProcAddress( _hWebView2Inst, "GetAvailableCoreWebView2BrowserVersionString" ) );

			// try to get WebView2 version using the GetAvailableCoreWebView2BrowserVersionString function
			LPWSTR ver = nullptr;

			if( pCompareBrowserVersions &&
				pCreateCoreWebView2Environment &&
				pCreateCoreWebView2EnvironmentWithOptions &&
				pGetAvailableCoreWebView2BrowserVersionString )
			{
				if( SUCCEEDED( pGetAvailableCoreWebView2BrowserVersionString( nullptr, &ver ) ) && ver )
					_webv2Ver = ver;
			}

			if( _webv2Ver.empty() )
			{
				FreeLibrary( _hWebView2Inst );
				_hWebView2Inst = nullptr;

				pCompareBrowserVersions = nullptr;
				pCreateCoreWebView2Environment = nullptr;
				pCreateCoreWebView2EnvironmentWithOptions = nullptr;
				pGetAvailableCoreWebView2BrowserVersionString = nullptr;
			}
		}

		return !_webv2Ver.empty();
	}

	bool CWebView2Browser::createWebView2Environment()
	{
		_ASSERTE( pCreateCoreWebView2EnvironmentWithOptions );

		HRESULT hr = pCreateCoreWebView2EnvironmentWithOptions( nullptr, FCS::inst().getTempPath().c_str(),
			nullptr, WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[this]( HRESULT result, ICoreWebView2Environment *env )
		{
			if( SUCCEEDED( result ) )
			{
				_webv2Env = env;
				result = createWebView2Controller();
			}
			else
				SendNotifyMessage( _hWndParent, UM_STATUSNOTIFY, 0, 0 );

			return result;
		}).Get() );

		if( FAILED( hr ) )
		{
			PrintDebug("WebView2 environment creation failed");
		}

		return SUCCEEDED( hr );
	}

	HRESULT CWebView2Browser::createWebView2Controller()
	{
		// create WebView2Controller and get the WebView2
		HRESULT hr = _webv2Env->CreateCoreWebView2Controller( _hWndParent,
			WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
			[this]( HRESULT result, ICoreWebView2Controller *ctl )
		{
			if( ctl )
			{
				_webv2Ctl = ctl;
				_webv2Ctl->get_CoreWebView2( &_webView2 );

				// add keydown event handler
				_webv2Ctl->add_AcceleratorKeyPressed(
					WRL::Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
						[this]( ICoreWebView2Controller *sender, ICoreWebView2AcceleratorKeyPressedEventArgs *args )
				{
					COREWEBVIEW2_KEY_EVENT_KIND kind;
					args->get_KeyEventKind( &kind );

					if( kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN )
					{
						UINT key;
						if( SUCCEEDED( args->get_VirtualKey( &key ) ) )
						{
							if( key == VK_ESCAPE )
								SendNotifyMessage( _hWndParent, UM_STATUSNOTIFY, 2, 0 );
						}
					}

					return S_OK;
				}).Get(), &_keyPressedToken );
			}

			if( ctl && _webView2 )
			{
				// setup the webview
				WRL::ComPtr<ICoreWebView2Settings> settings;

				// default settings
				_webView2->get_Settings( &settings );
				settings->put_IsScriptEnabled( TRUE );
				settings->put_AreDefaultScriptDialogsEnabled( TRUE );
				settings->put_IsWebMessageEnabled( TRUE );

				// resize webview to fit the bounds of the dialog window
				RECT rct;
				GetClientRect( _hWndParent, &rct );
				_webv2Ctl->put_Bounds( rct );

				// set focus to webview2 control
				COREWEBVIEW2_MOVE_FOCUS_REASON reason;
				reason = COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC;
				_webv2Ctl->MoveFocus( reason );
			}

			SendNotifyMessage( _hWndParent, UM_STATUSNOTIFY, ctl && _webView2, 0 );

			return result;
		}).Get() );

		return hr;
	}

	CWebView2Browser::CWebView2Browser( HWND hWndParent )
	{
		_hWndParent = hWndParent;

		// try to initialize WebView2 loader
		if( initWebView2Loader() )
		{
			// create webview environment and a controller
			createWebView2Environment();
		}
	}

	CWebView2Browser::~CWebView2Browser()
	{
		if( _webv2Ctl )
			_webv2Ctl->remove_AcceleratorKeyPressed( _keyPressedToken );

		if( _hWebView2Inst )
			FreeLibrary( _hWebView2Inst );
	}

	void CWebView2Browser::navigate( const std::wstring& url )
	{
		if( _webView2 )
			_webView2->Navigate( url.c_str() ); // TODO: long path names
	}

	void CWebView2Browser::updateRect( RECT *rct )
	{
		if( _webv2Ctl )
			_webv2Ctl->put_Bounds( *rct );
	}
}
