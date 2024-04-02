#include "stdafx.h"

#include "Commander.h"
#include "ControlStaticText.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CStaticText::CStaticText()
	{
	}

	CStaticText::~CStaticText()
	{
		PrintDebug("hw: 0x%08X, size: %zu", _hWnd, FCS::inst().getApp().getControls().size());
		DestroyWindow( _hWndTip );
		DestroyWindow( _hWnd );
	}


	//
	// Create Static-text control and intialize it
	//
	bool CStaticText::init( CFcPanel *const panel, CPanelTab *const panelTab )
	{
		// create Static-text control at place given by rct parameter
		_hWnd = CreateWindowEx(
			0,
			WC_STATIC,
			L"",
			WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_NOTIFY | SS_WORDELLIPSIS,
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

		// associate tooltip with static control (SS_NOTIFY style needed)
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = FCS::inst().getFcWindow();
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR)_hWnd;

		SendMessage( _hWndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo );

		return _hWnd != NULL;
	}

	//
	// Update text in Static-text
	//
	void CStaticText::updateText( const std::wstring& properties )
	{
		if( _controlUserData != properties )
		{
			// set static text
			LPARAM lParam = reinterpret_cast<LPARAM>( properties.c_str() );
			SendMessage( _hWnd, WM_SETTEXT, 0, lParam );

			_controlUserData = properties;
		}
	}

	//
	// Update tooltip associated with Static-text
	//
	void CStaticText::updateToolTip( const std::wstring& tip )
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
	LRESULT CALLBACK CStaticText::wndProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_SETTEXT:
			break;

		default:
			break;
		}

		return CallWindowProc( _wndProcDefault, _hWnd, message, wParam, lParam );
	}
}
