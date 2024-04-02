#include "stdafx.h"

#include "Commander.h"
#include "Panel.h"
#include "AboutBox.h"

namespace Commander
{
	//
	// Process particular command from Menu/Toolbar
	//
	bool processMenuCommand( WORD itemId )
	{
		auto leftTab = FCS::inst().getApp().getPanelLeft().getActiveTab();
		auto rightTab = FCS::inst().getApp().getPanelRight().getActiveTab();
		auto activeTab = FCS::inst().getApp().getActivePanel().getActiveTab();

		switch (itemId)
		{
		case IDM_GOTO_PARENTDIRECTORY:
			activeTab->processCommand( EFcCommand::GoUpper );
			break;
		case IDM_GOTO_BACK:
			activeTab->processCommand( EFcCommand::HistoryBackward );
			break;
		case IDM_GOTO_FORWARD:
			activeTab->processCommand( EFcCommand::HistoryForward );
			break;
		case IDM_LEFT: // nothing to do
			break;
		case IDM_LEFT_CHANGEDRIVE:
			FCS::inst().getApp().getPanelLeft().showDrivesList();
			break;
		case IDM_LEFT_NEWTAB:
			FCS::inst().getApp().getPanelLeft().createTab( ShellUtils::getKnownFolderPath( FOLDERID_Documents ) );
			break;
		case IDM_LEFT_CLOSETAB:
			FCS::inst().getApp().getPanelLeft().removeTab();
			break;
		case IDM_LEFT_GOTO: // nothing to do
			break;
		case IDM_LEFT_GOTO_BACK:
			leftTab->processCommand( EFcCommand::HistoryBackward );
			break;
		case IDM_LEFT_GOTO_FORWARD:
			leftTab->processCommand( EFcCommand::HistoryForward );
			break;
		case IDM_LEFT_GOTO_PARENTDIRECTORY:
			leftTab->processCommand( EFcCommand::GoUpper );
			break;
		case IDM_LEFT_GOTO_ROOTDIRECTORY:
			leftTab->processCommand( EFcCommand::GoRoot );
			break;
		case IDM_LEFT_GOTO_PATHOTHERPANEL:
			leftTab->changeDirectory( rightTab->getDataManager()->getCurrentDirectory() );
			break;
		case IDM_LEFT_BRIEF:
			leftTab->processCommand( EFcCommand::ViewModeBrief );
			break;
		case IDM_LEFT_DETAILED:
			leftTab->processCommand( EFcCommand::ViewModeDetailed );
			break;
		case IDM_LEFT_SORTBY: // nothing to do
			break;
		case IDM_LEFT_SORTBY_NAME:
			leftTab->processCommand( EFcCommand::SortByName );
			break;
		case IDM_LEFT_SORTBY_EXTENSION:
			leftTab->processCommand( EFcCommand::SortByExt );
			break;
		case IDM_LEFT_SORTBY_TIME:
			leftTab->processCommand( EFcCommand::SortByTime );
			break;
		case IDM_LEFT_SORTBY_SIZE:
			leftTab->processCommand( EFcCommand::SortBySize );
			break;
		case IDM_LEFT_SORTBY_ATTR:
			leftTab->processCommand( EFcCommand::SortByAttr );
			break;
		case IDM_LEFT_SWAP:
			leftTab->processCommand( EFcCommand::SwapPanels );
			break;
		case IDM_LEFT_REFRESH:
			leftTab->refresh();
			break;
		case IDM_FILES_QUICKRENAME:
			activeTab->processCommand( EFcCommand::RenameItem );
			break;
		case IDM_FILES_VIEW:
			activeTab->processCommand( EFcCommand::ViewFile );
			break;
		case IDM_FILES_ALTERNATEVIEW:
			activeTab->processCommand( EFcCommand::ViewFileHex );
			break;
		case IDM_FILES_EDIT:
			activeTab->processCommand( EFcCommand::EditFile );
			break;
		case IDM_FILES_EDITNEWFILE:
			activeTab->processCommand( EFcCommand::MakeFile );
			break;
		case IDM_FILES_COPY:
			activeTab->processCommand( EFcCommand::CopyItems );
			break;
		case IDM_FILES_MOVE:
			activeTab->processCommand( EFcCommand::MoveItems );
			break;
		case IDM_FILES_DELETE:
			activeTab->processCommand( EFcCommand::RecycleItems );
			break;
		case IDM_FILES_PROPERTIES:
			activeTab->processCommand( EFcCommand::ShowProperties );
			break;
		case IDM_FILES_CHANGEATTRIBUTES:
			activeTab->processCommand( EFcCommand::ChangeAttr );
			break;
		case IDM_FILES_CHANGECASE:
			activeTab->processCommand( EFcCommand::ChangeCase );
			break;
		case IDM_FILES_PACK:
			activeTab->processCommand( EFcCommand::PackItem );
			break;
		case IDM_FILES_UNPACK:
			activeTab->processCommand( EFcCommand::UnpackItem );
			break;
		case IDM_FILES_SPLIT:
			activeTab->processCommand( EFcCommand::SplitFile );
			break;
		case IDM_FILES_COMBINE:
			activeTab->processCommand( EFcCommand::CombineFile );
			break;
		case IDM_FILES_DOWNLOAD:
			activeTab->processCommand( EFcCommand::DownloadFile );
			break;
		case IDM_FILES_CALCULATECHECKSUMS:
			activeTab->processCommand( EFcCommand::ChecksumCalc );
			break;
		case IDM_FILES_VERIFYCHECKSUMS:
			activeTab->processCommand( EFcCommand::ChecksumVerify );
			break;
		case IDM_EDIT: // nothing to do
			break;
		case IDM_EDIT_CUT:
			activeTab->processCommand( EFcCommand::ClipCut );
			break;
		case IDM_EDIT_COPY:
			activeTab->processCommand( EFcCommand::ClipCopy );
			break;
		case IDM_EDIT_PASTE:
			activeTab->processCommand( EFcCommand::ClipPaste );
			break;
		case IDM_EDIT_COPYPATHFULL:
			activeTab->processCommand( EFcCommand::ClipCopyPathFull );
			break;
		case IDM_EDIT_COPYNAMEASTEXT:
			activeTab->processCommand( EFcCommand::ClipCopyName );
			break;
		case IDM_EDIT_COPYPATHASTEXT:
			activeTab->processCommand( EFcCommand::ClipCopyPath );
			break;
		case IDM_EDIT_SELECT:
			activeTab->processCommand( EFcCommand::MarkFiles );
			break;
		case IDM_EDIT_UNSELECT:
			activeTab->processCommand( EFcCommand::UnmarkFiles );
			break;
		case IDM_EDIT_INVERTSELECTION:
			activeTab->processCommand( EFcCommand::InvertSelectionFiles );
			break;
		case IDM_EDIT_SELECTALL:
			activeTab->processCommand( EFcCommand::SelectAll );
			break;
		case IDM_EDIT_UNSELECTALL:
			activeTab->processCommand( EFcCommand::UnselectAll );
			break;
		case IDM_COMMANDS: // nothing to do
			break;
		case IDM_COMMANDS_CREATEDIRECTORY:
			activeTab->processCommand( EFcCommand::MakeDir );
			break;
		case IDM_COMMANDS_CHANGEDIRECTORY:
			activeTab->processCommand( EFcCommand::ChangeDir );
			break;
		case IDM_COMMANDS_COMPAREDIRECTORIES:
			activeTab->processCommand( EFcCommand::CompareDirs );
			break;
		case IDM_COMMANDS_COMPAREFILES:
			activeTab->processCommand( EFcCommand::CompareFiles );
			break;
		case IDM_COMMANDS_FINDFILES:
			activeTab->processCommand( EFcCommand::FindFiles );
			break;
		case IDM_COMMANDS_MAKEFILELIST:
			activeTab->processCommand( EFcCommand::MakeFileList );
			break;
		case IDM_COMMANDS_GOTOSHORTCUTTARGET:
			activeTab->processCommand( EFcCommand::GoShortcutTarget );
			break;
		case IDM_COMMANDS_CONNECTNETDRIVE:
			activeTab->processCommand( EFcCommand::ConnectNetDrive );
			break;
		case IDM_COMMANDS_DISCONNECTNETDRIVE:
			activeTab->processCommand( EFcCommand::DisconnectNetDrive );
			break;
		case IDM_COMMANDS_SHAREDDIRECTORIES:
			activeTab->processCommand( EFcCommand::ShowSharedDirs );
			break;
		case IDM_COMMANDS_COMMANDSHELL:
			activeTab->processCommand( EFcCommand::ShowCmdPrompt );
			break;
		case IDM_COMMANDS_OPENFOLDER: // nothing to do
			break;
		case IDM_OPENFOLDER_ACTIVEFOLDER:
			activeTab->processCommand( EFcCommand::OpenFolderActive );
			break;
		case IDM_OPENFOLDER_DESKTOP:
			activeTab->processCommand( EFcCommand::OpenFolderDesktop );
			break;
		case IDM_OPENFOLDER_COMPUTER:
			activeTab->processCommand( EFcCommand::OpenFolderComputer );
			break;
		case IDM_OPENFOLDER_CONTROLPANEL:
			activeTab->processCommand( EFcCommand::OpenFolderControlPanel );
			break;
		case IDM_OPENFOLDER_NETWORK:
			activeTab->processCommand( EFcCommand::OpenFolderNetwork );
			break;
		case IDM_OPENFOLDER_RECYCLEBIN:
			activeTab->processCommand( EFcCommand::OpenFolderRecycleBin );
			break;
		case IDM_OPENFOLDER_FONTS:
			activeTab->processCommand( EFcCommand::OpenFolderFonts );
			break;
		case IDM_OPENFOLDER_PRINTERS:
			activeTab->processCommand( EFcCommand::OpenFolderPrinters );
			break;
		case IDM_OPENFOLDER_MYDOCUMENTS:
			activeTab->processCommand( EFcCommand::OpenFolderMyDocuments );
			break;
		case IDM_OPTIONS: // nothing to do
			break;
		case IDM_OPTIONS_CONFIGURATION:
			activeTab->processCommand( EFcCommand::ShowConfiguration );
			break;
		case IDM_OPTIONS_ALWAYSONTOP:
			activeTab->processCommand( EFcCommand::SetAlwaysOnTop );
			break;
		case IDM_RIGHT: // nothing to do
			break;
		case IDM_RIGHT_CHANGEDRIVE:
			FCS::inst().getApp().getPanelRight().showDrivesList();
			break;
		case IDM_RIGHT_NEWTAB:
			FCS::inst().getApp().getPanelRight().createTab( ShellUtils::getKnownFolderPath( FOLDERID_Documents ) );
			break;
		case IDM_RIGHT_CLOSETAB:
			FCS::inst().getApp().getPanelRight().removeTab();
			break;
		case IDM_RIGHT_GOTO: // nothing to do
			break;
		case IDM_RIGHT_GOTO_BACK:
			rightTab->processCommand( EFcCommand::HistoryBackward );
			break;
		case IDM_RIGHT_GOTO_FORWARD:
			rightTab->processCommand( EFcCommand::HistoryForward );
			break;
		case IDM_RIGHT_GOTO_PARENTDIRECTORY:
			rightTab->processCommand( EFcCommand::GoUpper );
			break;
		case IDM_RIGHT_GOTO_ROOTDIRECTORY:
			rightTab->processCommand( EFcCommand::GoRoot );
			break;
		case IDM_RIGHT_GOTO_PATHOTHERPANEL:
			rightTab->changeDirectory( leftTab->getDataManager()->getCurrentDirectory() );
			break;
		case IDM_RIGHT_BRIEF:
			rightTab->processCommand( EFcCommand::ViewModeBrief );
			break;
		case IDM_RIGHT_DETAILED:
			rightTab->processCommand( EFcCommand::ViewModeDetailed );
			break;
		case IDM_RIGHT_SORTBY: // nothing to do
			break;
		case IDM_RIGHT_SORTBY_NAME:
			rightTab->processCommand( EFcCommand::SortByName );
			break;
		case IDM_RIGHT_SORTBY_EXTENSION:
			rightTab->processCommand( EFcCommand::SortByExt );
			break;
		case IDM_RIGHT_SORTBY_TIME:
			rightTab->processCommand( EFcCommand::SortByTime );
			break;
		case IDM_RIGHT_SORTBY_SIZE:
			rightTab->processCommand( EFcCommand::SortBySize );
			break;
		case IDM_RIGHT_SORTBY_ATTR:
			rightTab->processCommand( EFcCommand::SortByAttr );
			break;
		case IDM_RIGHT_SWAP:
			rightTab->processCommand( EFcCommand::SwapPanels );
			break;
		case IDM_RIGHT_REFRESH:
			rightTab->refresh();
			break;

		case IDM_HELP_ABOUT:
			CBaseDialog::createModal<CAboutBox>( FCS::inst().getFcWindow() );
			break;

		case IDM_FILES_EXIT:
			SendMessage( FCS::inst().getFcWindow(), WM_CLOSE, 0, 0 );
			break;

		default:
			return false;
		}
		return true;
	}
}
