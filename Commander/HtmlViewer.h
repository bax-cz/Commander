#pragma once

#include "BaseViewer.h"
#include "WebBrowser.h"
#include "WebView2Browser.h"

namespace Commander
{
	class CHtmlViewer : public CBaseViewer
	{
	public:
		static const UINT resouceIdTemplate = IDD_IMAGEVIEWER;

	public:
		virtual bool viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel = nullptr ) override;

		virtual void onInit() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void viewFileCore( const std::wstring& fileName );
		void provideHtmlFile( const std::wstring& fileName );

	private:
		std::unique_ptr<CBaseDataProvider> _upHtmlDataProvider;
		std::unique_ptr<CWebBrowser> _upWebBrowser;
		std::unique_ptr<CWebView2Browser> _upWebView2Browser;

		bool _webView2Initialized;
		//HMENU _hMenu;
	};
}
