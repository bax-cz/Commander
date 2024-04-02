#include "stdafx.h"

#include <objbase.h>
#include <shlobj.h>

#include "Commander.h"
#include "PathUtils.h"
#include "ShellUtils.h"
#include "MiscUtils.h"
#include "MenuUtils.h"
#include "ToolBarUtils.h"
#include "CalculateChecksums.h"
#include "ChangeCase.h"
#include "ChangeAttributes.h"
#include "CombineFiles.h"
#include "CompareDirectories.h"
#include "CompareFiles.h"
#include "DuplicateFinder.h"
#include "FilePacker.h"
#include "FileEditor.h"
#include "FileFinder.h"
#include "FileUnpacker.h"
#include "FtpLogin.h"
#include "FtpTransfer.h"
#include "HttpDownloadFile.h"
#include "MainConfiguration.h"
#include "SharedDirectories.h"
#include "SplitFile.h"
#include "SshLogin.h"
#include "SshTransfer.h"
#include "VerifyChecksums.h"
#include "ViewerTypes.h"

#include "PanelTab.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CPanelTab::CPanelTab( DWORD id )
		: _pParentPanel( nullptr )
		, _pSecureShell( nullptr )
		, _dirChangeTimerSet( false )
		, _directoryChanged( false )
		, _contentChanged( false )
		, _reloadOverlays( false )
	{
		_id = id;
	}

	CPanelTab::~CPanelTab()
	{
		if( _dirChangeTimerSet )
			KillTimer( _listViewItems.getHwnd(), FC_TIMER_OVERLAYS_ID );
	}


	//
	// Update controls placement
	//

	// List-view panel
	void CPanelTab::updateLayoutItems()
	{
		RECT& rct = _listViewItems.getRect();

		rct.top = _rctPanel.top + 48;
		rct.left = _rctPanel.left;
		rct.bottom = _rctPanel.bottom - ( _pParentPanel->getTabCount() == 1 ? 19 : 38 );
		rct.right = _rctPanel.right;

		_listViewItems.updateLayout();
	}


	//
	// Create and initialize panel tab's controls
	//
	void CPanelTab::init( CFcPanel *const panel, const std::wstring& dirName, const std::wstring& itemName )
	{
		_toolBar = FCS::inst().getApp().getToolbar();
		_menuBar = FCS::inst().getApp().getMenuBar();
		_pParentPanel = panel;

		_listViewItems.init( panel, this );

		changeDirectory( dirName, itemName );

		if( _pParentPanel->id() == FCS::inst().getApp().getActivePanelId() )
			setPanelFocus();
	}

	//
	// Deinitialize panel tab's controls and close tab
	//
	void CPanelTab::close()
	{
		if( _pSecureShell )
		{
			_pSecureShell->destroy();
			_pSecureShell = nullptr;
		}

		_listViewItems.show( SW_HIDE );
		FCS::inst().getApp().getControls().erase( _listViewItems.getHwnd() ); // remove this handle from map
	}

	//
	// Show/hide panel tab's controls
	//
	void CPanelTab::show( int cmdShow )
	{
		_listViewItems.show( cmdShow );
	}

	//
	// Perform action according to file type
	//
	bool CPanelTab::processActionOnFile( const std::wstring& fileName, const std::wstring& dirName )
	{
		if( !_dataManager.isInDiskMode() )
			; // TODO: copy to local tempdir

		if( ArchiveType::isKnownType( fileName ) )
		{
			return changeDirectory( dirName + fileName );
		}
		else if( StringUtils::endsWith( fileName, L".lnk" ) )
		{
			auto target = ShellUtils::getShortcutTarget( dirName + fileName );
			return ShellUtils::executeFile( target, dirName ); // dirName param needed ??
		}

		return ShellUtils::executeFile( fileName, dirName );
	}

	//
	// Set panel control focus
	//
	void CPanelTab::setPanelFocus()
	{
		_listViewItems.focus();
	}

	//
	// Panel got focus and is active now
	//
	void CPanelTab::gotFocus()
	{
		// refresh opposite panel first
		FCS::inst().getApp().getActivePanel().updateDirectoryBox( FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getCurrentDirectory(), _id );
		FCS::inst().getApp().getActivePanel().updateFreeSpaceBox( FCS::inst().getApp().getActivePanel().getActiveTab()->getDataManager()->getFreeSpace(), _id );

		// activate new panel
		FCS::inst().getApp().setActivePanelId( _pParentPanel->id() );

		// refresh current panel
		_pParentPanel->updateDirectoryBox( _dataManager.getCurrentDirectory(), _id );
		_pParentPanel->updateFreeSpaceBox( _dataManager.getFreeSpace(), _id );

		_pParentPanel->updateMainWindowCaption( _dataManager.getCurrentDirectory(), _id );
		updateMenuAndToolbar();
	}

	//
	// Update controls layout
	//
	void CPanelTab::updateLayout( const RECT& rctPanel )
	{
		_rctPanel = rctPanel;

		updateLayoutItems();
	}

	//
	// Refresh panel
	//
	void CPanelTab::refresh( const std::wstring& focusItemName )
	{
		_focusedItemName = focusItemName.empty() ? _dataManager.getEntryName( _listViewItems.getFocusedItemIndex() ) : focusItemName;
		directoryChanging( _dataManager.getCurrentDirectory() );
	}

	//
	// Rescan and update drives
	//
	void CPanelTab::driveChanged( const std::wstring& path, bool added )
	{
		if( !added && _dataManager.isInDiskMode() && _dataManager.getRootPath() == path )
		{
			directoryChanging( ShellUtils::getKnownFolderPath( FOLDERID_Documents ) );
		}
	}

	//
	// Compare files' contents
	//
	void CPanelTab::compareFiles()
	{
		CBaseDialog::createModeless<CCompareFiles>();
	}

	//
	// Try to guess directory and/or filename from random clipboard (or gui) text
	//
	bool CPanelTab::changeDirectoryFromText( const std::wstring& text )
	{
		std::wstring path = text;
		StringUtils::trim( path );

		bool isInternalPath = ReaderType::getRequestedMode( path, false ) != EReadMode::Disk;

		if( !isInternalPath && PathIsRelative( path.c_str() ) )
			path = _dataManager.getCurrentDirectory() + path;

		WIN32_FIND_DATA wfd = { 0 };

		if( isInternalPath || ( FsUtils::getFileInfo( path, wfd ) && !IsDir( wfd.dwFileAttributes ) ) )
			return changeDirectory( PathUtils::stripFileName( path ), PathUtils::stripPath( path ) );
		else
			return changeDirectory( path );
	}

	//
	// On timer procedure - refresh data entries
	//
	void CPanelTab::updateContentTimerProc()
	{
		if( _directoryChanged )
		{
			if( _contentChanged )
			{
				_contentChanged = false;

				auto dirName = PathUtils::stripDelimiter( _dataManager.getCurrentDirectory() );

				_listViewItems.updateItems( _focusedItemName );
				_pParentPanel->updateDirectoryBox( dirName, _id );
				_pParentPanel->updateDrivesBar( _dataManager.getRootPath(), _id );
				_pParentPanel->updateMainWindowCaption( dirName, _id );

				updateMenuAndToolbar();
			}
			else
				_listViewItems.updateProperties();

			// when content has not changed - update properties and freespace only
			_pParentPanel->updateFreeSpaceBox( _dataManager.getFreeSpace(), _id );
			_pParentPanel->updateProperties( _dataManager.getPropertiesText(), _id );
		}
		else
		{
			_dirChangeTimerSet = !KillTimer( _listViewItems.getHwnd(), FC_TIMER_OVERLAYS_ID );

			// on the last timer call reload overlays
			if( _reloadOverlays )
			{
				_reloadOverlays = false;
				SendNotifyMessage( _listViewItems.getHwnd(), UM_DIRCONTENTCH, 2, 0 );
			}
		}

		PrintDebug("timer:%d, change:%d", _dirChangeTimerSet, _directoryChanged);

		_directoryChanged = false;
	}

	//
	// On data manager notification
	//
	void CPanelTab::onDataManagerNotify( UINT msgId, int retVal )
	{
		switch( msgId )
		{
		case WM_TIMER:
			updateContentTimerProc();
			break;

		case UM_DIRCONTENTCH: // there's been some change(s) inside directory
			_directoryChanged = true;

			if( retVal )
				_contentChanged = _reloadOverlays = true;

			if( !_dirChangeTimerSet )
			{
				_dirChangeTimerSet = !!SetTimer( _listViewItems.getHwnd(), FC_TIMER_OVERLAYS_ID, FC_TIMER_OVERLAYS_TICK, NULL );
				updateContentTimerProc();
			}
			break;

		case UM_READERNOTIFY:
			if( !retVal )
			{
				std::wostringstream sstr;
				sstr << L"Path: \"" << _dataManager.getCurrentDirectory() << L"\"\r\n";
				sstr << _dataManager.getLastErrorMessage();

				MessageBox( NULL, sstr.str().c_str(), L"Error Changing Directory", MB_ICONEXCLAMATION | MB_OK );
				//directoryChanged( FsUtils::getClosestExistingDir( _dataManager.getCurrentDirectory() ) );
				navigateThroughHistory( EFcCommand::HistoryBackward );
			}
			else
				directoryChanged( _dataManager.getCurrentDirectory() );
			break;

		case UM_CALCSIZEDONE:
			selectionChanged();
			break;

		case UM_ARCHOPENDONE:
			if( !retVal )
			{
			//if( _dataManager.getStatus() == EReaderStatus::DataError )
				MessageBox( NULL, _dataManager.getLastErrorMessage().c_str(), L"Archive Error", MB_ICONERROR | MB_OK );
				navigateThroughHistory( EFcCommand::HistoryBackward );
			}
			else
				directoryChanged( _dataManager.getCurrentDirectory() );
			break;
		}
	}

	//
	// Rescan file system items and update controls accordingly
	//
	bool CPanelTab::directoryChanging( const std::wstring& dirName )
	{
		if( ( _dataManager.isInPuttyMode() || _dataManager.isInFtpMode() ) && !_dataManager.isPathStillValid( dirName ) )
		{
			// check whether we are still active
			if( _dataManager.isPathStillValid( _dataManager.getCurrentDirectory() ) )
			{
				if( MessageBox( FCS::inst().getFcWindow(), L"Disconnect from server?", L"Change Directory", MB_ICONQUESTION | MB_YESNO ) == IDNO )
					return false;
			}

			if( _pSecureShell )
			{
				_pSecureShell->destroy();
				_pSecureShell = nullptr;
			}
		}

		auto curDir = PathUtils::stripDelimiter( dirName );

		// pre-update Commander main window title
		_pParentPanel->updateMainWindowCaption( curDir, _id );
		_pParentPanel->updateDirectoryBox( curDir, _id );

		// update data file system items
		if( !_dataManager.readFromPath( curDir ) )
		{
			bool ret = true;

			if( _dataManager.isInPuttyMode() )
				ret = connectPuttyGui();
			else if( _dataManager.isInFtpMode() )
				ret = connectToFtpGui();

			// fall back to documents folder
			if( !ret )
				_dataManager.readFromPath( ShellUtils::getKnownFolderPath( FOLDERID_Documents ) );
		}

		switch( _dataManager.getStatus() )
		{
		case EReaderStatus::ReadingData:
			_listViewItems.updateItems();
			_pParentPanel->updateDrivesBar( _dataManager.getRootPath(), _id );
			_pParentPanel->updateProperties( _dataManager.getPropertiesText(), _id );
			break;

		case EReaderStatus::DataOk:
			_listViewItems.updateItems( _focusedItemName );
			_pParentPanel->updateDirectoryBox( curDir, _id );
			_pParentPanel->updateFreeSpaceBox( _dataManager.getFreeSpace(), _id );
			_pParentPanel->updateDrivesBar( _dataManager.getRootPath(), _id );
			_pParentPanel->updateMainWindowCaption( curDir, _id );
			break;

		case EReaderStatus::Invalid:
		case EReaderStatus::DataError:
			PrintDebug("Error reading path: '%ls'", curDir.c_str());
			return false;
		}

		return true;
	}

	//
	// TODO: duplicated directoryChanging - rewrite
	//
	bool CPanelTab::directoryChanged( const std::wstring& dirName )
	{
		auto curDir = PathUtils::stripDelimiter( dirName );

		// pre-update Commander main window title
		_pParentPanel->updateMainWindowCaption( curDir, _id );
		_pParentPanel->updateDirectoryBox( curDir, _id );

		_dataManager.readFromPath( curDir );

		switch( _dataManager.getStatus() )
		{
		case EReaderStatus::DataOk:
			_listViewItems.updateItems( _focusedItemName );
			_pParentPanel->updateDirectoryBox( curDir, _id );
			_pParentPanel->updateFreeSpaceBox( _dataManager.getFreeSpace(), _id );
			_pParentPanel->updateDrivesBar( _dataManager.getRootPath(), _id );
			_pParentPanel->updateProperties( _dataManager.getPropertiesText(), _id );
			_pParentPanel->updateMainWindowCaption( curDir, _id );
			break;

		case EReaderStatus::Invalid:
		case EReaderStatus::DataError:
			PrintDebug("Error reading path: '%ls'", curDir.c_str());
			return false;
		}

		return true;
	}

	//
	// Selection in list-view has been changed
	//
	void CPanelTab::selectionChanged()
	{
		if( _dataManager.getStatus() == EReaderStatus::DataOk )
			_focusedItemName = _dataManager.getEntryName( _listViewItems.getFocusedItemIndex() );

		_pParentPanel->updateProperties( _dataManager.getPropertiesText(), _id );

		updateMenuAndToolbar();
	}

	void CPanelTab::updateCommandsMap()
	{
		int focusedItemIndex = _listViewItems.getFocusedItemIndex();
		std::wstring itemName = _dataManager.getEntryName( focusedItemIndex );

		bool isFile = _dataManager.isEntryFile( focusedItemIndex );
		bool isFixed = _dataManager.isEntryFixed( focusedItemIndex );
		bool isReadOnly = _dataManager.isInReadOnlyMode();
		bool isMarked = !_dataManager.getMarkedEntries().empty();
		bool isEmpty = !_dataManager.getEntryCount();
		bool canGoUp = !PathUtils::getParentDirectory( _dataManager.getCurrentDirectory() ).empty();

		bool canCompareFiles = false;

		auto oppositeTab = FCS::inst().getApp().getOppositePanel().getActiveTab();
		if( oppositeTab )
		{
			auto dataManOpp = oppositeTab->getDataManager();
			bool isFileOpp = oppositeTab->isFocusedEntryFile();
			bool isMarkedOpp = !dataManOpp->getMarkedEntries().empty();
			auto& entriesOpp = dataManOpp->getMarkedEntries();
			auto& entries = _dataManager.getMarkedEntries();

			canCompareFiles = (!isMarked && !isMarkedOpp && isFile && isFileOpp)
				|| (entries.size() == 1 && entriesOpp.size() == 1 && _dataManager.isEntryFile( entries[0] ) && dataManOpp->isEntryFile( entriesOpp[0] ))
				|| (entries.size() == 2 && _dataManager.isEntryFile( entries[0] ) && _dataManager.isEntryFile( entries[1] ));
		}

		_availableCommands[EFcCommand::SetAlwaysOnTop] = true;
		_availableCommands[EFcCommand::ExecuteItem] = true;
		_availableCommands[EFcCommand::PackItem] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::UnpackItem] = isFile && ArchiveType::isKnownType( itemName );
		_availableCommands[EFcCommand::RenameItem] = !isFixed && !isReadOnly;
		_availableCommands[EFcCommand::CopyItems] = (!isFixed || isMarked) && !_dataManager.isInNetworkMode();
		_availableCommands[EFcCommand::MoveItems] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::LinkItem] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::RecycleItems] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::DeleteItems] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::FindFiles] = !isReadOnly;
		_availableCommands[EFcCommand::FindDuplicates] = !isReadOnly;
		_availableCommands[EFcCommand::ViewFile] = isFile && !_dataManager.isInNetworkMode();
		_availableCommands[EFcCommand::ViewFileHex] = isFile && !_dataManager.isInNetworkMode();
		_availableCommands[EFcCommand::EditFile] = isFile && !isReadOnly;
		_availableCommands[EFcCommand::SplitFile] = isFile && !isReadOnly;
		_availableCommands[EFcCommand::CombineFile] = (isFile || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::DownloadFile] = !isReadOnly;
		_availableCommands[EFcCommand::ChecksumCalc] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::ChecksumVerify] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::ChangeAttr] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::ChangeCase] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::ChangeDir] = true;
		_availableCommands[EFcCommand::MakeDir] = !isReadOnly;
		_availableCommands[EFcCommand::MakeFile] = !isReadOnly;
		_availableCommands[EFcCommand::MakeFileList] = !isEmpty;
		_availableCommands[EFcCommand::MarkFiles] = !isEmpty;
		_availableCommands[EFcCommand::UnmarkFiles] = isMarked;
		_availableCommands[EFcCommand::SelectAll] = !isEmpty;
		_availableCommands[EFcCommand::UnselectAll] = isMarked;
		_availableCommands[EFcCommand::SelectSameExt] = !isEmpty;
		_availableCommands[EFcCommand::UnselectSameExt] = isMarked;
		_availableCommands[EFcCommand::SelectSameName] = !isEmpty;
		_availableCommands[EFcCommand::UnselectSameName] = isMarked;
		_availableCommands[EFcCommand::InvertSelectionFiles] = !isEmpty;
		_availableCommands[EFcCommand::InvertSelectionAll] = !isEmpty;
		_availableCommands[EFcCommand::ShowCmdPrompt] = _dataManager.isInDiskMode() || _dataManager.isInPuttyMode();
		_availableCommands[EFcCommand::ShowProperties] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::ShowConfiguration] = true;
		_availableCommands[EFcCommand::ShowSharedDirs] = true;
		_availableCommands[EFcCommand::GoRoot] = canGoUp;
		_availableCommands[EFcCommand::GoUpper] = canGoUp;
		_availableCommands[EFcCommand::GoFtp] = true;
		_availableCommands[EFcCommand::GoPutty] = true;
		_availableCommands[EFcCommand::GoNetwork] = true;
		_availableCommands[EFcCommand::GoReged] = true;
		_availableCommands[EFcCommand::GoPathOtherPanel] = true;
		_availableCommands[EFcCommand::GoShortcutTarget] = isFile && StringUtils::endsWith( itemName, L".lnk" );
		_availableCommands[EFcCommand::SwapPanels] = true;
		_availableCommands[EFcCommand::HistoryForward] = _navigationHistory.canGoForward();
		_availableCommands[EFcCommand::HistoryBackward] = _navigationHistory.canGoBackward();
		_availableCommands[EFcCommand::OpenTab] = true;
		_availableCommands[EFcCommand::OpenTabOpposite] = true;
		_availableCommands[EFcCommand::PrevTab] = true;
		_availableCommands[EFcCommand::NextTab] = true;
		_availableCommands[EFcCommand::CloseTab] = _pParentPanel->getTabCount() > 1;
		_availableCommands[EFcCommand::ClipCut] = (!isFixed || isMarked) && !isReadOnly;
		_availableCommands[EFcCommand::ClipCopy] = (!isFixed || isMarked) && !_dataManager.isInNetworkMode();
		_availableCommands[EFcCommand::ClipPaste] = !isReadOnly;
		_availableCommands[EFcCommand::ClipCopyName] = !isFixed;
		_availableCommands[EFcCommand::ClipCopyPath] = true;
		_availableCommands[EFcCommand::ClipCopyPathFull] = !isFixed;
		_availableCommands[EFcCommand::ViewModeBrief] = !_listViewItems.isInBriefMode();
		_availableCommands[EFcCommand::ViewModeDetailed] = !_listViewItems.isInDetailedMode();
		_availableCommands[EFcCommand::SortByName] = !_dataManager.isSortedByName();
		_availableCommands[EFcCommand::SortByExt] = !_dataManager.isSortedByExtension();
		_availableCommands[EFcCommand::SortByTime] = !_dataManager.isSortedByTime();
		_availableCommands[EFcCommand::SortBySize] = !_dataManager.isSortedBySize();
		_availableCommands[EFcCommand::SortByAttr] = !_dataManager.isSortedByAttr();
		_availableCommands[EFcCommand::CompareDirs] = true;
		_availableCommands[EFcCommand::CompareFiles] = canCompareFiles;
		_availableCommands[EFcCommand::ConnectNetDrive] = true;
		_availableCommands[EFcCommand::DisconnectNetDrive] = true;
		_availableCommands[EFcCommand::OpenFolderActive] = !isReadOnly;
		_availableCommands[EFcCommand::OpenFolderComputer] = true;
		_availableCommands[EFcCommand::OpenFolderControlPanel] = true;
		_availableCommands[EFcCommand::OpenFolderDesktop] = true;
		_availableCommands[EFcCommand::OpenFolderFonts] = true;
		_availableCommands[EFcCommand::OpenFolderMyDocuments] = true;
		_availableCommands[EFcCommand::OpenFolderNetwork] = true;
		_availableCommands[EFcCommand::OpenFolderPrinters] = true;
		_availableCommands[EFcCommand::OpenFolderRecycleBin] = true;
		_availableCommands[EFcCommand::CreateIso] = true; // ?
	}

	//
	// Update menu and toolbar items states according to real states
	//
	void CPanelTab::updateMenuAndToolbar()
	{
		updateCommandsMap();

		// check whether this tab is an active one
		if( _pParentPanel->getActiveTab()->id() != _id )
			return;

		if( FCS::inst().getApp().getPanelLeft().id() == _pParentPanel->id() )
		{
			// left panel specific
			MenuUtils::enableItem( _menuBar, IDM_LEFT_CLOSETAB,              _availableCommands[EFcCommand::CloseTab] );

			MenuUtils::enableItem( _menuBar, IDM_LEFT_GOTO_BACK,             _availableCommands[EFcCommand::HistoryBackward] );
			MenuUtils::enableItem( _menuBar, IDM_LEFT_GOTO_FORWARD,          _availableCommands[EFcCommand::HistoryForward] );
			MenuUtils::enableItem( _menuBar, IDM_LEFT_GOTO_PARENTDIRECTORY,  _availableCommands[EFcCommand::GoUpper] );
			MenuUtils::enableItem( _menuBar, IDM_LEFT_GOTO_ROOTDIRECTORY,    _availableCommands[EFcCommand::GoRoot] );

			MenuUtils::checkItem( _menuBar, IDM_LEFT_BRIEF,                 !_availableCommands[EFcCommand::ViewModeBrief] );
			MenuUtils::checkItem( _menuBar, IDM_LEFT_DETAILED,              !_availableCommands[EFcCommand::ViewModeDetailed] );

			MenuUtils::checkItem( _menuBar, IDM_LEFT_SORTBY_NAME,           !_availableCommands[EFcCommand::SortByName] );
			MenuUtils::checkItem( _menuBar, IDM_LEFT_SORTBY_EXTENSION,      !_availableCommands[EFcCommand::SortByExt] );
			MenuUtils::checkItem( _menuBar, IDM_LEFT_SORTBY_TIME,           !_availableCommands[EFcCommand::SortByTime] );
			MenuUtils::checkItem( _menuBar, IDM_LEFT_SORTBY_SIZE,           !_availableCommands[EFcCommand::SortBySize] );
			MenuUtils::checkItem( _menuBar, IDM_LEFT_SORTBY_ATTR,           !_availableCommands[EFcCommand::SortByAttr] );
		}
		else // right panel specific
		{
			MenuUtils::enableItem( _menuBar, IDM_RIGHT_CLOSETAB,             _availableCommands[EFcCommand::CloseTab] );

			MenuUtils::enableItem( _menuBar, IDM_RIGHT_GOTO_BACK,            _availableCommands[EFcCommand::HistoryBackward] );
			MenuUtils::enableItem( _menuBar, IDM_RIGHT_GOTO_FORWARD,         _availableCommands[EFcCommand::HistoryForward] );
			MenuUtils::enableItem( _menuBar, IDM_RIGHT_GOTO_PARENTDIRECTORY, _availableCommands[EFcCommand::GoUpper] );
			MenuUtils::enableItem( _menuBar, IDM_RIGHT_GOTO_ROOTDIRECTORY,   _availableCommands[EFcCommand::GoRoot] );

			MenuUtils::checkItem( _menuBar, IDM_RIGHT_BRIEF,                !_availableCommands[EFcCommand::ViewModeBrief] );
			MenuUtils::checkItem( _menuBar, IDM_RIGHT_DETAILED,             !_availableCommands[EFcCommand::ViewModeDetailed] );

			MenuUtils::checkItem( _menuBar, IDM_RIGHT_SORTBY_NAME,          !_availableCommands[EFcCommand::SortByName] );
			MenuUtils::checkItem( _menuBar, IDM_RIGHT_SORTBY_EXTENSION,     !_availableCommands[EFcCommand::SortByExt] );
			MenuUtils::checkItem( _menuBar, IDM_RIGHT_SORTBY_TIME,          !_availableCommands[EFcCommand::SortByTime] );
			MenuUtils::checkItem( _menuBar, IDM_RIGHT_SORTBY_SIZE,          !_availableCommands[EFcCommand::SortBySize] );
			MenuUtils::checkItem( _menuBar, IDM_RIGHT_SORTBY_ATTR,          !_availableCommands[EFcCommand::SortByAttr] );
		}

		if( FCS::inst().getApp().getActivePanelId() == _pParentPanel->id() )
		{
			// active panel
			TbUtils::enableItem( _toolBar, IDM_GOTO_BACK,                    _availableCommands[EFcCommand::HistoryBackward] );
			TbUtils::enableItem( _toolBar, IDM_GOTO_FORWARD,                 _availableCommands[EFcCommand::HistoryForward] );
			TbUtils::enableItem( _toolBar, IDM_GOTO_PARENTDIRECTORY,         _availableCommands[EFcCommand::GoUpper] );

			TbUtils::enableItem( _toolBar, IDM_EDIT_SELECTALL,               _availableCommands[EFcCommand::SelectAll] );
			TbUtils::enableItem( _toolBar, IDM_EDIT_UNSELECTALL,             _availableCommands[EFcCommand::UnselectAll] );
			TbUtils::enableItem( _toolBar, IDM_EDIT_CUT,                     _availableCommands[EFcCommand::ClipCut] );
			TbUtils::enableItem( _toolBar, IDM_EDIT_COPY,                    _availableCommands[EFcCommand::ClipCopy] );
			TbUtils::enableItem( _toolBar, IDM_EDIT_PASTE,                   _availableCommands[EFcCommand::ClipPaste] );

			TbUtils::enableItem( _toolBar, IDM_FILES_PACK,                   _availableCommands[EFcCommand::PackItem] );
			TbUtils::enableItem( _toolBar, IDM_FILES_UNPACK,                 _availableCommands[EFcCommand::UnpackItem] );
			TbUtils::enableItem( _toolBar, IDM_FILES_PROPERTIES,             _availableCommands[EFcCommand::ShowProperties] );
			TbUtils::enableItem( _toolBar, IDM_FILES_CHANGECASE,             _availableCommands[EFcCommand::ChangeCase] );
			TbUtils::enableItem( _toolBar, IDM_FILES_CHANGEATTRIBUTES,       _availableCommands[EFcCommand::ChangeAttr] );

			TbUtils::enableItem( _toolBar, IDM_COMMANDS_FINDFILES,           _availableCommands[EFcCommand::FindFiles] );
			TbUtils::enableItem( _toolBar, IDM_COMMANDS_COMPAREDIRECTORIES,  _availableCommands[EFcCommand::CompareDirs] );
			TbUtils::enableItem( _toolBar, IDM_COMMANDS_COMMANDSHELL,        _availableCommands[EFcCommand::ShowCmdPrompt] );
			TbUtils::enableItem( _toolBar, IDM_OPENFOLDER_ACTIVEFOLDER,      _availableCommands[EFcCommand::OpenFolderActive] );

			MenuUtils::enableItem( _menuBar, IDM_FILES_QUICKRENAME,          _availableCommands[EFcCommand::RenameItem] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_VIEW,                 _availableCommands[EFcCommand::ViewFile] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_ALTERNATEVIEW,        _availableCommands[EFcCommand::ViewFileHex] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_EDIT,                 _availableCommands[EFcCommand::EditFile] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_EDITNEWFILE,          _availableCommands[EFcCommand::MakeFile] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_COPY,                 _availableCommands[EFcCommand::CopyItems] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_MOVE,                 _availableCommands[EFcCommand::MoveItems] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_DELETE,               _availableCommands[EFcCommand::DeleteItems] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_PROPERTIES,           _availableCommands[EFcCommand::ShowProperties] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_CHANGEATTRIBUTES,     _availableCommands[EFcCommand::ChangeAttr] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_CHANGECASE,           _availableCommands[EFcCommand::ChangeCase] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_PACK,                 _availableCommands[EFcCommand::PackItem] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_UNPACK,               _availableCommands[EFcCommand::UnpackItem] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_SPLIT,                _availableCommands[EFcCommand::SplitFile] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_COMBINE,              _availableCommands[EFcCommand::CombineFile] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_DOWNLOAD,             _availableCommands[EFcCommand::DownloadFile] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_CALCULATECHECKSUMS,   _availableCommands[EFcCommand::ChecksumCalc] );
			MenuUtils::enableItem( _menuBar, IDM_FILES_VERIFYCHECKSUMS,      _availableCommands[EFcCommand::ChecksumVerify] );

			MenuUtils::enableItem( _menuBar, IDM_EDIT_CUT,                   _availableCommands[EFcCommand::ClipCut] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_COPY,                  _availableCommands[EFcCommand::ClipCopy] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_PASTE,                 _availableCommands[EFcCommand::ClipPaste] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_COPYPATHFULL,          _availableCommands[EFcCommand::ClipCopyPathFull] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_COPYNAMEASTEXT,        _availableCommands[EFcCommand::ClipCopyName] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_COPYPATHASTEXT,        _availableCommands[EFcCommand::ClipCopyPath] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_SELECT,                _availableCommands[EFcCommand::MarkFiles] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_UNSELECT,              _availableCommands[EFcCommand::UnmarkFiles] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_INVERTSELECTION,       _availableCommands[EFcCommand::InvertSelectionFiles] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_SELECTALL,             _availableCommands[EFcCommand::SelectAll] );
			MenuUtils::enableItem( _menuBar, IDM_EDIT_UNSELECTALL,           _availableCommands[EFcCommand::UnselectAll] );

			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_CREATEDIRECTORY,   _availableCommands[EFcCommand::MakeDir] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_CHANGEDIRECTORY,   _availableCommands[EFcCommand::ChangeDir] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_COMPAREDIRECTORIES,_availableCommands[EFcCommand::CompareDirs] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_COMPAREFILES,      _availableCommands[EFcCommand::CompareFiles] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_CONNECTNETDRIVE,   _availableCommands[EFcCommand::ConnectNetDrive] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_DISCONNECTNETDRIVE,_availableCommands[EFcCommand::DisconnectNetDrive] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_FINDFILES,         _availableCommands[EFcCommand::FindFiles] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_MAKEFILELIST,      _availableCommands[EFcCommand::MakeFileList] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_GOTOSHORTCUTTARGET,_availableCommands[EFcCommand::GoShortcutTarget] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_SHAREDDIRECTORIES, _availableCommands[EFcCommand::ShowSharedDirs] );
			MenuUtils::enableItem( _menuBar, IDM_COMMANDS_COMMANDSHELL,      _availableCommands[EFcCommand::ShowCmdPrompt] );
			MenuUtils::enableItem( _menuBar, IDM_OPENFOLDER_ACTIVEFOLDER,    _availableCommands[EFcCommand::OpenFolderActive] );
		}
	}

	//
	// Execute file or change the directory
	//
	bool CPanelTab::processActionOnItem( const std::wstring& itemName, const std::wstring& dirName, int index )
	{
		switch( _dataManager.getEntryType( index ) )
		{
		case EEntryType::File: // open file
			return processActionOnFile( itemName, dirName );

		case EEntryType::Reparse: // read junction
		{
			auto reparseTarget = FsUtils::getReparsePointTarget( dirName + itemName );
			if( !reparseTarget.empty() ) // fall-through otherwise
				return changeDirectory( reparseTarget );
		}
		case EEntryType::Directory: // change directory
			return changeDirectory( dirName + itemName );
		}

		return false;
	}

	//
	// Open new tab in active or opposite panel
	//
	bool CPanelTab::processOpenTab( EFcCommand cmd, const std::wstring& itemName, const std::wstring& path, int index )
	{
		auto targetPath = path;

		if( index != -1 )
		{
			switch( _dataManager.getEntryType( index ) )
			{
			case EEntryType::Reparse:
				targetPath = FsUtils::getReparsePointTarget( path + itemName );
				if( !targetPath.empty() ) // fall-through otherwise
					break;
			case EEntryType::Directory:
				targetPath = path + itemName;
				break;
			}
		}

		switch( cmd )
		{
		case EFcCommand::OpenTab:
			_pParentPanel->createTab( targetPath, ( targetPath == path ) ? itemName : L"" );
			break;
		case EFcCommand::OpenTabOpposite:
			FCS::inst().getApp().getOppositePanel().createTab( targetPath, ( targetPath == path ) ? itemName : L"" );
			break;
		}

		return true;
	}

	//
	// Clipboard actions
	//
	bool CPanelTab::processClipboard( EFcCommand cmd, const std::wstring& itemName, const std::wstring& path )
	{
		std::wstring clipText = MiscUtils::getClipboardText( FCS::inst().getFcWindow() );
	//	std::vector<std::wstring> items;
	//	std::string cmdVerb;

		switch( cmd )
		{
		case EFcCommand::ClipCut:
			_listViewItems.cutSelectedItems();
			return ShellUtils::copyItemsToClipboard( getSelectedItemsPathFull(), false );
			//items = getSelectedItemsPathFull();
			//cmdVerb = "cut";
			//break;
		case EFcCommand::ClipCopy:
			return ShellUtils::copyItemsToClipboard( getSelectedItemsPathFull() );
			//items = getSelectedItemsPathFull();
			//cmdVerb = "copy";
			//break;
		case EFcCommand::ClipPaste:
			if( clipText.empty() )
			{
				//copyItemsFromClipboard( path );
				//items.push_back( path );
				//cmdVerb = "paste";
				auto upThread = std::make_unique<std::thread>( ShellUtils::copyItemsFromClipboard, path, FCS::inst().getFcWindow() );
				upThread->detach();
				break;
			}
			else
				return changeDirectoryFromText( clipText );
		}

	//	ShellUtils::invokeCommand( cmdVerb, items );

		return true;
	}

	//
	// Prepare FastCopy and start process
	//
	bool CPanelTab::changeViewMode( EFcCommand cmd )
	{
		switch( cmd )
		{
		case EFcCommand::ViewModeBrief:
			_listViewItems.setViewMode( EViewMode::Brief );
			break;
		case EFcCommand::ViewModeDetailed:
			_listViewItems.setViewMode( EViewMode::Detailed );
			break;
		}

		updateMenuAndToolbar();

		return false;
	}

	//
	// Prepare FastCopy and start process
	//
	bool CPanelTab::processSorting( EFcCommand cmd, const std::wstring& itemName, const std::wstring& path )
	{
		ESortMode mode;

		switch( cmd )
		{
		case EFcCommand::SortByName:
			mode = ESortMode::ByName;
			break;
		case EFcCommand::SortByExt:
			mode = ESortMode::ByExt;
			break;
		case EFcCommand::SortByTime:
			mode = ESortMode::ByTime;
			break;
		case EFcCommand::SortBySize:
			mode = ESortMode::BySize;
			break;
		case EFcCommand::SortByAttr:
			mode = ESortMode::ByAttr;
			break;
		default:
			mode = ESortMode::ByName;
		}

		auto name = _dataManager.getEntryName( _listViewItems.getFocusedItemIndex() );

		// do sorting and refresh list-view items
		_dataManager.sortEntries( mode );
		_listViewItems.updateItems( name );
		//_listViewItems.show( SW_HIDE );
		//_listViewItems.show();
		//_listViewItems.focus();

		updateMenuAndToolbar();

		return true;
	}

	//
	// Format selected file/directory items as string
	//
	std::wstring CPanelTab::getSelectedItemsString( const std::vector<int>& markedItems, const std::wstring& title, bool question )
	{
		std::wostringstream sstr; sstr << title << L" ";
		if( markedItems.empty() )
		{
			int focusedItem = _listViewItems.getFocusedItemIndex();
			std::wstring itemName = _dataManager.getEntryName( focusedItem );

			if( _dataManager.getEntryType( focusedItem ) == EEntryType::File )
				sstr << L"file";
			else
				sstr << L"directory";

			sstr << L" \"" << itemName << "\"";
		}
		else
		{
			int filesCount = 0, dirsCount = 0;
			_dataManager.countEntries( markedItems, filesCount, dirsCount );
			sstr << FsUtils::getFormatStrItemsCount( filesCount, dirsCount );
		}

		sstr << ( question ? L"?" : L" to:" );

		return sstr.str();
	}

	//
	// Get selected file(s) and/or directories as a string array
	//
	std::vector<std::wstring> CPanelTab::getSelectedItemsPathFull( bool allItems )
	{
		std::vector<std::wstring> itemsOut;
		auto& markedItems = _dataManager.getMarkedEntries();

		if( !markedItems.empty() && allItems )
		{
			for( auto it = markedItems.begin(); it != markedItems.end(); ++it )
			{
				itemsOut.push_back( _dataManager.getEntryPathFull( *it ) );
			}
		}
		else
		{
			int focusedItem = _listViewItems.getFocusedItemIndex();
			auto selectedItemName = _dataManager.getEntryName( focusedItem );
			itemsOut.push_back( _dataManager.getEntryPathFull( selectedItemName ) );
		}

		return itemsOut;
	}

	//
	// Get browsing history list backward (or forward)
	//
	std::vector<std::wstring> CPanelTab::getHistoryList( bool backward )
	{
		return _navigationHistory.getList( backward );
	}

	//
	// Get current directory from opposite panel
	//
	std::wstring CPanelTab::getOppositeDir()
	{
		return FCS::inst().getApp().getOppositePanel().getActiveTab()->getDataManager()->getCurrentDirectory();
	}

	//
	// Start copy/move/delete/extract process
	//
	void CPanelTab::processFileOperations( EFcCommand cmd )
	{
		if( _dataManager.isInArchiveMode() )
		{
			extractArchiveGui( _dataManager.getRootPath(), false );
		}
		else if( _dataManager.isInFtpMode() )
		{
			ftpTransferGui( cmd );
		}
		else if( _dataManager.isInPuttyMode() )
		{
			sshTransferGui( cmd );
		}
		else if( _dataManager.isInRegedMode() )
		{
			// TODO
		}
		else if( _dataManager.isInNetworkMode() )
		{
			// nothing to do
		}
		else
		{
			startFastCopyGui( cmd );
		}
	}

	//
	// Prepare FastCopy and start process
	//
	bool CPanelTab::startFastCopyGui( EFcCommand cmd )
	{
		std::wstring title, text;
		bool isDelete = CFastCopyManager::getModeName( cmd, title, text ) == FastCopy::Mode::DELETE_MODE;

		if( isDelete )
		{
			auto upThread = std::make_unique<std::thread>( ShellUtils::deleteItems, getSelectedItemsPathFull(), cmd == EFcCommand::RecycleItems, FCS::inst().getFcWindow() );
			upThread->detach();
			return true;
		}

		std::wstring caption = getSelectedItemsString( _dataManager.getMarkedEntries(), title, isDelete );
		CTextInput::CParams params( title, caption, getOppositeDir(), !isDelete, false, 400 );
		std::wstring result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			PathUtils::unifyDelimiters( result );

			auto pDataManOpposite = FCS::inst().getApp().getOppositePanel().getActiveTab()->getDataManager();
			if( pDataManOpposite->isInDiskMode() )
			{
				auto upThread = std::make_unique<std::thread>( ShellUtils::copyItems, getSelectedItemsPathFull(), result, cmd == EFcCommand::MoveItems, FCS::inst().getFcWindow() );
				upThread->detach();
				//FCS::inst().getFastCopyManager().start( cmd, getSelectedItemsPathFull(), result );
			}
			else if( pDataManOpposite->isInPuttyMode() )
				CBaseDialog::createModeless<CSshTransfer>()->transferFiles( cmd, getSelectedItemsPathFull(), _dataManager.getMarkedEntriesSize(),
					result, FCS::inst().getApp().getOppositePanel().getActiveTab(), false );
			else if( pDataManOpposite->isInFtpMode() )
				CBaseDialog::createModeless<CFtpTransfer>()->transferFiles( cmd, getSelectedItemsPathFull(), _dataManager.getMarkedEntriesSize(),
					result, FCS::inst().getApp().getOppositePanel().getActiveTab(), false );
		}

		return true;
	}

	//
	// Show command prompt
	//
	void CPanelTab::showCommandPromptGui( const std::wstring& curPath )
	{
		if( _dataManager.isInDiskMode() )
		{
			ShellUtils::executeCommand( L"cmd", curPath );
		}
		else if( _dataManager.isInPuttyMode() )
		{
			if( !_pSecureShell )
			{
				_pSecureShell = CBaseDialog::createModeless<CSshOutput>();
				_pSecureShell->setParent( _pParentPanel->getActiveTab() );
				_pSecureShell->show();
			}
			else
				_pSecureShell->show( !_pSecureShell->visible() );
		}
	}

	//
	// Find files
	//
	void CPanelTab::findFilesGui()
	{
		CBaseDialog::createModeless<CFileFinder>();
	}

	//
	// Find duplicates
	//
	void CPanelTab::findDuplicatesGui()
	{
		CBaseDialog::createModeless<CDuplicateFinder>();
	}

	//
	// File viewer
	//
	void CPanelTab::fileViewerGui( const std::wstring& dirName, const std::wstring& fileName, bool viewHex )
	{
		ViewerType::createViewer( fileName, viewHex )->viewFile( dirName, fileName, _pParentPanel->getActiveTab() );
	}

// FIXME: quick hack - test threaded modeless dialog
bool testThreadProc( const std::wstring& dirName, const std::wstring& fileName )
{
	ShellUtils::CComInitializer _com;
	CBaseDialog::createModeless<CFileEditor>( nullptr, StringUtils::NOBOM )->viewFile( dirName, fileName );

	HACCEL hAccelTable = LoadAccelerators( FCS::inst().getFcInstance(), MAKEINTRESOURCE( IDA_COMMANDER_ACCEL ) );
	MSG msg;

	while( GetMessage( &msg, NULL, 0, 0 ) )
	{
		PrintDebug("0x%04X", msg.message);
			//PostQuitMessage( 0 );

		if( !TranslateAccelerator( msg.hwnd, hAccelTable, &msg )
		// send messages to currently active modeless dialog
		&& !( FCS::inst().getActiveDialog() && IsDialogMessage( FCS::inst().getActiveDialog(), &msg ) )
		// send messages to main Commander's controls
		&& !IsDialogMessage( FCS::inst().getFcWindow(), &msg ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
	PrintDebug("finished!");
	return true;
}

	//
	// File editor
	//
	void CPanelTab::fileEditorGui( const std::wstring& dirName, const std::wstring& fileName )
	{
		//CBaseDialog::createModeless<CFileEditor>( nullptr, StringUtils::NOBOM )->viewFile( dirName, fileName, _pParentPanel->getActiveTab() );
		auto upThread = std::make_unique<std::thread>( testThreadProc, dirName, fileName );
		upThread->detach();
	}

	//
	// Read items from ftp
	//
	bool CPanelTab::connectToFtpGui()
	{
		auto result = CBaseDialog::createModal<CFtpLogin>( _listViewItems.getHwnd() );
		if( !result.empty() )
			return changeDirectory( ReaderType::getModePrefix( EReadMode::Ftp ) + result );

		return false;
	}

	//
	// Read from ssh through putty
	//
	bool CPanelTab::connectPuttyGui()
	{
		auto result = CBaseDialog::createModal<CSshConnect>( _listViewItems.getHwnd() );
		if( !result.empty() )
		{
			if( _pSecureShell )
			{
				_pSecureShell->destroy();
				_pSecureShell = nullptr;
			}

			return changeDirectory( ReaderType::getModePrefix( EReadMode::Putty ) + result );
		}

		return false;
	}

	//
	// Select/deselect file items
	//
	void CPanelTab::markFileItemsGui( EFcCommand cmd )
	{
		bool mark = ( cmd == EFcCommand::MarkFiles );

		CTextInput::CParams params(
			mark ? L"Select" : L"Unselect",
			mark ? L"Select Files:" : L"Unselect Files:",
			L"*.*" );

		auto result = MiscUtils::showTextInputBox( params );

		// select/deselect files in active panel
		_listViewItems.markItems( PathUtils::preMatchWild( result ), mark );
	}

	//
	// Create new text file
	//
	bool CPanelTab::makeNewFileGui( const std::wstring& curPath )
	{
		CTextInput::CParams params(
			L"Edit New File",
			L"New File Name:",
			L"new.txt" );

		auto result = MiscUtils::showTextInputBox( params );
		if( !result.empty() )
		{
			std::wstring fileNameFull = PathIsRelative( result.c_str() ) ? curPath + result : result;

			// create new file in current dir when relative path given
			if( !FsUtils::makeNewFile( fileNameFull ) )
			{
				DWORD errorId = GetLastError();

				bool aborted = false;
				auto hr = ShellUtils::createItem( curPath, result, FILE_ATTRIBUTE_NORMAL, &aborted, FCS::inst().getFcWindow() );

				if( SUCCEEDED( hr ) && !aborted )
				{
					WIN32_FIND_DATA wfd = { 0 };

					if( !FsUtils::getFileInfo( fileNameFull, wfd ) )
					{
						std::wostringstream sstr;
						sstr << L"File: \"" << result << L"\"\r\n";
						sstr << SysUtils::getErrorMessage( errorId );

						MessageBox( FCS::inst().getFcWindow(), sstr.str().c_str(), L"Cannot Create New File", MB_ICONEXCLAMATION | MB_OK );
					}
				}
			}
			else
			{
				_focusedItemName = result;
				//return ShellUtils::executeCommand( L"notepad", curPath, result );
				CBaseDialog::createModeless<CFileEditor>( nullptr, StringUtils::UTF8 )->viewFile( curPath, result, _pParentPanel->getActiveTab() );
				return true;
			}
		}

		return false;
	}

	//
	// Try to change current directory
	//
	bool CPanelTab::changeDirectoryGui()
	{
		auto curDir = PathUtils::stripDelimiter( _dataManager.getCurrentDirectory() );

		CTextInput::CParams params( L"Change Directory", L"Change directory to", curDir );
		auto result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			return changeDirectoryFromText( result );
		}

		return false;
	}

	//
	// Show FC's configuration dialog
	//
	void CPanelTab::showConfigurationGui()
	{
		CBaseDialog::createModal<CMainConfiguration>( FCS::inst().getFcWindow() );
	}

	//
	// Show file/directory change attributes dialog
	//
	void CPanelTab::changeAttributesGui()
	{
		CBaseDialog::createModal<CChangeAttributes>( FCS::inst().getFcWindow() );
	}

	//
	// Show change case dialog
	//
	void CPanelTab::changeCaseGui()
	{
		CBaseDialog::createModal<CChangeCase>( FCS::inst().getFcWindow() );
	}

	//
	// Show split file dialog
	//
	void CPanelTab::splitFileGui()
	{
		CBaseDialog::createModeless<CSplitFile>( FCS::inst().getFcWindow() );
	}

	//
	// Show combine files dialog
	//
	void CPanelTab::combineFilesGui()
	{
		CBaseDialog::createModeless<CCombineFiles>( FCS::inst().getFcWindow() );
	}

	//
	// Show download file dialog
	//
	void CPanelTab::downloadFileGui()
	{
		CBaseDialog::createModeless<CHttpDownloadFile>();
	}

	//
	// Show calculate checksums dialog
	//
	void CPanelTab::calcChecksumsGui()
	{
		CBaseDialog::createModeless<CCalcChecksums>();
	}

	//
	// Show verify checksums dialog
	//
	void CPanelTab::verifyChecksumsGui()
	{
		CBaseDialog::createModeless<CVerifyChecksums>();
	}

	//
	// Show shared directories dialog
	//
	void CPanelTab::sharedDirsGui()
	{
		CBaseDialog::createModeless<CSharedDirectories>();
	}

	//
	// Un/Mark entry by name
	//
	void CPanelTab::markEntry( const std::wstring& itemName )
	{
		int index = _dataManager.getEntryIndex( itemName );
		auto& markedItems = _dataManager.getMarkedEntries();

		bool select = true;

		if( std::find( markedItems.begin(), markedItems.end(), index ) != markedItems.end() )
			select = false;

		_listViewItems.markItems( itemName, select );
	}

	//
	// Mark several entries at once
	//
	void CPanelTab::markEntries( const std::vector<int>& items )
	{
		_dataManager.setMarkedEntries( items );
		_listViewItems.refresh();
	}

	//
	// Create new directory
	//
	bool CPanelTab::makeDirectoryGui( const std::wstring& curPath )
	{
		CTextInput::CParams params( L"Create Directory", L"Directory Name:", L"New Directory" );
		auto result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			if( _dataManager.isInPuttyMode() ) {
				CBaseDialog::createModeless<CSshTransfer>()->transferFiles( EFcCommand::MakeDir, getSelectedItemsPathFull(), _dataManager.getMarkedEntriesSize(), result, _pParentPanel->getActiveTab() );
				return true;
			}
			if( _dataManager.isInFtpMode() ) {
				CBaseDialog::createModeless<CFtpTransfer>()->transferFiles( EFcCommand::MakeDir, getSelectedItemsPathFull(), _dataManager.getMarkedEntriesSize(), result, _pParentPanel->getActiveTab() );
				return true;
			}

			std::wstring dirNameFull = PathIsRelative( result.c_str() ) ? curPath + result : result;

			// create new dir in current dir when relative path given
			if( !FsUtils::makeDirectory( dirNameFull ) )
			{
				DWORD errorId = GetLastError();

				bool aborted = false;
				auto hr = ShellUtils::createItem( curPath, result, FILE_ATTRIBUTE_DIRECTORY, &aborted, FCS::inst().getFcWindow() );

				if( SUCCEEDED( hr ) && !aborted )
				{
					WIN32_FIND_DATA wfd = { 0 };

					if( !FsUtils::getFileInfo( dirNameFull, wfd ) )
					{
						std::wostringstream sstr;
						sstr << L"Path: \"" << result << L"\"\r\n";
						sstr << SysUtils::getErrorMessage( errorId );

						MessageBox( FCS::inst().getFcWindow(), sstr.str().c_str(), L"Cannot Create Directory", MB_ICONEXCLAMATION | MB_OK );
					}
				}
			}
			else
			{
				_focusedItemName = result;
				return true;
			}
		}

		return false;
	}

	//
	// Rename entry at given index
	//
	bool CPanelTab::renameEntry( int itemIndex, const std::wstring& fileName )
	{
		if( _dataManager.isInPuttyMode() ) {
			CBaseDialog::createModeless<CSshTransfer>()->transferFiles( EFcCommand::RenameItem, getSelectedItemsPathFull( false ), _dataManager.getMarkedEntriesSize(), fileName, _pParentPanel->getActiveTab() );
			return true;
		}
		if( _dataManager.isInFtpMode() ) {
			CBaseDialog::createModeless<CFtpTransfer>()->transferFiles( EFcCommand::RenameItem, getSelectedItemsPathFull( false ), _dataManager.getMarkedEntriesSize(), fileName, _pParentPanel->getActiveTab() );
			return true;
		}

		// try to rename file/directory on disk
		if( !_dataManager.renameEntry( itemIndex, fileName ) )
		{
			DWORD errorId = GetLastError();
			std::wostringstream sstr;
			sstr << L"Path: \"" << fileName << L"\"\r\n";
			sstr << SysUtils::getErrorMessage( errorId );

			MessageBox( FCS::inst().getFcWindow(), sstr.str().c_str(), L"Cannot Rename Item", MB_ICONEXCLAMATION | MB_OK );

			return false;
		}
		else
		{
			_focusedItemName = fileName;
			return true;
		}
	}

	//
	// Create archive (zip, 7z)
	//
	bool CPanelTab::createArchiveGui( const std::wstring& fileName )
	{
		std::wstring title = L"Pack";
		auto caption = getSelectedItemsString( _dataManager.getMarkedEntries(), title, false );

		auto entries = getSelectedItemsPathFull();
		auto tmpName = PathUtils::getParentDirectory( _dataManager.getCurrentDirectory() );

		if( entries.size() == 1 || tmpName.empty() )
		{
			tmpName = PathUtils::stripPath( entries[0] );

			if( _dataManager.getEntryType( _listViewItems.getFocusedItemIndex() ) == EEntryType::File )
				tmpName = PathUtils::stripFileExtension( tmpName );
		}

		tmpName += L".7z";

		CTextInput::CParams params( title, caption, tmpName, true, false, 400 );
		auto result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			auto archName = PathIsRelative( result.c_str() ) ? _dataManager.getCurrentDirectory() + result : result;
			CBaseDialog::createModeless<CFilePacker>()->packFiles( archName, this );
		}

		return false;
	}

	//
	// Extract archive (rar, zip, tar, gz, bz2, 7z, xz)
	//
	bool CPanelTab::extractArchiveGui( const std::wstring& archiveName, bool extractAll )
	{
		if( ArchiveType::isKnownType( archiveName ) )
		{
			std::wstring title = L"Extract", caption;

			if( extractAll )
				caption = L"Extract archive \"" + PathUtils::stripPath( archiveName ) + L"\" to:";
			else
				caption = getSelectedItemsString( _dataManager.getMarkedEntries(), title, false );

			CTextInput::CParams params( title, caption, getOppositeDir(), true, false, 400 );
			auto result = MiscUtils::showTextInputBox( params );

			if( !result.empty() )
			{
				auto outDir = PathIsRelative( result.c_str() ) ? getOppositeDir() + result : result;
				CBaseDialog::createModeless<CFileExtractor>()->extract( L"", PathUtils::addDelimiter( outDir ), _pParentPanel->getActiveTab() );
			}
		}

		return false;
	}

	//
	// Transfer files over ftp
	//
	bool CPanelTab::ftpTransferGui( EFcCommand cmd )
	{
		std::wstring title, text;
		bool isDelete = CFastCopyManager::getModeName( cmd, title, text ) == FastCopy::Mode::DELETE_MODE;
		std::wstring caption = getSelectedItemsString( _dataManager.getMarkedEntries(), title, isDelete );

		CTextInput::CParams params( title, caption, getOppositeDir(), !isDelete, false, 400 );
		std::wstring result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			CBaseDialog::createModeless<CFtpTransfer>()->transferFiles( cmd, getSelectedItemsPathFull(), _dataManager.getMarkedEntriesSize(), result, _pParentPanel->getActiveTab() );
		}

		return true;
	}

	//
	// Transfer files over ssh
	//
	bool CPanelTab::sshTransferGui( EFcCommand cmd )
	{
		std::wstring title, text;
		bool isDelete = CFastCopyManager::getModeName( cmd, title, text ) == FastCopy::Mode::DELETE_MODE;
		std::wstring caption = getSelectedItemsString( _dataManager.getMarkedEntries(), title, isDelete );

		CTextInput::CParams params( title, caption, getOppositeDir(), !isDelete, false, 400 );
		std::wstring result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			CBaseDialog::createModeless<CSshTransfer>()->transferFiles( cmd, getSelectedItemsPathFull(), _dataManager.getMarkedEntriesSize(), result, _pParentPanel->getActiveTab() );
		}

		return true;
	}

	//
	// Compare directories - find and mark duplicate files
	//
	void CPanelTab::compareDirectoriesGui()
	{
		CBaseDialog::createModal<CCompareDirectories>( FCS::inst().getFcWindow() );
	}

	//
	// Connect network drive dialog
	//
	void CPanelTab::connectNetworkDriveGui()
	{
		auto result = WNetConnectionDialog( FCS::inst().getFcWindow(), RESOURCETYPE_DISK );
		PrintDebug("WNetConnectionDialog: %d",result);
	}

	//
	// Disconnect network drive dialog
	//
	void CPanelTab::disconnectNetworkDriveGui()
	{
		auto result = WNetDisconnectDialog( FCS::inst().getFcWindow(), RESOURCETYPE_DISK );
		PrintDebug("WNetDisconnectDialog: %d",result);
	}

	//
	// Create disk image - only test function
	//
	bool CPanelTab::makeDiskImageGui( const std::wstring& curPath )
	{
		std::wstring text = L"Save Disk Image of \"" + curPath.substr(0, 2) + L"\" drive to:";
		CTextInput::CParams params( L"Create Disk Image", text, L"image.iso" );
		auto result = MiscUtils::showTextInputBox( params );

		if( !result.empty() )
		{
			// create image file in current dir when relative path given
			auto isoFile = PathIsRelative( result.c_str() ) ? getOppositeDir() + result : result;
			if( !FsUtils::createIsoFile( curPath, isoFile ) )
			{
				DWORD errorId = GetLastError();
				std::wostringstream sstr;
				sstr << L"Image file: \"" << result << L"\" from \"" + curPath.substr(0, 2) + L"\" drive\r\n";
				sstr << SysUtils::getErrorMessage( errorId );

				MessageBox( FCS::inst().getFcWindow(), sstr.str().c_str(), L"Cannot Create Image", MB_ICONEXCLAMATION | MB_OK );
			}
			else
			{
				std::wostringstream sstr;
				sstr << L"Image file: \"" << result << L"\" from \"" + curPath.substr(0, 2) + L"\" drive\r\n";
				sstr << L"Succesfully created!";

				MessageBox( FCS::inst().getFcWindow(), sstr.str().c_str(), L"Create Image", MB_ICONINFORMATION | MB_OK );
				return true;
			}
		}

		return false;
	}

	//
	// Try to find and focus .lnk shortcut target
	//
	bool CPanelTab::focusShortcutTarget( const std::wstring& itemName )
	{
		if( StringUtils::endsWith( itemName, L".lnk" ) )
		{
			auto target = ShellUtils::getShortcutTarget( itemName );
			return changeDirectory( PathUtils::stripFileName( target ), PathUtils::stripPath( target ) );
		}

		return false;
	}

	//
	// Navigate through directories history backwards/forward
	//
	void CPanelTab::navigateThroughHistory( EFcCommand cmd, int step )
	{
		bool canNavigate = false;

		_navigationHistory.updateCurrentEntry( _dataManager.getEntryName( _listViewItems.getFocusedItemIndex() ) );

		switch( cmd ) {
		case EFcCommand::HistoryBackward:
			canNavigate = _navigationHistory.goBackward( step );
			break;

		case EFcCommand::HistoryForward:
			canNavigate = _navigationHistory.goForward( step );
			break;
		}

		if( canNavigate )
		{
			auto entry = _navigationHistory.getCurrentEntry();
			changeDirectory( entry.first, entry.second, false );
		}
	}

	//
	// Update browsing history
	//
	void CPanelTab::updateBrowseHistory( const std::wstring& oldDirectory, const std::wstring& oldFocusedItem )
	{
		if( oldDirectory != _dataManager.getCurrentDirectory() )
		{
			_navigationHistory.updateCurrentEntry( oldFocusedItem );
			_navigationHistory.addEntry( std::make_pair( _dataManager.getCurrentDirectory(), _dataManager.getEntryName( _listViewItems.getFocusedItemIndex() ) ) );
		}
	}

	//
	// Change panel's current drive
	//
	bool CPanelTab::changeDrive( const std::wstring& driveName )
	{
		if( _dataManager.isInDiskMode() )
		{
			std::wstring oldDriveName = _dataManager.getRootPath();

			// save current directory in drive history
			auto it = _driveDirs.find( oldDriveName[0] );
			if( it != _driveDirs.end() )
				it->second = _dataManager.getCurrentDirectory();
			else
				_driveDirs[oldDriveName[0]] = _dataManager.getCurrentDirectory();
		}

		std::wstring dirName = driveName;

		// check for new letter in history
		auto it = _driveDirs.find( driveName[0] );
		if( it != _driveDirs.end() && it->second != dirName )
		{
			// check whether the directory actually exists - TODO: move to worker thread
			if( FsUtils::isPathDirectory( PathUtils::stripDelimiter( it->second ) ) )
				dirName = it->second;
		}

		changeDirectory( dirName );

		// set focus back to panel
		setPanelFocus();

		return true;
	}

	//
	// Change panel's current working directory
	//
	bool CPanelTab::changeDirectory( const std::wstring& dirName, const std::wstring& focusItemName, bool storeHistory )
	{
		KillTimer( _listViewItems.getHwnd(), FC_TIMER_OVERLAYS_ID );

		auto targetDir = dirName;

		PathUtils::unifyDelimiters( targetDir );

		_dirChangeTimerSet = _directoryChanged = _contentChanged = false;
		_focusedItemName = focusItemName;

		// when going upper level always focus superior directory
		if( PathUtils::isDirectoryDoubleDotted( PathUtils::getParentDirectory( targetDir ).c_str() ) )
		{
			_focusedItemName = PathUtils::getParentDirectory( _dataManager.getCurrentDirectory() );
			targetDir = PathUtils::stripFileName( PathUtils::stripFileName( targetDir ) );
		}

		std::wstring oldDirectory = _dataManager.getCurrentDirectory();
		std::wstring oldFocusedItem = _dataManager.getEntryName( _listViewItems.getFocusedItemIndex() );

		if( targetDir.empty() || ( _dataManager.isInDiskMode() && ReaderType::getRequestedMode( targetDir, true ) == EReadMode::Disk ) )
			targetDir = PathUtils::getFullPath( targetDir );

		// update panel's items
		bool retVal = directoryChanging( PathUtils::addDelimiter( targetDir ) );

		if( !retVal )
			_navigationHistory.revert();

		if( storeHistory && retVal )
		{
			updateBrowseHistory( oldDirectory, oldFocusedItem );
		}

		updateMenuAndToolbar();

		return retVal;
	}
}
