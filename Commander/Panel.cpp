#include "stdafx.h"

#include <objbase.h>
#include <shlobj.h>

#include "Commander.h"
#include "Panel.h"

#include "MiscUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CFcPanel::CFcPanel( BYTE id )
	{
		_id = id;
	}

	CFcPanel::~CFcPanel()
	{
	}

	// Tab-view properties
	void CFcPanel::updateLayoutTabs()
	{
		RECT& rct = _tabView.getRect();

		rct.top = _rctPanel.bottom - 38;
		rct.left = _rctPanel.left;
		rct.bottom = _rctPanel.bottom - 19;
		rct.right = _rctPanel.right;

		_tabView.updateLayout();
	}

	// Combo-box panel
	void CFcPanel::updateLayoutCurDrive()
	{
		RECT& rct = _toolbarDrives.getRect();

		rct.top = _rctPanel.top + 27;
		rct.left = _rctPanel.left;
		rct.bottom = rct.top + 20;
		rct.right = _rctPanel.left + 32;

		_toolbarDrives.updateLayout();
	}

	// Text-box current directory
	void CFcPanel::updateLayoutCurDir()
	{
		RECT& rct = _textBoxDirectory.getRect();

		rct.top = _rctPanel.top + 27;
		rct.left = _rctPanel.left + 33;
		rct.bottom = rct.top + 20;
		rct.right = _rctPanel.right - 50;

		_textBoxDirectory.updateLayout();
	}

	// Text-box free space
	void CFcPanel::updateLayoutFreeSpace()
	{
		RECT& rct = _textBoxFreeSpace.getRect();

		rct.top = _rctPanel.top + 27;
		rct.left = _rctPanel.right - 49;
		rct.bottom = rct.top + 20;
		rct.right = _rctPanel.right;

		_textBoxFreeSpace.updateLayout();
	}

	// Static text properties
	void CFcPanel::updateLayoutProperties()
	{
		RECT& rct = _staticTextProperties.getRect();

		rct.top = _rctPanel.bottom - 18;
		rct.left = _rctPanel.left;
		rct.bottom = _rctPanel.bottom;
		rct.right = _rctPanel.right;

		_staticTextProperties.updateLayout();
	}

	void CFcPanel::init( const std::wstring& path )
	{
		_toolbarDrives.init( this );
		_textBoxDirectory.init( this );
		_textBoxFreeSpace.init( this );
		_staticTextProperties.init( this );
		_tabView.init( this );
		_tabView.createTab( path, L"" );
		_tabView.show( SW_HIDE );

		if( _id == FCS::inst().getApp().getActivePanelId() )
			setPanelFocus();
	}

	void CFcPanel::setActiveTab( int index )
	{
		auto activeTab = _tabView.setActive( index );
		activeTab->updateLayout( _rctPanel );
		activeTab->selectionChanged();
		activeTab->show();
		updateDirectoryBox( activeTab->getDataManager()->getCurrentDirectory(), activeTab->id() );
		updateDrivesBar( activeTab->getDataManager()->getRootPath(), activeTab->id() );
		updateFreeSpaceBox( activeTab->getDataManager()->getFreeSpace(), activeTab->id() );
		activeTab->setPanelFocus();
	}

	void CFcPanel::switchTab( EFcCommand cmd )
	{
		auto count = _tabView.getCount();
		auto index = _tabView.getCurSel();

		if( count > 1 )
		{
			switch( cmd )
			{
			case EFcCommand::PrevTab:
				index = index ? index - 1 : count - 1;
				break;
			case EFcCommand::NextTab:
				index = ( index + 1 < count ) ? index + 1 : 0;
				break;
			}

			setActiveTab( index );
		}
	}

	//
	// Create new panel's tab
	//
	void CFcPanel::createTab( const std::wstring& path, const std::wstring& itemName, bool active )
	{
		_tabView.createTab( path, itemName, active );
	}

	//
	// Remove panel's tab at given index
	//
	void CFcPanel::removeTab( int index )
	{
		// TODO: add canBeRemoved function
		_tabView.removeTab( index );
	}

	//
	// Handle panel's tab history - toolbar's dropdown arrow clicked
	//
	LRESULT CFcPanel::handleHistory( LPNMTOOLBAR tbNotifMsg )
	{
		// get backward/forward history list
		bool goBack = tbNotifMsg->iItem == IDM_GOTO_BACK;
		auto list = getActiveTab()->getHistoryList( goBack );

		// construct popup menu with history elements from the list
		auto hMenu = CreatePopupMenu();

		for( size_t i = 0; i < list.size(); ++i )
			AppendMenu( hMenu, MF_STRING, i + 1, list[i].c_str() );

		// get screen coordinates for popup menu
		POINT pt{ tbNotifMsg->rcButton.left, tbNotifMsg->rcButton.bottom };
		ClientToScreen( FCS::inst().getFcWindow(), &pt );

		// show the menu and wait for input
		auto cmd = TrackPopupMenu( hMenu,
			TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
			pt.x, pt.y, 0, FCS::inst().getFcWindow(), NULL );

		DestroyMenu( hMenu );

		// navigate through history
		if( cmd )
			getActiveTab()->navigateThroughHistory(
				goBack ? EFcCommand::HistoryBackward : EFcCommand::HistoryForward, cmd );

		return TBDDRET_DEFAULT;
	}

	//
	// Set panel control focus
	//
	void CFcPanel::setPanelFocus( bool activate )
	{
		if( activate )
		{
			getActiveTab()->setPanelFocus();
		}
		else
		{
			RedrawWindow( _textBoxDirectory.getHwnd(), NULL, NULL, RDW_INVALIDATE );
			RedrawWindow( _textBoxFreeSpace.getHwnd(), NULL, NULL, RDW_INVALIDATE );
		}
	}

	//
	// Show combo-box's drop down list
	//
	void CFcPanel::showDrivesList()
	{
		_toolbarDrives.showDropDownList();
	}

	//
	// Update Commander's main window title
	//
	void CFcPanel::updateMainWindowCaption( const std::wstring& dirName, DWORD tabId )
	{
		if( FCS::inst().getApp().getActivePanelId() == _id && getActiveTab()->id() == tabId )
		{
			std::wstring productName, productVer;
			SysUtils::getProductAndVersion( productName, productVer );

			auto dir = PathUtils::getLabelFromPath( dirName );
			_tabView.setLabel( dir, tabId );

			productVer = productVer.substr( 0, productVer.find_first_of( L'.' ) + 2 );

			std::wostringstream title;
			title << dir << L" - " << productName << L" " << productVer;
			title << L" (" << SysUtils::getPlatformName() << L")";

			if( IsUserAnAdmin() )
				title << L" (Administrator)";

			SetWindowText( FCS::inst().getFcWindow(), title.str().c_str() );
		}
	}

	void CFcPanel::updateDrivesBar( const std::wstring& path, DWORD tabId )
	{
		if( getActiveTab()->id() == tabId )
		{
			_toolbarDrives.updateToolbarImage( path );
			//_toolbarDrives.updateToolTip( FsUtils::getStringDiskInfo( path ) );
		}
	}

	void CFcPanel::updateDirectoryBox( const std::wstring& dirName, DWORD tabId )
	{
		RedrawWindow( _textBoxDirectory.getHwnd(), NULL, NULL, RDW_INVALIDATE );

		if( getActiveTab()->id() == tabId )
		{
			_textBoxDirectory.updateText( MiscUtils::makeCompactPath( _textBoxDirectory.getHwnd(), PathUtils::stripDelimiter( dirName ) ) );
			_textBoxDirectory.updateToolTip( dirName );
		}
	}

	void CFcPanel::updateFreeSpaceBox( ULONGLONG freeSpace, DWORD tabId )
	{
		RedrawWindow( _textBoxFreeSpace.getHwnd(), NULL, NULL, RDW_INVALIDATE );

		if( getActiveTab()->id() == tabId )
		{
			_textBoxFreeSpace.updateText( FsUtils::bytesToString( freeSpace ) );
			_textBoxFreeSpace.updateToolTip( FsUtils::getStringDiskFreeSpace( freeSpace ) );
		}
	}

	void CFcPanel::updateProperties( const std::wstring& props, DWORD tabId )
	{
		if( getActiveTab()->id() == tabId )
		{
			_staticTextProperties.updateText( props );
			_staticTextProperties.updateToolTip( props );
		}
	}

	//
	// Update controls layout
	//
	void CFcPanel::updateLayout( const RECT& rctPanel )
	{
		_rctPanel = rctPanel;

		updateLayoutTabs();
		updateLayoutCurDrive();
		updateLayoutCurDir();
		updateLayoutFreeSpace();
		updateLayoutProperties();

		if( FCS::inst().getApp().isInitialized() )
		{
			getActiveTab()->updateLayout( rctPanel );

			auto curDir = PathUtils::stripDelimiter( getActiveTab()->getDataManager()->getCurrentDirectory() );
			_textBoxDirectory.updateText( MiscUtils::makeCompactPath( _textBoxDirectory.getHwnd(), curDir ) );
		}
	}

	//
	// Refresh panel
	//
	void CFcPanel::refresh()
	{
		getActiveTab()->refresh();
	}
}
