#pragma once

/*
	File commander's user interface panel
	-------------------------------------
	Deals with panel content and actions
	Holds following objects:
	 - CDataManager - manages panel's items data
*/

#include "ControlListView.h"

#include "HistoryNavigator.h"
#include "DataManager.h"
#include "SshOutput.h"
#include "Commands.h"

namespace Commander
{
	//
	// Encapsulate each panel's controls
	//
	class CPanelTab
	{
	public:
		CPanelTab( DWORD id );
		~CPanelTab();

		const DWORD id() const { return _id; }
		void init( CFcPanel *const panel, const std::wstring& dirName, const std::wstring& itemName = L"" );
		void close();
		void show( int cmdShow = SW_SHOWNORMAL );

		// returns pointer to panel items data manager object
		CDataManager *getDataManager() { return &_dataManager; }

		void updateLayout( const RECT& rctPanel );
		void driveChanged( const std::wstring& path, bool added );
		void selectionChanged();
		void setPanelFocus();
		void gotFocus();
		void refresh( const std::wstring& focusItemName = L"" );

		bool changeDirectory( const std::wstring& dirName, const std::wstring& focusItemName = L"", bool storeHistory = true );
		bool changeDrive( const std::wstring& driveName );
		bool renameEntry( int itemIndex, const std::wstring& fileName );
		void markEntry( const std::wstring& itemName );
		void markEntries( const std::vector<int>& items );
		bool isQuickFindMessage( LPMSG msg ) { return _listViewItems.isQuickFindMessage( msg ); }
		void compareFiles();

		bool isFocusedEntryFile() { return _dataManager.isEntryFile( _listViewItems.getFocusedItemIndex() ); }

		std::vector<std::wstring> getSelectedItemsPathFull( bool allItems = true );
		std::vector<std::wstring> getHistoryList( bool backward = true );
		std::wstring getOppositeDir();

		void onDataManagerNotify( UINT msgId, int retVal );
		bool processCommand( const EFcCommand cmd );
		void navigateThroughHistory( EFcCommand cmd, int step = 1 );

	private:
		bool changeDirectoryGui();
		bool connectToFtpGui();
		bool connectPuttyGui();
		void findFilesGui();
		void findDuplicatesGui();
		void showConfigurationGui();
		void changeAttributesGui();
		void changeCaseGui();
		void sharedDirsGui();
		void splitFileGui();
		void combineFilesGui();
		void downloadFileGui();
		void calcChecksumsGui();
		void verifyChecksumsGui();
		bool changeViewMode( EFcCommand cmd );
		bool ftpTransferGui( EFcCommand cmd );
		bool sshTransferGui( EFcCommand cmd );
		bool startFastCopyGui( EFcCommand cmd );
		void markFileItemsGui( EFcCommand cmd );
		void fileViewerGui( const std::wstring& dirName, const std::wstring& fileName, bool viewHex );
		void fileEditorGui( const std::wstring& dirName, const std::wstring& fileName );
		bool createArchiveGui( const std::wstring& fileName );
		bool extractArchiveGui( const std::wstring& archiveName, bool extractAll = true );
		bool makeNewFileGui( const std::wstring& curPath );
		bool makeDirectoryGui( const std::wstring& curPath );
		bool makeDiskImageGui( const std::wstring& curPath );
		void showCommandPromptGui( const std::wstring& curPath );
		void compareDirectoriesGui();
		void connectNetworkDriveGui();
		void disconnectNetworkDriveGui();

		void processFileOperations( EFcCommand cmd );
		bool focusShortcutTarget( const std::wstring& itemName );

		bool changeDirectoryFromText( const std::wstring& text );

		bool directoryChanging( const std::wstring& dirName );
		bool directoryChanged( const std::wstring& dirName );

		void updateBrowseHistory( const std::wstring& oldDirectory, const std::wstring& oldFocusedItem );
		void updateMenuAndToolbar();
		void updateCommandsMap();
		void updateContentTimerProc();

		std::wstring getSelectedItemsString( const std::vector<int>& markedItems, const std::wstring& title, bool question );

		bool processSorting( EFcCommand cmd, const std::wstring& itemName, const std::wstring& path );
		bool processOpenTab( EFcCommand cmd, const std::wstring& itemName, const std::wstring& path, int index );
		bool processClipboard( EFcCommand cmd, const std::wstring& itemName, const std::wstring& path );
		bool processActionOnItem( const std::wstring& itemName, const std::wstring& dirName, int index );
		bool processActionOnFile( const std::wstring& fileName, const std::wstring& dirName );

		void updateLayoutItems();

	private:
		DWORD _id; // panel's tab identificator
		RECT _rctPanel; // panel's placement

		HWND _toolBar;
		HMENU _menuBar;

		CFcPanel *_pParentPanel;
		CSshOutput *_pSecureShell;
		CListView _listViewItems;

		CDataManager _dataManager; // panel items data manager
		CHistoryNavigator _navigationHistory;

		std::map<TCHAR, std::wstring> _driveDirs; // stores history for each drive letter
		std::map<EFcCommand, bool> _availableCommands;

		std::wstring _focusedItemName;

		bool _directoryChanged; // something has changed in a directory
		bool _contentChanged; // content changed or date/time/size changed only
		bool _reloadOverlays;
		bool _dirChangeTimerSet;
	};
}
