#include "stdafx.h"

#include <MsHTML.h>

#include "Commander.h"
#include "WebBrowser.h"

#define FINDRANGE_DEFAULT   0x00000000 // Match partial words.
#define FINDRANGE_REVERSE   0x00000001 // Match in reverse.
#define FINDRANGE_WORDS     0x00000002 // Match whole words only.
#define FINDRANGE_CASE      0x00000004 // Match case.
#define FINDRANGE_BYTES     0x00020000 // Match bytes.
#define FINDRANGE_DIACRITIC 0x20000000 // Match diacritical marks.
#define FINDRANGE_KASHIDA   0x40000000 // Match Kashida character.
#define FINDRANGE_ALEFHAMZA 0x80000000 // Match AlefHamza character.

namespace Commander
{
	CWebBrowser::CWebBrowser( HWND hWndParent )
		: _hWndParent( hWndParent )
		, _hWndControl( nullptr )
		, _pWebBrowser( nullptr )
		, _pOleObj( nullptr )
		, _pOleInPlaceObj( nullptr )
		, _wbProcCallback( nullptr )
		, _pFindDlg( nullptr )
		, _dwCookie( 0 )
		, _cRef( 0 )
	{
		GetClientRect( _hWndParent, &_rctObj );

		HRESULT hr = OleCreate( CLSID_WebBrowser, IID_IOleObject, OLERENDER_DRAW, 0, this, this, (void**)&_pOleObj );
		if( FAILED( hr ) )
		{
			PrintDebug("Cannot create OLE Object CLSID_WebBrowser");
		}

		hr = _pOleObj->SetClientSite( this );
		hr = OleSetContainedObject( _pOleObj, TRUE );

		// in-place activate the WebBrowser control
		hr = _pOleObj->DoVerb( OLEIVERB_INPLACEACTIVATE, NULL, this, -1, _hWndParent, &_rctObj );
		if( FAILED( hr ) )
		{
			PrintDebug("_pOleObj->DoVerb() failed");
		}

		// query IWebBrowser2 interface
		hr = _pOleObj->QueryInterface( &_pWebBrowser );
		if( FAILED( hr ) )
		{
			PrintDebug("_pOleObj->QueryInterface( &_pWebBrowser ) failed");
		}

		// don't show error dialogs
		_pWebBrowser->put_Silent( VARIANT_TRUE );
		_pWebBrowser->put_Offline( VARIANT_TRUE );

	//	CComVariant varToolbar( (short)0x000A ), varShow( true );
	//	hr = _pWebBrowser->ShowBrowserBar( &varToolbar, &varShow, NULL );

		// register container to intercept WebBrowser events
		hr = AtlAdvise( _pWebBrowser, (IDispatch*)this, DIID_DWebBrowserEvents2, &_dwCookie );

		_findParams = { false };
	}

	CWebBrowser::~CWebBrowser()
	{
		AtlUnadvise( (IDispatch*)this, DIID_DWebBrowserEvents2, _dwCookie );

		_pWebBrowser->Stop();
		_pWebBrowser->ExecWB( OLECMDID_CLOSE, OLECMDEXECOPT_DONTPROMPTUSER, nullptr, nullptr );
		_pWebBrowser->put_Visible( VARIANT_FALSE );
		_pWebBrowser->Release();

		_pOleInPlaceObj->InPlaceDeactivate();
		_pOleInPlaceObj->Release();

		_pOleObj->DoVerb( OLEIVERB_HIDE, nullptr, this, 0, _hWndParent, nullptr );
		_pOleObj->Close( OLECLOSE_NOSAVE );
		OleSetContainedObject( _pOleObj, FALSE );
		_pOleObj->SetClientSite( nullptr );
		CoDisconnectObject( _pOleObj, 0 );
		_pOleObj->Release();
	}

	/*
	From: https://stackoverflow.com/a/14652605
	I had many problems with "access violation" when closing webbrowser control, these are the steps that worked for me:

	- Unadvise any previously advised events (DWebBrowserEvents2 in my case).
	- If you've attached click events unattach them like this: _variant_t v; v.vt = VT_DISPATCH; v.pdispVal = 0; IHTMLDocument2->put_onclick(v);
	- IWebBrowser2->Stop()
	- IWebBrowser2->ExecWB(OLECMDID_CLOSE, OLECMDEXECOPT_DONTPROMPTUSER, 0, 0) - when closing browser window through window.external.CloseWindow() I had unhandled exceptions and OLECMDID_CLOSE fixed it.
	- IWebBrowser2->put_Visible(VARIANT_FALSE)
	- IWebBrowser2->Release()
	- IOleInPlaceObject->InPlaceDeactivate()
	- IOleInPlaceObject->Release()
	- IOleObject->DoVerb(OLEIVERB_HIDE, NULL, IOleClientSite, 0, windowHandle_, NULL)
	- IOleObject->Close(OLECLOSE_NOSAVE)
	- OleSetContainedObject(IOleObject, FALSE)
	- IOleObject->SetClientSite(NULL)
	- CoDisconnectObject(IOleObject, 0)
	- IOleObject->Release()

	IWebBrowser2->Quit() should not be called for WebBrowser control (CLSID_WebBrowser), it is intended only for Internet Explorer object (CLSID_InternetExplorer).
	*/

	void CWebBrowser::navigate( const std::wstring& url )
	{
		// TODO: long paths
		CComBSTR bstrUrl( &url[0] );
		//CComVariant flags( 0x02u ); // navNoHistory

		_pWebBrowser->Navigate( bstrUrl, NULL, NULL, NULL, NULL );
	}

	void CWebBrowser::hiliteText( const std::wstring& text, const CFindText::CParams& params )
	{
		_textToFind = text;
		_findParams = params;

		searchText();
	}

	void CWebBrowser::hiliteTextCore( bool reverse )
	{
		if( !_textToFind.empty() )
		{
			if( _pWebBrowser )
			{
				SetCursor( LoadCursor( NULL, IDC_WAIT ) );

				CComPtr<IDispatch> disp;
				HRESULT hr = _pWebBrowser->get_Document( &disp );

				VARIANT_BOOL success = VARIANT_FALSE;

				if( SUCCEEDED( hr ) && disp )
				{
					CComQIPtr<IHTMLDocument2> doc( disp );

					if( doc )
					{
						/*CComPtr<IHTMLElement> body;
						hr = doc->get_body( &body );

						if( SUCCEEDED( hr ) && body )
						{
							CComBSTR innerText;
							hr = body->get_innerText( &innerText );
						}*/

						CComPtr<IHTMLSelectionObject> sel;
						hr = doc->get_selection( &sel );

						CComBSTR bstr;
						hr = sel->get_type( &bstr );

						CComPtr<IHTMLTxtRange> range;
						hr = sel->createRange( (IDispatch**)&range );

						hr = range->collapse();

						// construct flags
						long flags = FINDRANGE_DEFAULT;

						if( _findParams.matchCase  ) flags |= FINDRANGE_CASE;
						if( _findParams.wholeWords ) flags |= FINDRANGE_WORDS;
						if( reverse                ) flags |= FINDRANGE_REVERSE;

						CComBSTR str( &_textToFind[0] );
						hr = range->findText( str, static_cast<long>( _textToFind.length() ), flags, &success );

						if( success == VARIANT_TRUE )
							hr = range->select();
					}
				}

				SetCursor( NULL );

				if( success == VARIANT_FALSE )
				{
					std::wstringstream sstr;
					sstr << L"Cannot find \"" << _textToFind << L"\".";
					_pFindDlg->showMessageBox( sstr.str() );
				}
			}
		}
		else
			findTextDialog();
	}

	void CWebBrowser::searchText()
	{
		if( _findParams.hexMode )
		{
			std::string strOut; // TODO
			if( !StringUtils::convertHex( _textToFind, strOut ) )
			{
				std::wstringstream sstr;
				sstr << L"Error converting hexadecimal string.";

				MessageBox( _hWndParent, sstr.str().c_str(), L"Conversion Failed", MB_ICONEXCLAMATION | MB_OK );

				return;
			}
		}
		else if( _findParams.regexMode )
		{
			// TODO

			/*auto flags = _findParams.matchCase ? std::regex_constants::ECMAScript : std::regex_constants::icase;

			try {
				_regexA = std::make_unique<std::regex>( StringUtils::convert2A( _textToFind ), flags );
				_regexW = std::make_unique<std::wregex>( _textToFind, flags );
			}
			catch( const std::regex_error& e )
			{
				std::wstringstream sstr;
				sstr << L"Expression: \"" << _textToFind << L"\"\r\n";
				sstr << StringUtils::convert2W( e.what() );

				MessageBox( _hDlg, sstr.str().c_str(), L"Regular Expressions Error", MB_ICONEXCLAMATION | MB_OK );

				return;
			}*/
		}

		hiliteTextCore( _findParams.reverse );
	}

	void CWebBrowser::setEncoding( const std::wstring& encoding )
	{
		if( _pWebBrowser )
		{
			CComPtr<IDispatch> disp;
			HRESULT hr = _pWebBrowser->get_Document( &disp );

			if( SUCCEEDED( hr ) && disp )
			{
				CComQIPtr<IHTMLDocument2> doc( disp );

				if( doc )
				{
					CComBSTR charset( &encoding[0] );
					hr = doc->put_charset( charset );
					hr = _pWebBrowser->Refresh();
				}
			}
		}
	}

	std::wstring CWebBrowser::getEncoding()
	{
		std::wstring encoding;

		if( _pWebBrowser )
		{
			CComPtr<IDispatch> disp;
			HRESULT hr = _pWebBrowser->get_Document( &disp );

			if( SUCCEEDED( hr ) && disp )
			{
				CComQIPtr<IHTMLDocument2> doc( disp );

				if( doc )
				{
					CComBSTR charset;
					hr = doc->get_charset( &charset );

					encoding = charset;
				}
			}
		}

		return encoding;
	}

	void CWebBrowser::findTextDialog()
	{
		std::wstring text = _textToFind;

		if( !_findParams.hexMode && !_findParams.regexMode )
		{
			CComPtr<IDispatch> disp;
			HRESULT hr = _pWebBrowser->get_Document( &disp );

			if( SUCCEEDED( hr ) && disp )
			{
				CComQIPtr<IHTMLDocument2> doc( disp );

				if( doc )
				{
					CComPtr<IHTMLSelectionObject> sel;
					hr = doc->get_selection( &sel );

					CComBSTR bstrType;
					hr = sel->get_type( &bstrType );
					if( SUCCEEDED( hr ) && bstrType == L"Text" )
					{
						CComPtr<IHTMLTxtRange> range;
						hr = sel->createRange( (IDispatch**)&range );

						CComBSTR bstrText;
						if( SUCCEEDED( hr ) && range )
							hr = range->get_text( &bstrText );

						if( SUCCEEDED( hr ) && bstrText )
							text = bstrText;
					}
				}
			}
		}

		_pFindDlg->updateText( text.empty() ? _textToFind : text );
		_pFindDlg->updateParams( _findParams );
		_pFindDlg->show();
	}

	/*void CWebBrowser::handleKeyboard( WPARAM key )
	{
		switch( key )
		{
		case VK_SPACE:
			break;
		case VK_BACK:
			break;
		//case VK_HOME:
		//	break;
		//case VK_END:
		//	break;
		//case VK_PRIOR:
		//	break;
		//case VK_NEXT:
		//	break;
		default:
			break;
		}
	}*/

	HWND CWebBrowser::findControlWindow()
	{
		_ASSERTE( _pOleInPlaceObj );

		HWND hWnd = nullptr;
		_pOleInPlaceObj->GetWindow( &hWnd );

		// get child HWND of "Internet Explorer_Server" class name
		if( IsWindow( hWnd ) )
		{
			HWND hWndChild = FindWindowEx( hWnd, NULL, L"Internet Explorer_Server", NULL );

			if( IsWindow( hWndChild ) )
			{
				return hWndChild;
			}
			else
			{
				hWndChild = GetFirstChild( hWnd );

				if( IsWindow( hWndChild ) )
				{
					hWndChild = GetFirstChild( hWndChild );

					if( IsWindow( hWndChild ) )
						return hWndChild;
				}
			}
		}

		return hWnd;
	}

	bool CWebBrowser::onBeforeNavigate()
	{
		// this is sent before navigation to give a chance to abort

		return true;
	}

	void CWebBrowser::onNavigateComplete()
	{
		// UIActivate new document
	}

	void CWebBrowser::onDocumentComplete()
	{
		// new document goes ReadyState_Complete

		// set utf-8 encoding
		if( getEncoding() != L"utf-8" )
			setEncoding( L"utf-8" );

		_hWndControl = findControlWindow();

		// subclass the web browser control window
		SetWindowSubclass( _hWndControl, wndProcWebBrowserSubclass, 0, reinterpret_cast<DWORD_PTR>( this ) );

		// create find dialog instance
		_pFindDlg = CBaseDialog::createModeless<CFindText>( _hWndControl );

		SetFocus( _hWndControl );
	}

	bool CWebBrowser::webBrowserProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
	//	PrintDebug("msg: 0x%04X, wpar: 0x%04X, lpar: 0x%04X", message, wParam, lParam);

		switch( message )
		{
		case WM_KEYDOWN:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				switch( wParam )
				{
				case 0x41: // 'A' - select all
					_pWebBrowser->ExecWB( OLECMDID_SELECTALL, OLECMDEXECOPT_DONTPROMPTUSER, nullptr, nullptr );
					break;
				case 0x43: // 'C' - copy to clipboard
					_pWebBrowser->ExecWB( OLECMDID_COPY, OLECMDEXECOPT_DONTPROMPTUSER, nullptr, nullptr );
					break;
				case 0x46: // 'F' - find text
					//findTextDialog();
					_pWebBrowser->ExecWB( OLECMDID_FIND, OLECMDEXECOPT_DODEFAULT, nullptr, nullptr );
					break;
				}
			}
			else if( wParam == VK_F3 )
			{
				// invert direction when holding shift key
				bool shift = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
				hiliteTextCore( ( _findParams.reverse && !shift ) || ( !_findParams.reverse && shift ) );
			}
			else if( wParam == VK_BACK )
				goBack();
			break;

		case UM_REPORTMSGBOX:
			_pFindDlg->messageBoxClosed();
			return true;

		case UM_FINDLGNOTIFY:
			if( wParam == IDE_FINDTEXT_TEXTFIND ) // from CFindText dialog
			{
				switch( lParam )
				{
				case 0: // CFindText dialog is being destroyed
					_pFindDlg = nullptr;
					break;
				case 5: // search previous
				case 4: // search next
					hiliteTextCore( lParam == 4 ? _findParams.reverse : !_findParams.reverse );
					break;
				case 1: // perform search text
					_textToFind = _pFindDlg->result();
					_findParams = _pFindDlg->getParams();

					searchText();
					break;
				default:
					PrintDebug("unknown command: %d", lParam);
					break;
				}
			}
			else
			{
				SetCursor( NULL );
				if( wParam == 0 ) {
					std::wstringstream sstr;
					sstr << L"Cannot find \"" << _textToFind << L"\".";
					_pFindDlg->showMessageBox( sstr.str() );
				}
			}
			return true;
		}

		if( _wbProcCallback )
			_wbProcCallback( hWnd, message, wParam, lParam );

		return false;
	}
}
