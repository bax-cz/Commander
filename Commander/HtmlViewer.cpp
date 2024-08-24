#include "stdafx.h"

#include "Commander.h"
#include "HtmlViewer.h"
#include "IconUtils.h"
#include "MiscUtils.h"
#include "FictionBook2DataProvider.h"
#include "PalmDataProvider.h"
#include "MarkdownDataProvider.h"
#include "ViewerTypes.h"

namespace Commander
{
	void CHtmlViewer::onInit()
	{
		SendMessage( _hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_DOCASSOC ) ) );
		SendMessage( _hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( IconUtils::getStock( SIID_DOCASSOC, true ) ) );

		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		SetWindowText( _hDlg, L"Html Viewer" );

		// try to initialize WebView2 browser - sends UM_WEBVW2NOTIFY when initialized
		_upWebView2Browser = std::make_unique<CWebView2Browser>( _hDlg );
		_webView2Initialized = false;

		// if unsuccessful, use IWebBrowser2 based browser otherwise
		if( _upWebView2Browser->getWebView2Version().empty() )
		{
			_upWebBrowser = std::make_unique<CWebBrowser>( _hDlg );
			_upWebView2Browser = nullptr;
		}
	}

	bool CHtmlViewer::onClose()
	{
		show( SW_HIDE ); // hide dialog

		return true;
	}

	void CHtmlViewer::onDestroy()
	{
		_upWebBrowser = nullptr;
		_upWebView2Browser = nullptr;

		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_SMALL, 0 ) ) );
		DestroyIcon( reinterpret_cast<HICON>( SendMessage( _hDlg, WM_GETICON, ICON_BIG, 0 ) ) );
	}

	bool CHtmlViewer::viewFile( const std::wstring& dirName, const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel )
	{
		_fileName = PathUtils::addDelimiter( dirName ) + fileName;

		if( spPanel )
		{
			_spPanel = spPanel;
			return _spPanel->getDataManager()->getDataProvider()->readToFile( _fileName, spPanel, _hDlg );
		}
		else
			viewFileCore( _fileName );

		return true;
	}

	void CHtmlViewer::provideHtmlFile( const std::wstring& fileName )
	{
		switch( ViewerType::getType( fileName ) )
		{
		case ViewerType::EViewType::FMT_MOBI:
			_upHtmlDataProvider = std::make_unique<CPalmDataProvider>();
			break;
		case ViewerType::EViewType::FMT_FCBK2:
			_upHtmlDataProvider = std::make_unique<CFictionBook2DataProvider>();
			break;
		case ViewerType::EViewType::FMT_MARKD:
			_upHtmlDataProvider = std::make_unique<CMarkdownDataProvider>();
			break;
		default:
			PrintDebug("Format not supported");
			break;
		}

		if( _upHtmlDataProvider )
			_upHtmlDataProvider->readToFile( _fileName, _spPanel ? _spPanel : nullptr, _hDlg );
	}

	void CHtmlViewer::viewFileCore( const std::wstring& fileName )
	{
		_fileName = fileName;

		std::wostringstream sstrTitle;
		sstrTitle << fileName << L" - Viewer";
		
		SetWindowText( _hDlg, sstrTitle.str().c_str() );

		// when file is already a html file, just display it
		if( ViewerType::getType( fileName ) == ViewerType::EViewType::FMT_HTML )
		{
			if( _upWebView2Browser && _webView2Initialized )
				_upWebView2Browser->navigate( _fileName );

			if( _upWebBrowser )
				_upWebBrowser->navigate( _fileName );
		}
		else if( !_upHtmlDataProvider )
			provideHtmlFile( fileName );
	}

	INT_PTR CALLBACK CHtmlViewer::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			PrintDebug("msg: 0x%04X, wpar: 0x%04X, lpar: 0x%04X", message, wParam, lParam);
			//handleMenuCommands( LOWORD( wParam ) );
			break;

		case WM_KEYDOWN:
			switch( wParam )
			{
			case VK_INSERT:
				//if( _spPanel )
				//	_spPanel->markEntry( PathUtils::stripPath( _fileName ) );
				break;
			default:
				break;
			}
			break;

		case UM_WEBVW2NOTIFY:
			if( wParam != 2 )
			{
				if( wParam == 0 )
				{
					// cannot initialize WebView2 - use older IWebBrowser2 based browser
					_upWebBrowser = std::make_unique<CWebBrowser>( _hDlg );
					_upWebView2Browser = nullptr;
				}
				else
					_webView2Initialized = true;

				viewFileCore( _fileName );
			}
			else
				close(); // ESC key pressed
			break;

		case UM_READERNOTIFY:
			if( wParam == FC_ARCHDONEOK )
			{
				std::wstring result = _fileName;

				if( _spPanel )
					result = _spPanel->getDataManager()->getDataProvider()->getResult();

				if( _upHtmlDataProvider )
					result = _upHtmlDataProvider->getResult();

				viewFileCore( result );
			}
			else
			{
				SetCursor( NULL );
				std::wstringstream sstr;
				sstr << L"Error opening \"" << _fileName << L"\" file.";
				if( _upHtmlDataProvider ) sstr << L"\r\n" << _upHtmlDataProvider->getErrorMessage();
				MessageBox( _hDlg, sstr.str().c_str(), L"View Mobi", MB_OK | MB_ICONEXCLAMATION );
				close();
			}
			break;

		case WM_DROPFILES:
			//handleDropFiles( (HDROP)wParam );
			break;

		case WM_SIZE:
			{
				RECT rct = { 0 };

				rct.right = LOWORD( lParam );
				rct.bottom = HIWORD( lParam );

				if( _upWebView2Browser && _webView2Initialized )
					_upWebView2Browser->updateRect( &rct );
				if( _upWebBrowser )
					_upWebBrowser->updateRect( &rct );
			}
			break;
		}

		return (INT_PTR)false;
	}
}
