#include "stdafx.h"

#include "Commander.h"
#include "ControlTextBox.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CTextBox::CTextBox()
	{
	}

	CTextBox::~CTextBox()
	{
		PrintDebug("hw: 0x%08X, size: %zu", _hWnd, FCS::inst().getApp().getControls().size());
		DestroyWindow( _hWndTip );
		DestroyWindow( _hWnd );
	}


	//
	// Create Text-box control and intialize it
	//
	bool CTextBox::init( CFcPanel *const panel, CPanelTab *const panelTab )
	{
		// create text-box control at place given by rct parameter
		_hWnd = CreateWindowEx(
			WS_EX_CLIENTEDGE,
			WC_EDIT,
			L"",
			WS_CHILD | WS_VISIBLE | ES_LEFT | ES_READONLY | ES_AUTOHSCROLL,
			_rect.left, _rect.top, _rect.right - _rect.left, _rect.bottom - _rect.top,
			FCS::inst().getFcWindow(), NULL,
			FCS::inst().getFcInstance(), NULL );

		// register this window procedure
		registerWndProc( panel, panelTab, this );

		// set font
		SendMessage( _hWnd, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getGuiFont() ), TRUE );

		// create tooltip window
		_hWndTip = CreateWindowEx(
			WS_EX_TOOLWINDOW, TOOLTIPS_CLASS, NULL,
			WS_POPUP | WS_CLIPSIBLINGS | TTS_NOPREFIX,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			FCS::inst().getFcWindow(), NULL,
			FCS::inst().getFcInstance(), NULL );

		// associate tooltip with edit control
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = FCS::inst().getFcWindow();
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR)_hWnd;

		SendMessage( _hWndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo );

		return _hWnd != NULL;
	}

	//
	// Update text in Text-box
	//
	void CTextBox::updateText( const std::wstring& text )
	{
		if( _controlUserData != text )
		{
			LPARAM lParam = reinterpret_cast<LPARAM>( text.c_str() );
			SendMessage( _hWnd, WM_SETTEXT, 0, lParam );

			_controlUserData = text;
		}
	}

	//
	// Update tooltip associated with Text-box
	//
	void CTextBox::updateToolTip( const std::wstring& tip )
	{
		// update tooltip text
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = FCS::inst().getFcWindow();
		toolInfo.uId = (UINT_PTR)_hWnd;
		toolInfo.lpszText = (TCHAR*)tip.c_str();

		SendMessage( _hWndTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&toolInfo );
	}

	//
	// Subclassed window procedure
	//
	LRESULT CALLBACK CTextBox::wndProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_CTLCOLORSTATIC:
		{
			bool overMaxPath = _pParentPanel->getActiveTab()->getDataManager()->isOverMaxPath();
			
			SetTextColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_TEXT );

			if( FCS::inst().getApp().isActivated() && FCS::inst().getApp().getActivePanelId() == _pParentPanel->id() )
			{
				SetBkColor( reinterpret_cast<HDC>( wParam ), overMaxPath ? FC_COLOR_DIRBOXACTIVEWARN : FC_COLOR_DIRBOXACTIVE );
				return reinterpret_cast<LRESULT>( FCS::inst().getApp().getActiveBrush( overMaxPath ) );
			}
			else
			{
				SetBkColor( reinterpret_cast<HDC>( wParam ), overMaxPath ? FC_COLOR_DIRBOXBKGNDWARN : FC_COLOR_DIRBOXBKGND );
				return reinterpret_cast<LRESULT>( FCS::inst().getApp().getBkgndBrush( overMaxPath ) );
			}
			break;
		}
		case WM_COMMAND:
			if( HIWORD( wParam ) == EN_SETFOCUS )
			{
				// disable cursor in edit boxes
				HideCaret( reinterpret_cast<HWND>( lParam ) );
			}
			break;

		case WM_LBUTTONDBLCLK:
			_pParentPanel->getActiveTab()->processCommand( EFcCommand::ChangeDir );
			return FALSE;

		case WM_SETFOCUS:
			_pParentPanel->getActiveTab()->gotFocus();
			break;

		default:
			break;
		}

		return CallWindowProc( _wndProcDefault, _hWnd, message, wParam, lParam );
	}
}
