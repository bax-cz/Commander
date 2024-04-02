#pragma once

#include "WebView2.h"

#include <wrl.h>

namespace Commander
{
	using namespace Microsoft;

	class CWebView2Browser
	{
	public:
		CWebView2Browser( HWND hWndParent );
		~CWebView2Browser();

		void navigate( const std::wstring& url );
		void updateRect( RECT *rct );

		inline std::wstring& getWebView2Version() { return _webv2Ver; }

	private:
		bool initWebView2Loader();
		bool createWebView2Environment();
		HRESULT createWebView2Controller();

	private:
		std::wstring _webv2Ver;
		HMODULE _hWebView2Inst;
		HWND _hWndParent;

		WRL::ComPtr<ICoreWebView2Environment> _webv2Env;
		WRL::ComPtr<ICoreWebView2Controller> _webv2Ctl;
		WRL::ComPtr<ICoreWebView2> _webView2;

		EventRegistrationToken _keyPressedToken;

	private: // the WebView2Loader library functions' pointers
		HRESULT(__stdcall *pCompareBrowserVersions)( PCWSTR version1, PCWSTR version2, int *result );
		HRESULT(__stdcall *pCreateCoreWebView2Environment)( ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environmentCreatedHandler );
		HRESULT(__stdcall *pCreateCoreWebView2EnvironmentWithOptions)( PCWSTR browserExecutableFolder, PCWSTR userDataFolder, ICoreWebView2EnvironmentOptions *environmentOptions, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environmentCreatedHandler );
		HRESULT(__stdcall *pGetAvailableCoreWebView2BrowserVersionString)( PCWSTR browserExecutableFolder, LPWSTR *versionInfo );
	};
}
