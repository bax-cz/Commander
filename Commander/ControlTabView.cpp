#include "stdafx.h"

#include "Commander.h"
#include "ControlTabView.h"
#include "TabViewUtils.h"
#include "PathUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CTabView::CTabView()
	{
		_idTab = 0;
	}

	CTabView::~CTabView()
	{
		//PrintDebug("hw: 0x%08X, size: %zu", _hWnd, FCS::inst().getApp().getControls().size());
		DestroyWindow( _hWnd );
	}


	//
	// Create Tab-view control and intialize it
	//
	bool CTabView::init( CFcPanel *const panel, CPanelTab *const panelTab )
	{
		// create Static-text control at place given by rct parameter
		_hWnd = CreateWindowEx(
			0,
			WC_TABCONTROL,
			L"",
			WS_CHILD | WS_VISIBLE | TCS_SINGLELINE | TCS_FOCUSNEVER | TCS_BOTTOM | TCS_TOOLTIPS,
			_rect.left, _rect.top, _rect.right - _rect.left, _rect.bottom - _rect.top,
			FCS::inst().getFcWindow(),
			NULL,
			FCS::inst().getFcInstance(),
			NULL );

		// store parent panel and tab pointers
		_pParentPanel = panel;

		// register this window procedure
		FCS::inst().getApp().getControls()[_hWnd] = this;

		// subclass edit control and store its default window procedure
		SetWindowLongPtr( _hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( this ) );
		LONG_PTR defProc = SetWindowLongPtr( _hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( _wndProcSubclass ) );
		_wndProcDefault = reinterpret_cast<WNDPROC>( defProc );

		// set font
		SendMessage( _hWnd, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getGuiFont() ), TRUE );

		// force displaying '&' symbol by setting TTS_NOPREFIX style to tooltip window
		_hWndTip = TabCtrl_GetToolTips( _hWnd );
		SetWindowLongPtr( _hWndTip, GWL_STYLE, GetWindowLongPtr( _hWndTip, GWL_STYLE ) | TTS_NOPREFIX );

		return _hWnd != NULL;
	}

	//
	// Remove all tabs
	//
	void CTabView::cleanUp()
	{
		TabCtrl_DeleteAllItems( _hWnd );

		for( auto it = _tabs.begin(); it != _tabs.end(); )
		{
			it = _tabs.erase( it );
		}

		_spActive = nullptr;
	}

	//
	// Create new tab with given label
	//
	void CTabView::createTab( const std::wstring& path, const std::wstring& itemName, bool active )
	{
		TabUtils::insertItem( _hWnd, PathUtils::getLabelFromPath( path ) );

		auto spTab = std::make_shared<CPanelTab>( _idTab++ );

		if( _spActive && active )
			_spActive->show( SW_HIDE );
		else if( !_spActive )
			_spActive = spTab;

		spTab->init( _pParentPanel, path, itemName );
		_tabs.push_back( spTab );

		if( getCount() > 1 )
		{
			show();
			_pParentPanel->setActiveTab( active ? getCount() - 1 : getCurSel() );
		}
	}

	//
	// Remove tab at given index
	//
	void CTabView::removeTab( int index )
	{
		if( getCount() > 1 && index < getCount() )
		{
			auto curIndex = getCurSel();

			if( index == -1 )
				index = curIndex;

			TabUtils::removeItem( _hWnd, index );

			_tabs[index]->close();
			_tabs.erase( _tabs.begin() + index );

			// we are removing active tab
			if( index == curIndex )
			{
				if( index >= getCount() )
					index = getCount() - 1;

				_pParentPanel->setActiveTab( index );
			}

			// hide tabs in UI when there is only one tab
			if( getCount() == 1 )
			{
				show( SW_HIDE );
				_pParentPanel->setActiveTab( 0 );
			}
		}
	}

	//
	// Set active tab
	//
	CPanelTab *CTabView::setActive( int index )
	{
		if( getCurSel() != index )
			setCurSel( index );

		if( _spActive != _tabs[index] )
		{
			_spActive->show( SW_HIDE );
			_spActive = _tabs[index];
		}

		return _spActive.get();
	}

	//
	// Set tab label
	//
	void CTabView::setLabel( const std::wstring& label, DWORD tabId )
	{
		for( size_t i = 0; i < _tabs.size(); ++i )
		{
			if( _tabs[i]->id() == tabId )
			{
				// prevent windows bug - https://microsoft.public.vc.mfc.narkive.com/9seKHEbF/ctabctrl-bug-using-tcs-bottom-style
				auto tmp = StringUtils::removeAll( label, L'&' );
				TabUtils::setItemLabel( _hWnd, static_cast<int>( i ), tmp );
				break;
			}
		}
	}

	//
	// On WM_NOTIFY message handler
	//
	LRESULT CTabView::onNotifyMessage( WPARAM wParam, LPARAM lParam )
	{
		switch( reinterpret_cast<LPNMHDR>( lParam )->code )
		{
		case TTN_GETDISPINFO:
			{
				auto di = reinterpret_cast<LPNMTTDISPINFO>( lParam );
				di->lpszText = const_cast<LPWSTR>(
					&_tabs[(int)di->hdr.idFrom]->getDataManager()->getCurrentDirectory()[0] );
			}
			break;

		case TCN_SELCHANGING:
			_pParentPanel->gotFocus();
			break;

		case TCN_SELCHANGE:
			_pParentPanel->setActiveTab( getCurSel() );
			break;
		}

		return 0;
	}

	//
	// Subclassed window procedure
	//
	LRESULT CALLBACK CTabView::wndProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_NOTIFY:
			return onNotifyMessage( wParam, lParam );

		case WM_MBUTTONDOWN:
			removeTab( TabUtils::findItem( _hWnd, POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } ) );
			return 0;

		case WM_SETFOCUS:
			_pParentPanel->gotFocus();
			break;

		default:
			//PrintDebug("wpar:0x%08X, lpar:0x%08X", wParam, lParam);
			break;
		}

		if( _wndProcDefault )
			return CallWindowProc( _wndProcDefault, _hWnd, message, wParam, lParam );

		return (LRESULT)0;
	}
}
