#include "stdafx.h"

#include "Commander.h"
#include "Control.h"


namespace Commander
{
	//
	// Constructor
	//
	CControl::CControl()
		: _pParentTab( nullptr )
		, _pParentPanel( nullptr )
		, _wndProcDefault( nullptr )
		, _hWnd( nullptr )
		, _hWndTip( nullptr )
	{
		ZeroMemory( &_rect, sizeof( RECT ) );
	}

	//
	// Register control's window procedure - store the old one
	//
	void CControl::registerWndProc( CFcPanel *const panel, CPanelTab *const panelTab, CControl *control )
	{
		// store pointer to parent panel and its tab
		_pParentPanel = panel;
		_pParentTab = panelTab;

		LONG_PTR defProc = SetWindowLongPtr( _hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( controlsWndProcMain ) );
		_wndProcDefault = reinterpret_cast<WNDPROC>( defProc );

		FCS::inst().getApp().getControls()[_hWnd] = control;
	}

	//
	// Update control's window according to main window
	//
	void CControl::updateLayout()
	{
		SetWindowPos(
			_hWnd,
			0,
			_rect.left, _rect.top, _rect.right - _rect.left, _rect.bottom - _rect.top,
			SWP_NOZORDER | SWP_NOACTIVATE );

		UpdateWindow( _hWnd );
	}

	//
	// Show/hide control's window
	//
	void CControl::show( int cmdShow )
	{
		ShowWindow( _hWnd, cmdShow );
	}

	//
	// Set control's window focus
	//
	void CControl::focus()
	{
		auto cur = GetFocus();
		auto foc = SetFocus( _hWnd );
		PrintDebug( "hw: 0x%08X, current: 0x%08X, focus: 0x%08X", _hWnd, cur, foc );
	}

	//
	// Global delegating main window's controls procedure
	//
	LRESULT CALLBACK CControl::controlsWndProcMain( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDA_DRIVELEFT:
				FCS::inst().getApp().getPanelLeft().showDrivesList();
				break;

			case IDA_DRIVERIGHT:
				FCS::inst().getApp().getPanelRight().showDrivesList();
				break;

			case IDA_PREVTAB:
				FCS::inst().getApp().getActivePanel().switchTab( EFcCommand::PrevTab );
				break;

			case IDA_NEXTTAB:
				FCS::inst().getApp().getActivePanel().switchTab( EFcCommand::NextTab );
				break;
			}
			break;

		case WM_NOTIFY:
			if( reinterpret_cast<LPNMHDR>( lParam )->hwndFrom == FCS::inst().getApp().getToolbar() &&
				reinterpret_cast<LPNMHDR>( lParam )->code == TBN_DROPDOWN )
			{
				return FCS::inst().getApp().getActivePanel().handleHistory( reinterpret_cast<LPNMTOOLBAR>( lParam ) );
			}
		}

		auto it = FCS::inst().getApp().getControls().find( hWnd );
		if( it != FCS::inst().getApp().getControls().end() )
		{
			if( message == WM_NCDESTROY )
				FCS::inst().getApp().getControls().erase( it ); // remove this handle from map
			else
				return it->second->wndProc( message, wParam, lParam );
		}

		return DefWindowProc( hWnd, message, wParam, lParam );
	}
}
