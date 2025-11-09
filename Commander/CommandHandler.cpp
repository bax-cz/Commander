#include "stdafx.h"

#include "Commander.h"
#include "PanelTab.h"

#include "MenuUtils.h"
#include "MiscUtils.h"

namespace Commander
{
	//
	// Process particular command
	//
	bool CPanelTab::processCommand( const EFcCommand cmd )
	{
		// check if a command can be executed right now
		if( _availableCommands[cmd] == false )
			return false;

		bool retVal = false;

		int focusedItem = _listViewItems.getFocusedItemIndex();

		std::wstring itemName = _dataManager.getEntryName( focusedItem );
		std::wstring path = _dataManager.getCurrentDirectory();

		switch( cmd )
		{
		case EFcCommand::SetAlwaysOnTop:
			FCS::inst().getApp().setAlwaysOnTop();
			break;

		case EFcCommand::ExecuteItem:
			retVal = processActionOnItem( itemName, path, focusedItem );
			break;

		case EFcCommand::PackItem:
			retVal = createArchiveGui( itemName );
			break;

		case EFcCommand::UnpackItem:
			retVal = extractArchiveGui( path + itemName );
			break;

		case EFcCommand::RepackItem:
			retVal = extractArchiveGui( path + itemName, true );
			break;

		case EFcCommand::RenameItem:
			LvUtils::editItemLabel( _listViewItems.getHwnd(), focusedItem );
			break;

		case EFcCommand::GoUpper:
			retVal = changeDirectory( path + PathUtils::getDoubleDottedDirectory() );
			break;

		case EFcCommand::GoRoot:
			retVal = changeDirectory( _dataManager.getRootPath() );
			break;

		case EFcCommand::GoFtp:
			connectToFtpGui();
			break;

		case EFcCommand::GoPutty:
			connectPuttyGui();
			break;

		case EFcCommand::GoNetwork:
			retVal = changeDirectory( ReaderType::getModePrefix( EReadMode::Network ) );
			break;

		case EFcCommand::GoReged:
			retVal = changeDirectory( ReaderType::getModePrefix( EReadMode::Reged ) );
			break;

		case EFcCommand::GoPathOtherPanel:
			retVal = changeDirectory( getOppositeDir() );
			break;

		case EFcCommand::GoShortcutTarget:
			retVal = focusShortcutTarget( path + itemName );
			break;

		case EFcCommand::SwapPanels:
			FCS::inst().getApp().swapPanels();
			break;

		case EFcCommand::FindFiles:
			findFilesGui();
			break;

		case EFcCommand::FindDuplicates:
			findDuplicatesGui();
			break;

		case EFcCommand::ViewFile:
		case EFcCommand::ViewFileHex:
			fileViewerGui( path, itemName, cmd == EFcCommand::ViewFileHex );
			break;

		case EFcCommand::EditFile:
			//retVal = ShellUtils::executeCommand( L"notepad", path, itemName );
			fileEditorGui( path, itemName );
			break;

		case EFcCommand::SplitFile:
			splitFileGui();
			break;

		case EFcCommand::CombineFile:
			combineFilesGui();
			break;

		case EFcCommand::DownloadFile:
			downloadFileGui();
			break;

		case EFcCommand::ChecksumCalc:
			calcChecksumsGui();
			break;

		case EFcCommand::ChecksumVerify:
			verifyChecksumsGui();
			break;

		case EFcCommand::ChangeDir:
			changeDirectoryGui();
			break;

		case EFcCommand::ChangeAttr:
			changeAttributesGui();
			break;

		case EFcCommand::ChangeCase:
			changeCaseGui();
			break;

		case EFcCommand::ClipCopyName:
			MiscUtils::setClipboardText( FCS::inst().getFcWindow(), itemName );
			break;

		case EFcCommand::ClipCopyPath:
			MiscUtils::setClipboardText( FCS::inst().getFcWindow(), path );
			break;

		case EFcCommand::ClipCopyPathFull:
			MiscUtils::setClipboardText( FCS::inst().getFcWindow(), path + itemName );
			break;

		case EFcCommand::ShowProperties:
			ShellUtils::invokeCommand( "properties", getSelectedItemsPathFull() );
			break;

		case EFcCommand::ShowCmdPrompt:
			showCommandPromptGui( path );
			break;

		case EFcCommand::ShowConfiguration:
			showConfigurationGui();
			break;

		case EFcCommand::ShowSharedDirs:
			sharedDirsGui();
			break;

		case EFcCommand::MarkFiles:
		case EFcCommand::UnmarkFiles:
			markFileItemsGui( cmd );
			break;

		case EFcCommand::SelectAll:
		case EFcCommand::UnselectAll:
			_listViewItems.markItems( L"*", ( cmd == EFcCommand::SelectAll ), false );
			break;

		case EFcCommand::SelectSameExt:
		case EFcCommand::UnselectSameExt:
			_listViewItems.markItems( std::wstring( L"*." ) + PathUtils::getFileNameExtension( itemName ), ( cmd == EFcCommand::SelectSameExt ) );
			break;

		case EFcCommand::SelectSameName:
		case EFcCommand::UnselectSameName:
			_listViewItems.markItems( PathUtils::stripFileExtension( itemName ) + L".*", ( cmd == EFcCommand::SelectSameName ) );
			break;

		case EFcCommand::InvertSelectionFiles:
			_listViewItems.invertMarkedItems();
			break;

		case EFcCommand::InvertSelectionAll:
			_listViewItems.invertMarkedItems( false );
			break;

		case EFcCommand::MakeDir:
			retVal = makeDirectoryGui( path );
			break;

		case EFcCommand::MakeFile:
			retVal = makeNewFileGui( path );
			break;

		case EFcCommand::MakeFileList:
			MiscUtils::setClipboardText( FCS::inst().getFcWindow(), StringUtils::join( getSelectedItemsPathFull(), L"\r\n" ) );
			break;

		case EFcCommand::HistoryForward:
		case EFcCommand::HistoryBackward:
			navigateThroughHistory( cmd );
			break;

		case EFcCommand::OpenTab:
		case EFcCommand::OpenTabOpposite:
			processOpenTab( cmd, itemName, path, focusedItem );
			break;

		case EFcCommand::PrevTab:
		case EFcCommand::NextTab:
			_pParentPanel->switchTab( cmd );
			break;

		case EFcCommand::CloseTab:
			_pParentPanel->removeTab();
			break;

		case EFcCommand::ClipCut:
		case EFcCommand::ClipCopy:
		case EFcCommand::ClipPaste:
			processClipboard( cmd, itemName, path );
			break;

		case EFcCommand::ViewModeBrief:
		case EFcCommand::ViewModeDetailed:
			changeViewMode( cmd );
			break;

		case EFcCommand::SortByName:
		case EFcCommand::SortByExt:
		case EFcCommand::SortByTime:
		case EFcCommand::SortBySize:
		case EFcCommand::SortByAttr:
			processSorting( cmd, itemName, path );
			break;

		case EFcCommand::CopyItems:
		case EFcCommand::MoveItems:
		case EFcCommand::RecycleItems:
		case EFcCommand::DeleteItems:
			processFileOperations( cmd );
			break;

		case EFcCommand::CompareDirs:
			compareDirectoriesGui();
			break;

		case EFcCommand::CompareFiles:
			compareFiles();
			break;

		case EFcCommand::ConnectNetDrive:
			connectNetworkDriveGui();
			break;

		case EFcCommand::DisconnectNetDrive:
			disconnectNetworkDriveGui();
			break;

		case EFcCommand::OpenFolderActive:
			if( PathUtils::isDirectoryDoubleDotted( itemName.c_str() ) ) ShellUtils::executeFile( path, L"" );
			else ShellUtils::selectItemsInExplorer( path, getSelectedItemsPathFull() );
			break;

		case EFcCommand::OpenFolderDesktop:
			ShellUtils::executeCommand( L"explorer", path, L"shell:Desktop" );
			break;

		case EFcCommand::OpenFolderComputer:
			ShellUtils::executeCommand( L"explorer", path, L"shell:MyComputerFolder" );
			break;

		case EFcCommand::OpenFolderControlPanel:
			/*ShellUtils::executeCommand( L"control", L"access.cpl" );*/
			ShellUtils::executeCommand( L"explorer", path, L"shell:ControlPanelFolder" );
			break;

		case EFcCommand::OpenFolderNetwork:
			ShellUtils::executeCommand( L"explorer", path, L"shell:NetworkPlacesFolder" );
			break;

		case EFcCommand::OpenFolderRecycleBin:
			ShellUtils::executeCommand( L"explorer", path, L"shell:RecycleBinFolder" );
			break;

		case EFcCommand::OpenFolderFonts:
			ShellUtils::executeCommand( L"explorer", path, L"shell:Fonts" );
			break;

		case EFcCommand::OpenFolderPrinters:
			ShellUtils::executeCommand( L"explorer", path, L"shell:PrintersFolder" );
			break;

		case EFcCommand::OpenFolderMyDocuments:
			ShellUtils::executeCommand( L"explorer", path, L"shell:Personal" );
			break;

		case EFcCommand::CreateIso:
			retVal = makeDiskImageGui( path );
			break;

		default:
			break;
		}

		return retVal;
	}
}
