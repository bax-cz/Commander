#include "stdafx.h"

#include "Commander.h"
#include "SharedDirectories.h"

#include "IconUtils.h"
#include "ListViewUtils.h"
#include "MiscUtils.h"
#include "NetworkUtils.h"
#include "ShellUtils.h"
#include "StringUtils.h"

namespace Commander
{
	void CSharedDirectories::onInit()
	{
		MiscUtils::centerOnWindow( FCS::inst().getFcWindow(), _hDlg );

		LvUtils::setExtStyle( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );
		LvUtils::setImageList( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );

		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), L"Share Name" );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), L"Shared Path", 180 );
		LvUtils::addColumn( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), L"Comment", 140 );

		EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );

		show(); // show dialog

		_worker.init( [this]{ return _getSharedDirsCore(); }, _hDlg, UM_NEIGHBORDONE );
		_worker.start();
	}

	bool CSharedDirectories::onOk()
	{
		focusSelectedDirectory();

		return false;
	}

	bool CSharedDirectories::onClose()
	{
		show( SW_HIDE ); // hide dialog

		_worker.stop();

		return true;
	}

	bool CSharedDirectories::_getSharedDirsCore()
	{
	//	NetUtils::getSharedFolders();
		return true;
	}

	void CSharedDirectories::addItems()
	{
		auto items = NetUtils::getSharedFoldersFull();
		int i = 0;

		for( auto& it = items.begin(); it != items.end(); ++it )
		{
			auto iconIdx = IconUtils::getFromPathIndex( std::get<1>( *it ).c_str() );

			LvUtils::addItem( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), std::get<0>( *it ).c_str(), i, iconIdx );
			LvUtils::addSubItemText( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), i, 1, std::get<1>( *it ).c_str() );
			LvUtils::addSubItemText( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), i++, 2, std::get<2>( *it ).c_str() );
		}
	}

	void CSharedDirectories::selectionChanged( LPNMLISTVIEW lvData )
	{
		auto path = LvUtils::getItemText( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), lvData->iItem, 1 );

		if( !path.empty() && lvData->uNewState == ( LVIS_FOCUSED | LVIS_SELECTED ) )
			EnableWindow( GetDlgItem( _hDlg, IDOK ), TRUE );
		else
			EnableWindow( GetDlgItem( _hDlg, IDOK ), FALSE );
	}

	void CSharedDirectories::focusSelectedDirectory()
	{
		auto itemIdx = LvUtils::getSelectedItem( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ) );
		auto path = LvUtils::getItemText( GetDlgItem( _hDlg, IDC_LISTSHAREDDIRS ), itemIdx, 1 );

		if( !path.empty() )
			FCS::inst().getApp().getActivePanel().getActiveTab()->changeDirectory( path );
	}

	bool CSharedDirectories::onNotifyMessage( LPNMHDR notifyHeader )
	{
		switch( notifyHeader->code )
		{
		case NM_RETURN:
		case NM_DBLCLK:
			focusSelectedDirectory();
			break;

		case LVN_KEYDOWN:
			//onHandleVirtualKeys( reinterpret_cast<LPNMLVKEYDOWN>( notifyHeader ) );
			break;

		case LVN_ITEMCHANGED:
			selectionChanged( reinterpret_cast<LPNMLISTVIEW>( notifyHeader ) );
			break;

		case LVN_COLUMNCLICK:
			//sortItems( reinterpret_cast<LPNMLISTVIEW>( notifyHeader )->iSubItem );
			break;
		}

		return false;
	}


	INT_PTR CALLBACK CSharedDirectories::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case UM_NEIGHBORDONE:
			addItems();
			break;

		case WM_NOTIFY:
			return onNotifyMessage( reinterpret_cast<LPNMHDR>( lParam ) );

		case WM_CONTEXTMENU:
			//onContextMenu( reinterpret_cast<HWND>( wParam ), POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } );
			break;

		case WM_DRAWITEM:
		case WM_MEASUREITEM:
		case WM_MENUCHAR:
		case WM_INITMENUPOPUP:
		case WM_MENUSELECT:
		{
			/*auto ret = _contextMenu.handleMessages( _hDlg, message, wParam, lParam );
			if( ret ) return ret;
			else*/ break;
		}
		}

		return (INT_PTR)false;
	}
}
