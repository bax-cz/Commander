#pragma once

/*
	File commander's user interface panel
	-------------------------------------
	Deals with panel content and actions
	Holds following objects:
	 - CDataManager - manages panel's items data
*/

#include "PanelTab.h"

#include "ControlTabView.h"
#include "ControlTextBox.h"
#include "ControlToolbar.h"
#include "ControlStaticText.h"

namespace Commander
{
	//
	// Encapsulate each panel's tabs
	//
	class CFcPanel
	{
	public:
		CFcPanel( BYTE id );
		~CFcPanel();

		inline const BYTE id() const { return _id; }
		void init( const std::wstring& path );
		void cleanUp() { _tabView.cleanUp(); }

		inline int getPanelImageIndex() { return _toolbarDrives.getImageIndex(); }
		inline void gotFocus() { _tabView.getActive()->gotFocus(); }

		void createTab( const std::wstring& path, const std::wstring& itemName = L"", bool active = true );
		void removeTab( int index = -1 );

		inline std::shared_ptr<CPanelTab> getActiveTab() { return _tabView.getActive(); }
		inline int getTabCount() { return _tabView.getCount(); }
		void setActiveTab( int index );
		void switchTab( EFcCommand cmd );

		void updateLayout( const RECT& rctPanel );
		void showDrivesList(); // show drop down list
		void setPanelFocus( bool activate = true );
		void refresh();

		LRESULT handleHistory( LPNMTOOLBAR tbNotifMsg );

	public:
		void updateMainWindowCaption( const std::wstring& dirName, DWORD tabId );
		void updateDrivesBar( const std::wstring& path, DWORD tabId );
		void updateDirectoryBox( const std::wstring& dirName, DWORD tabId );
		void updateFreeSpaceBox( ULONGLONG freeSpace, DWORD tabId );
		void updateProperties( const std::wstring& props, DWORD tabId );

	private:
		void updateLayoutTabs();
		void updateLayoutCurDrive();
		void updateLayoutCurDir();
		void updateLayoutFreeSpace();
		void updateLayoutProperties();

	private:
		BYTE _id; // panel identificator
		RECT _rctPanel; // panel's placement

		CTabView _tabView;
		CToolBar _toolbarDrives;
		CTextBox _textBoxDirectory;
		CTextBox _textBoxFreeSpace;
		CStaticText _staticTextProperties;

		HWND _toolBar;
		HMENU _menuBar;
	};
}
