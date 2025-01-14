#include "stdafx.h"

#include <algorithm>
#include <windowsx.h>

#include "Commander.h"
#include "ControlToolbar.h"
#include "Commands.h"

#include "FileSystemUtils.h"
#include "StringUtils.h"
#include "ShellUtils.h"
#include "MiscUtils.h"
#include "IconUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CListView::CListView()
		: _dataMan( nullptr )
		, _cRef( 1 )
		, _hWndEditQuickFind( nullptr )
		, _isQuickFindMode( false )
		, _focusedItemIndex( -1 )
		, _prevFocusedItemIndex( -1 )
		, _markedItemsChanged( false )
		, _directoryChanged( true )
		, _currentViewMode( EViewMode::Brief )
	{
	}

	CListView::~CListView()
	{
		// remove this handle from map
	//	FCS::inst().getApp().getControls().erase( _hWnd );
		PrintDebug("hw: 0x%08X, size: %zu", _hWnd, FCS::inst().getApp().getControls().size());
		SetWindowLongPtr( _hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( _wndProcDefault ) );
		DestroyWindow( _hWnd );
	}


	//
	// Create list-view control and intialize it
	//
	bool CListView::init( CFcPanel *const panel, CPanelTab *const panelTab )
	{
		// create list-view control at place given by rct parameter
		_hWnd = CreateWindowEx(
			WS_EX_CLIENTEDGE,
			WC_LISTVIEW,
			L"",
			WS_CHILD | WS_HSCROLL | WS_VISIBLE | WS_TABSTOP | LVS_LIST | LVS_EDITLABELS | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHAREIMAGELISTS,
			_rect.left, _rect.top, _rect.right - _rect.left, _rect.bottom - _rect.top,
			FCS::inst().getFcWindow(),
			NULL,
			FCS::inst().getFcInstance(),
			NULL );

		// register this window procedure
		registerWndProc( panel, panelTab, this );

		// set font for list-view items
		SendMessage( _hWnd, WM_SETFONT, reinterpret_cast<WPARAM>( FCS::inst().getApp().getGuiFont() ), TRUE );

		// enable list-view double-buffering
		LvUtils::setExtStyle( _hWnd, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );

		// prepare list-view columns (detailed mode)
		LvUtils::addColumn( _hWnd, L"File" );
		LvUtils::addColumn( _hWnd, L"Size", 64, false );
		LvUtils::addColumn( _hWnd, L"Date", 68, false );
		LvUtils::addColumn( _hWnd, L"Time", 56, false );
		LvUtils::addColumn( _hWnd, L"Attr", 36, false );

		// hide focus rectangle - not reliable, solved within custom draw function
		SendMessage( _hWnd, WM_CHANGEUISTATE, MAKEWPARAM( UIS_SET, UISF_HIDEFOCUS ), 0 );

		// store data manager to make things little easier
		_dataMan = _pParentTab->getDataManager();

		// init directory watcher and background workers to send notifications to this window
		_dataMan->init( _hWnd );

		// tell windows that we can handle drag and drop
		InitializeDragDropHelper( _hWnd, DROPIMAGE_MOVE, L"%1" );

		return _hWnd != NULL;
	}

	//
	// Update items in list-view, select focusItemName if set
	//
	void CListView::updateItems( const std::wstring& focusItemName )
	{
		_cutItems.clear();
		_dataMan->getMarkedEntries().clear();
		_markedItemsChanged = true;
		_directoryChanged = _dataMan->getCurrentDirectory() != _controlUserData;

		// update drop location name for drag & drop
		if( _directoryChanged )
		{
			_controlUserData = _dataMan->getCurrentDirectory();
			_dropLocationText = PathUtils::getParentDirectory( _controlUserData );

			if( _dropLocationText.empty() )
				_dropLocationText = _controlUserData;

			LvUtils::cleanUp( _hWnd );

			_focusedItemIndex = 0;
		}

		// do not invalidate list-view when reading data
		if( _dataMan->getStatus() == EReaderStatus::ReadingData ) {
		//	PrintDebug("reading data..");
			LvUtils::setItemCount( _hWnd, _dataMan->getEntryCount(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL );
		//	Sleep(500);
			_prevFocusedItemIndex = _focusedItemIndex;
		}
		else {
		//	PrintDebug("data ok");
			LvUtils::setItemCount( _hWnd, _dataMan->getEntryCount(), 0 );
		//	Sleep(500);
			updateFocusedItem( focusItemName );
			updateColumnWidth(); // resize column according to its longest item text
		}

		// always focus first item after changing directory
		updateProperties();

		LvUtils::selectItem( _hWnd, _focusedItemIndex );
		LvUtils::ensureVisible( _hWnd, _focusedItemIndex, true );
	}

	//
	// Find and/or try to select previously focused item
	//
	void CListView::updateFocusedItem( const std::wstring& itemName )
	{
		if( !itemName.empty() )
		{
			int index = _dataMan->getEntryIndex( itemName );
			_focusedItemIndex = ( index != -1 ) ? index : _focusedItemIndex;
		}

		int itemsCount = static_cast<int>( _dataMan->getEntryCount() );

		if( _focusedItemIndex == -1 )
			_focusedItemIndex = _prevFocusedItemIndex;

		if( _focusedItemIndex >= itemsCount )
		{
			_focusedItemIndex = itemsCount - 1;
		}
		else if( _focusedItemIndex == -1 && itemsCount )
		{
			_focusedItemIndex = 0;
		}
	}

	//
	// Resize list-view column according to its longest item string
	//
	void CListView::updateColumnWidth()
	{
		if( _dataMan->getEntryCount() > 1 )
		/*int itemCount = static_cast<int>( _dataMan->getEntryCount() );
		int widthMax = 0, width = 0;

		for( int i = 0; i < itemCount; i++ )
		{
			width = LvUtils::getStringWidth( _hWnd, _dataMan->getEntryName( i ) );
			widthMax = ( width > widthMax ) ? width : widthMax;
		}

		LvUtils::setColumnWidth( _hWnd, widthMax ); // TODO - calc and add padding
		*/
		ListView_SetColumnWidth( _hWnd, 0, LVSCW_AUTOSIZE );
	}

	//
	// Update properties text
	//
	void CListView::updateProperties()
	{
		if( !_dataMan->getMarkedEntries().empty() )
		{
			if( _markedItemsChanged )
			{
				_markedItemsChanged = false;
				_dataMan->updateProperties( _focusedItemIndex );
			}
		}
		else
			_dataMan->updateProperties( _focusedItemIndex );
	}

	//
	// Check whether item is marked
	//
	bool CListView::isItemMarked( int itemIndex )
	{
		auto& markedItems = _dataMan->getMarkedEntries();
		return std::find( markedItems.begin(), markedItems.end(), itemIndex ) != markedItems.end();
	}

	//
	// Check whether item is marked
	//
	bool CListView::isItemCut( int itemIndex )
	{
		return std::find( _cutItems.begin(), _cutItems.end(), itemIndex ) != _cutItems.end();
	}

	//
	// Select/deselect file items
	//
	void CListView::markItems( const std::wstring& mask, bool select, bool filesOnly )
	{
		auto& markedItems = _dataMan->getMarkedEntries();
		_markedItemsChanged = true;

		std::wstring masklwr = StringUtils::convert2Lwr( mask );

		int cnt = static_cast<int>( _dataMan->getEntryCount() );
		for( int i = 0; i < cnt; ++i )
		{
			std::wstring itemNamelwr = StringUtils::convert2Lwr( _dataMan->getEntryName( i ) );

			if( i == 0 && PathUtils::isDirectoryDoubleDotted( itemNamelwr.c_str() ) )
				continue; // skip double-dotted directory

			if( !filesOnly || _dataMan->isEntryFile( i ) )
			{
				auto it = std::find( markedItems.begin(), markedItems.end(), i );
				if( select && it == markedItems.end() )
				{
					if( PathUtils::matchFileNameWild( masklwr, itemNamelwr ) )
						markedItems.push_back( i );
				}
				else if( !select && it != markedItems.end() )
				{
					if( PathUtils::matchFileNameWild( masklwr, itemNamelwr ) )
						markedItems.erase( it );
				}
			}
		}

		refresh();
	}

	//
	// Set cut appearance to selected icons
	//
	void CListView::cutSelectedItems()
	{
		_cutItems.clear();

		if( !_dataMan->getMarkedEntries().empty() )
		{
			_cutItems = _dataMan->getMarkedEntries();
		}
		else
		{
			_cutItems.push_back( _focusedItemIndex );
		}

		InvalidateRect( _hWnd, NULL, true );
	}

	//
	// Selection in list-view has been changed
	//
	void CListView::onSelectionChanged( LPNMLISTVIEW lvData )
	{
		if( lvData->uNewState )
		{
			int idx = LvUtils::getFocusedItem( _hWnd );

			_focusedItemIndex = ( idx != -1 ) ? idx : _focusedItemIndex;
			_focusedItemName = _dataMan->getEntryName( _focusedItemIndex );

			updateProperties();

			_pParentTab->selectionChanged();
		}
	}

	//
	// Column header clicked - do sorting
	//
	void CListView::onColumnClick( LPNMLISTVIEW lvData )
	{
		switch( lvData->iSubItem )
		{
		case 0:
			_pParentTab->processCommand( EFcCommand::SortByName );
			break;
		case 1:
			_pParentTab->processCommand( EFcCommand::SortBySize );
			break;
		case 2:
		case 3:
			_pParentTab->processCommand( EFcCommand::SortByTime );
			break;
		case 4:
			_pParentTab->processCommand( EFcCommand::SortByAttr );
			break;
		}
	}

	//
	// Begin item's label editing
	//
	bool CListView::onBeginLabelEdit( NMLVDISPINFO *lvDispInfo )
	{
		if( !_dataMan->isEntryFixed( lvDispInfo->item.iItem ) && !_dataMan->isInReadOnlyMode() )
		{
			if( _dataMan-> isEntryFile( lvDispInfo->item.iItem ) )
			{
				auto extIndex = PathUtils::getFileNameExtensionIndex( _dataMan->getEntryName( lvDispInfo->item.iItem ) );
				SendMessage( ListView_GetEditControl( _hWnd ), EM_SETSEL, static_cast<WPARAM>( 0 ), static_cast<LPARAM>( extIndex ) );
			}

			return false;
		}

		return true;
	}

	//
	// End item's label editing
	//
	bool CListView::onEndLabelEdit( NMLVDISPINFO *lvDispInfo )
	{
		if( lvDispInfo->item.pszText != NULL )
		{
			// try to rename file/directory on disk
			return _pParentTab->renameEntry( lvDispInfo->item.iItem, lvDispInfo->item.pszText );
		}

		return false;
	}

	//
	// Force list-view repaint
	//
	void CListView::refresh()
	{
		// a hack to force items to repaint
		InvalidateRect( _hWnd, NULL, true );
		LvUtils::unselectItem( _hWnd, _focusedItemIndex );
		LvUtils::selectItem( _hWnd, _focusedItemIndex );
	}

	//
	// Mark or unmark focused item and select next one
	//
	void CListView::markFocusedItem()
	{
		auto& markedItems = _dataMan->getMarkedEntries();
		_markedItemsChanged = true;

		LvUtils::selectItem( _hWnd, _focusedItemIndex );

		auto it = std::find( markedItems.begin(), markedItems.end(), _focusedItemIndex );
		if( it == markedItems.end() )
		{
			if( _focusedItemIndex == 0 )
			{
				std::wstring itemName = _dataMan->getEntryName( _focusedItemIndex );
				if( !PathUtils::isDirectoryDoubleDotted( itemName.c_str() ) )
				{
					markedItems.push_back( _focusedItemIndex );
				}
				else
					_markedItemsChanged = false;
			}
			else
			{
				markedItems.push_back( _focusedItemIndex );
			}
		}
		else
		{
			markedItems.erase( it );
		}

		if( _isQuickFindMode || _focusedItemIndex == static_cast<int>( _dataMan->getEntryCount() ) - 1 )
		{
			// a hack to force last item to repaint
			LvUtils::unselectItem( _hWnd, _focusedItemIndex );
			LvUtils::selectItem( _hWnd, _focusedItemIndex );
		}
		else
			LvUtils::selectItem( _hWnd, _focusedItemIndex + 1 );

		LvUtils::ensureVisible( _hWnd, _focusedItemIndex );
	}

	//
	// Invert marked items
	//
	void CListView::invertMarkedItems( bool filesOnly )
	{
		auto& markedItems = _dataMan->getMarkedEntries();
		_markedItemsChanged = true;

		int cnt = static_cast<int>( _dataMan->getEntryCount() );
		for( int i = 0; i < cnt; ++i )
		{
			if( i == 0 && !filesOnly && PathUtils::isDirectoryDoubleDotted( _dataMan->getEntryName( i ) ) )
				continue; // skip double-dotted directory

			if( !filesOnly || _dataMan->isEntryFile( i ) )
			{
				auto it = std::find( markedItems.begin(), markedItems.end(), i );
				if( it != markedItems.end() )
					markedItems.erase( it );
				else
					markedItems.push_back( i );
			}
		}

		refresh();
	}

	//
	// Get item's color according to its attributes
	//
	COLORREF CListView::getItemColor( int itemIndex )
	{
		if( isItemMarked( itemIndex ) )
		{
			// show item as marked
			return FC_COLOR_MARKED;
		}
		else if( _dataMan->isEntryAttrSet( itemIndex, FILE_ATTRIBUTE_COMPRESSED ) )
		{
			// show item as a compressed item
			return FC_COLOR_COMPRESSED;
		}
		else if( _dataMan->isEntryAttrSet( itemIndex, FILE_ATTRIBUTE_ENCRYPTED ) )
		{
			// show item as an encrypted item
			return FC_COLOR_ENCRYPTED;
		}
		else if( _dataMan->isEntryHardLink( itemIndex ) )
		{
			// show item as a hard link
			return FC_COLOR_HARDLINK;
		}
		else if( _dataMan->isEntryAttrSet( itemIndex, FILE_ATTRIBUTE_HIDDEN ) )
		{
			// show item as a hidden item
			return FC_COLOR_HIDDEN;
		}

		return 0;
	}

	//
	// Custom draw list-view control
	//
	LRESULT CListView::onCustomDraw( LPNMLVCUSTOMDRAW lvcd )
	{
		switch( lvcd->nmcd.dwDrawStage ) 
		{
		case CDDS_PREPAINT:
			return CDRF_DODEFAULT | CDRF_NOTIFYITEMDRAW;

		case ( CDDS_ITEM | CDDS_PREPAINT ):
			if( isInBriefMode() )
			{
				auto itemIndex = static_cast<int>( lvcd->nmcd.dwItemSpec );
				if( !itemIndex && PathUtils::isDirectoryDoubleDotted( _dataMan->getEntryName( itemIndex ) ) )
				{
					// draw upper directory icon
					RECT rctIcon;
					LvUtils::getItemRect( _hWnd, itemIndex, &rctIcon, LVIR_ICON );
					IconUtils::drawIconSmall( lvcd->nmcd.hdc, IDI_PARENTDIR, &rctIcon );

					if( itemIndex == _focusedItemIndex && FCS::inst().getFcWindow() == GetForegroundWindow() && _hWnd == GetFocus() )
					{
						RECT rctLbl;
						LvUtils::getItemRect( _hWnd, itemIndex, &rctLbl, LVIR_LABEL );

						// draw filled selection rectangle
						IconUtils::drawFocusRectangle( lvcd->nmcd.hdc, rctLbl );
					}
				//	PrintDebug("CDDS_PREPAINT, %d(%zu) - %d", _focusedItemIndex, _dataMan->getEntryCount(), _dataMan->getStatus());
				//	Sleep(100);
					return CDRF_SKIPDEFAULT;
				}

				if( LvUtils::getSelectedItem( _hWnd ) == itemIndex )
				{
					// clear item's selected and focused state to prevent drawing default selection rectangle
					lvcd->nmcd.uItemState &= ~( CDIS_SELECTED | CDIS_FOCUS );
				}

				lvcd->clrText = getItemColor( itemIndex );

			//	Sleep(100);
				return CDRF_NOTIFYPOSTPAINT;
			}

			return CDRF_DODEFAULT | CDRF_NOTIFYSUBITEMDRAW;

		case ( CDDS_ITEM | CDDS_SUBITEM | CDDS_PREPAINT ):
			{
				auto itemIndex = static_cast<int>( lvcd->nmcd.dwItemSpec );
				if( !itemIndex && PathUtils::isDirectoryDoubleDotted( _dataMan->getEntryName( itemIndex ) ) && lvcd->iSubItem == 0 )
				{
					// draw upper directory icon
					RECT rctIcon;
					LvUtils::getItemRect( _hWnd, itemIndex, &rctIcon, LVIR_ICON );
					IconUtils::drawIconSmall( lvcd->nmcd.hdc, IDI_PARENTDIR, &rctIcon );

					if( itemIndex == _focusedItemIndex && FCS::inst().getFcWindow() == GetForegroundWindow() && _hWnd == GetFocus() )
					{
						RECT rctLbl;
						LvUtils::getItemRect( _hWnd, itemIndex, &rctLbl, LVIR_LABEL );

						// draw filled selection rectangle
						IconUtils::drawFocusRectangle( lvcd->nmcd.hdc, rctLbl );
					}

				//	Sleep(100);
					return CDRF_SKIPDEFAULT;
				}

				if( LvUtils::getSelectedItem( _hWnd ) == itemIndex )
				{
					// clear item's selected and focused state to prevent drawing default selection rectangle
					lvcd->nmcd.uItemState &= ~( CDIS_SELECTED | CDIS_FOCUS );
				}

				lvcd->clrText = getItemColor( itemIndex );

				if( itemIndex == _focusedItemIndex && FCS::inst().getFcWindow() == GetForegroundWindow() && _hWnd == GetFocus() )
					lvcd->clrTextBk = FC_COLOR_FOCUSRECT;

			//	Sleep(100);
				return CDRF_NOTIFYPOSTPAINT;
			}

		case ( CDDS_ITEM | CDDS_POSTPAINT ):
			if( lvcd->nmcd.dwItemSpec == _focusedItemIndex && FCS::inst().getFcWindow() == GetForegroundWindow() && _hWnd == GetFocus() )
			{
				auto entryName = _dataMan->getEntryName( _focusedItemIndex );

				RECT rctLbl;
				LvUtils::getItemRect( _hWnd, _focusedItemIndex, &rctLbl, LVIR_LABEL );
				rctLbl.right = rctLbl.left + 5 + LvUtils::getStringWidth( _hWnd, entryName );

				// draw filled selection rectangle
				IconUtils::drawFocusRectangle( lvcd->nmcd.hdc, rctLbl );

				// draw item name - skipping parent (double-dotted) directory
				if( !PathUtils::isDirectoryDoubleDotted( entryName ) )
				{
					SetBkMode( lvcd->nmcd.hdc, TRANSPARENT );
					SetTextColor( lvcd->nmcd.hdc, getItemColor( _focusedItemIndex ) );

					rctLbl.left++;
					DrawText( lvcd->nmcd.hdc, entryName, -1, &rctLbl, DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE | DT_NOPREFIX );
				}

			//	PrintDebug("CDDS_POSTPAINT, %d(%zu) - %d", _focusedItemIndex, _dataMan->getEntryCount(), _dataMan->getStatus());
			//	Sleep(100);
				return CDRF_SKIPDEFAULT;
			}
			break;

		case ( CDDS_ITEM | CDDS_SUBITEM | CDDS_POSTPAINT ):
			switch( lvcd->iSubItem )
			{
			case 0:
				if( lvcd->nmcd.dwItemSpec == _focusedItemIndex && FCS::inst().getFcWindow() == GetForegroundWindow() && _hWnd == GetFocus() )
				{
					auto entryName = _dataMan->getEntryName( _focusedItemIndex );
					auto entryNameLen = LvUtils::getStringWidth( _hWnd, entryName );

					RECT rctLbl;
					LvUtils::getItemRect( _hWnd, _focusedItemIndex, &rctLbl, LVIR_LABEL );
					rctLbl.right = min( rctLbl.right, rctLbl.left + 5 + entryNameLen );

					// draw filled selection rectangle
					IconUtils::drawFocusRectangle( lvcd->nmcd.hdc, rctLbl );

					// draw item name - skipping parent (double-dotted) directory
					if( !PathUtils::isDirectoryDoubleDotted( entryName ) )
					{
						SetBkMode( lvcd->nmcd.hdc, TRANSPARENT );
						SetTextColor( lvcd->nmcd.hdc, getItemColor( _focusedItemIndex ) );

						TCHAR compactPath[MAX_PATH + 1];
						const TCHAR *pTemp = compactPath, **ppPath = &entryName;

						if( isInDetailedMode() && entryNameLen + 2 > ( rctLbl.right - rctLbl.left ) )
						{
							wcsncpy( compactPath, entryName, MAX_PATH );
							PathCompactPath( lvcd->nmcd.hdc, compactPath, rctLbl.right - rctLbl.left - 2 );
							ppPath = &pTemp;
						}

						rctLbl.left++;
						DrawText( lvcd->nmcd.hdc, *ppPath, -1, &rctLbl, DT_LEFT | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE | DT_NOPREFIX );
					}

				//	Sleep(100);
					return CDRF_SKIPDEFAULT;
				}
			}
			break;

		default:
			break;
		}

		return CDRF_DODEFAULT;
	}

	//
	// Get list-view item flags
	//
	UINT CListView::getItemFlags( int itemIndex )
	{
		UINT flags = 0;

		if( itemIndex == _focusedItemIndex )
			flags |= LVIS_FOCUSED | LVIS_SELECTED; // selected and focused item

		if( _dataMan->isEntryAttrSet( itemIndex, FILE_ATTRIBUTE_HIDDEN ) || isItemCut( itemIndex ) )
			flags |= LVIS_CUT; // hidden or cut item attribute

		return flags;
	}

	//
	// Draw items in virtual list-view
	//
	bool CListView::onDrawDispItems( NMLVDISPINFO *lvDisp )
	{
		switch( lvDisp->item.iSubItem )
		{
		case 0:
			if( lvDisp->item.mask & LVIF_STATE )
			{
				lvDisp->item.stateMask = LVIS_FOCUSED | LVIS_SELECTED | LVIS_CUT;
				lvDisp->item.state = getItemFlags( lvDisp->item.iItem );
				lvDisp->item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
			}

			if( lvDisp->item.mask & LVIF_TEXT )
				lvDisp->item.pszText = const_cast<TCHAR*>( _dataMan->getEntryName( lvDisp->item.iItem ) );

			if( lvDisp->item.mask & LVIF_IMAGE )
				lvDisp->item.iImage = _dataMan->getEntryImageIndex( lvDisp->item.iItem );

			if( lvDisp->item.mask & LVIF_INDENT )
				lvDisp->item.iIndent = 0;

			if( lvDisp->item.mask & LVIF_GROUPID )
				lvDisp->item.iGroupId = 0;

			if( lvDisp->item.mask & LVIF_COLUMNS )
				lvDisp->item.cColumns = isInDetailedMode() ? 5 : 1;

			if( lvDisp->item.mask & LVIF_COLFMT )
				lvDisp->item.piColFmt = 0;

			if( lvDisp->item.mask & LVIF_PARAM )
				lvDisp->item.lParam = 0;
			break;
		case 1:
			lvDisp->item.pszText = const_cast<TCHAR*>( _dataMan->getEntrySize( lvDisp->item.iItem ) );
			break;
		case 2:
			lvDisp->item.pszText = const_cast<TCHAR*>( _dataMan->getEntryDate( lvDisp->item.iItem ) );
			break;
		case 3:
			lvDisp->item.pszText = const_cast<TCHAR*>( _dataMan->getEntryTime( lvDisp->item.iItem ) );
			break;
		case 4:
			lvDisp->item.pszText = const_cast<TCHAR*>( _dataMan->getEntryAttr( lvDisp->item.iItem ) );
			break;
		default:
			return false;
		}

		return true;
	}

	//
	// Set current list-view mode
	//
	void CListView::setViewMode( EViewMode mode )
	{
		auto newMode = LV_VIEW_DETAILS;

		switch( mode )
		{
		//case EViewMode::Icon:
		//	newMode = LV_VIEW_ICON;
		//	break;
		case EViewMode::Detailed:
			newMode = LV_VIEW_DETAILS;
			break;
		//case EViewMode::SmallIcon:
		//	newMode = LV_VIEW_SMALLICON;
		//	break;
		case EViewMode::Brief:
			newMode = LV_VIEW_LIST;
			break;
		//case EViewMode::Tile:
		//	newMode = LV_VIEW_TILE;
		//	break;
		}

		if( ListView_SetView( _hWnd, newMode ) == 1 )
		{
			_currentViewMode = mode;
			updateColumnWidth();
		}
	}

	//
	// Load the cache with the recommended range
	//
	void CListView::onPrepareCache( LPNMLVCACHEHINT lvCache )
	{
		// disable redrawing during load process
		//LvUtils::beginUpdate( _hWnd );

	//	_dataMan->loadAssociatedlIcons( lvCache->iFrom, lvCache->iTo );

		// reenable redrawing
		//LvUtils::endUpdate( _hWnd );
	}

	int CListView::onQuickFindSubstring( const std::wstring& str, int from )
	{
		int cnt = static_cast<int>( _dataMan->getEntryCount() );

		for( int i = from; i < cnt; i++ )
		{
			auto name = _dataMan->getEntryName( i );
			if( StringUtils::startsWith( name, str ) )
			{
				return i;
			}
		}

		for( int i = from - 1; i >= 0; i-- )
		{
			auto name = _dataMan->getEntryName( i );
			if( StringUtils::startsWith( name, str ) )
			{
				return i;
			}
		}

		return -1;
	}

	void CListView::onQuickFindFocusItem( int index )
	{
		if( index != _focusedItemIndex )
		{
			ShowWindow( _hWndEditQuickFind, SW_HIDE ); // sends LVM_CANCELEDITLABEL

			_focusedItemIndex = index;
			_isQuickFindMode = true;

			PostMessage( _hWnd, LVM_EDITLABEL, _focusedItemIndex, 0 );
		}
	}

	void CListView::onQuickFindCharEditbox( WCHAR ch )
	{
		if( StringUtils::startsWith( _focusedItemName, _quickFindStr + ch ) )
		{
			_quickFindStr += ch;
			auto pos = _quickFindStr.length();

			PostMessage( _hWndEditQuickFind, EM_SETSEL, (WPARAM)pos, (LPARAM)pos );
		}
		else
		{
			int idx = onQuickFindSubstring( _quickFindStr + ch, _focusedItemIndex );
			if( idx != -1 )
			{
				_quickFindStr += ch;
				onQuickFindFocusItem( idx );
			}
		}
	}

	void CListView::onQuickFindPrevious( const std::wstring& str, int from )
	{
		for( int i = from; i >= 0; i-- )
		{
			auto name = _dataMan->getEntryName( i );
			if( StringUtils::startsWith( name, str ) )
			{
				onQuickFindFocusItem( i );
				break;
			}
		}
	}

	void CListView::onQuickFindNext( const std::wstring& str, int from )
	{
		int cnt = static_cast<int>( _dataMan->getEntryCount() );

		for( int i = from; i < cnt; i++ )
		{
			auto name = _dataMan->getEntryName( i );
			if( StringUtils::startsWith( name, str ) )
			{
				onQuickFindFocusItem( i );
				break;
			}
		}
	}

	LRESULT CALLBACK CListView::wndProcQuickFind( UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId )
	{
	//	PrintDebug("msg:0x%04X, hw:0x%08X, wp:0x%08X, lp:0x%08X", message, _hWndEditQuickFind, lParam, wParam);

		switch( message )
		{
		case WM_KEYDOWN:
			switch( wParam )
			{
			case VK_UP:
				if( _focusedItemIndex > 0 )
					onQuickFindPrevious( _quickFindStr, _focusedItemIndex - 1 );
				return 0;
			case VK_DOWN:
				if( _focusedItemIndex + 1 < static_cast<int>( _dataMan->getEntryCount() ) )
					onQuickFindNext( _quickFindStr, _focusedItemIndex + 1 );
				return 0;
			case VK_LEFT:
				if( _quickFindStr.length() )
					_quickFindStr = _focusedItemName.substr( 0, _quickFindStr.length() - 1 );
				break;
			case VK_RIGHT:
				if( _focusedItemName.length() > _quickFindStr.length() )
					_quickFindStr = _focusedItemName.substr( 0, _quickFindStr.length() + 1 );
				break;
			case VK_HOME:
				onQuickFindNext( _quickFindStr, 0 );
				return 0;
			case VK_END:
				onQuickFindPrevious( _quickFindStr, static_cast<int>( _dataMan->getEntryCount() ) - 1 );
				return 0;
			case VK_SPACE:
				// block space use for item marking
				break;

			case VK_CONTROL:
			case VK_SHIFT:
				// sends LVM_CANCELEDITLABEL
				ShowWindow( _hWndEditQuickFind, SW_HIDE );
				break;

			default:
				{
					// handle command keys
					NMLVKEYDOWN keyDown = { 0 };
					keyDown.wVKey = (WORD)wParam;
					handleKeyboardAction( &keyDown );
				}
				break;
			}
			break;

		case WM_SYSKEYDOWN:
			if( wParam == VK_MENU ) // alt key
				ShowWindow( _hWndEditQuickFind, SW_HIDE );
			break;

		case WM_SETCURSOR: // remove default edit-box cursor
			return 0;

		case WM_CHAR:
			onQuickFindCharEditbox( static_cast<WCHAR>( wParam ) );
			break;

		case WM_CONTEXTMENU:
			ShowWindow( _hWndEditQuickFind, SW_HIDE );
			FORWARD_WM_CONTEXTMENU( _hWnd, wParam, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), PostMessage );
			break;

		case WM_RBUTTONDOWN:
			ShowWindow( _hWndEditQuickFind, SW_HIDE );
			break;

		case WM_MBUTTONDOWN:
		{
			ShowWindow( _hWndEditQuickFind, SW_HIDE ); // sends LVM_CANCELEDITLABEL

			// recalc relative position and forward message to the list-view
			RECT rct;
			ListView_GetItemRect( _hWnd, _focusedItemIndex, &rct, LVIR_LABEL );

			POINT pt { rct.left + GET_X_LPARAM( lParam ), rct.top + GET_Y_LPARAM( lParam ) };
			FORWARD_WM_MBUTTONDOWN( _hWnd, 0, pt.x, pt.y, wParam, PostMessage );
			break;
		}

		case WM_LBUTTONDOWN:
		{
			ShowWindow( _hWndEditQuickFind, SW_HIDE ); // sends LVM_CANCELEDITLABEL

			// recalc relative position and forward message to the list-view
			RECT rct;
			ListView_GetItemRect( _hWnd, _focusedItemIndex, &rct, LVIR_LABEL );

			POINT pt { rct.left + GET_X_LPARAM( lParam ), rct.top + GET_Y_LPARAM( lParam ) };
			FORWARD_WM_LBUTTONDOWN( _hWnd, 0, pt.x, pt.y, wParam, PostMessage );
			break;
		}
		default:
			break;
		}

		return DefSubclassProc( _hWndEditQuickFind, message, wParam, lParam );
	}

	void CListView::onQuickFindChar( WCHAR ch )
	{
		if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) == 0 ) // ignore chars with SHIFT key modifier
		{
			if( ch > L' ' ) // skip non-printable chars and space
			{
				_isQuickFindMode = true;

				_quickFindStr.clear();

				int idx = onQuickFindSubstring( { ch }, _focusedItemIndex );

				if( idx == -1 )
					idx = _focusedItemIndex;
				else
				{
					_focusedItemIndex = idx;
					_quickFindStr += ch;
				}

				PrintDebug("%ls", _quickFindStr.c_str());
				PostMessage( _hWnd, LVM_EDITLABEL, static_cast<WPARAM>( idx ), 0 );
			}
		}
	}

	void CListView::startQuickFind()
	{
		PrintDebug("%ls(%ls)", _focusedItemName.c_str(), _quickFindStr.c_str());

		_hWndEditQuickFind = ListView_GetEditControl( _hWnd );
		auto len = _quickFindStr.length();

		PostMessage( _hWndEditQuickFind, EM_SETREADONLY, TRUE, 0 );
		PostMessage( _hWndEditQuickFind, EM_SETSEL, static_cast<WPARAM>( len ), static_cast<LPARAM> ( len ) );
		PostMessage( _hWndEditQuickFind, EM_SCROLLCARET, 0, 0 );

		SetWindowSubclass( _hWndEditQuickFind, wndProcQuickFindEdit, 0, reinterpret_cast<DWORD_PTR>( this ) );
	}

	void CListView::stopQuickFind()
	{
		PrintDebug("%ls(%ls)", _focusedItemName.c_str(), _quickFindStr.c_str());

		_isQuickFindMode = false;

		RemoveWindowSubclass( _hWndEditQuickFind, wndProcQuickFindEdit, 0 );

	//	_hWndEditQuickFind = nullptr; // do not reset, we use this value to detect messages for (now non-existing) edit-box
	}

	//
	// Show system shell context menu for selected item(s)
	//
	UINT CListView::onContextMenu( POINT& pt )
	{
		auto items = _pParentTab->getSelectedItemsPathFull();
		int idx = LvUtils::getSelectedItem( _hWnd );

		if( _dataMan->getMarkedEntries().empty() && idx == -1 )
		{
			// when user clicks outside of any item, show properties for current directory
			items.clear();
			items.push_back( _dataMan->getCurrentDirectory() );
		}

		// get current cursor (or selected item) position
		LvUtils::getItemPosition( _hWnd, idx, &pt );

		// check if only files are selected
		auto indices = LvUtils::getSelectedItems( _hWnd );
		bool isFile = !indices.empty();

		for( int itemIdx : indices )
		{
			if( !_dataMan->isEntryFile( itemIdx ) )
			{
				isFile = false;
				break;
			}
		}

		// show context menu for currently selected items
		UINT ret = FCS::inst().getApp().processContextMenu( _contextMenu, _hWnd, items, pt, isFile );

		if( ret == (UINT)~0 )
		{
			auto verb = _contextMenu.getResultVerb();

			if( verb == "cut" )
				cutSelectedItems();
			else if( verb == "copy" || verb == "paste" )
				_cutItems.clear();
		}

		return ret;
	}

	//
	// Open new tab on middle mouse button
	//
	void CListView::openNewTab( POINT& pt )
	{
		int idx = LvUtils::findItem( _hWnd, pt );

		if( idx != -1 )
		{
			LvUtils::unselectItem( _hWnd, _focusedItemIndex );
			LvUtils::selectItem( _hWnd, idx );

			_focusedItemIndex = idx;

			if( ( GetKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
				_pParentTab->processCommand( EFcCommand::OpenTabOpposite );
			else
				_pParentTab->processCommand( EFcCommand::OpenTab );
		}
	}

	//
	// Begin drag & drop items in list-view
	//
	void CListView::onBeginDrag( LPNMLISTVIEW lvData )
	{
		IShellItemArray *psiaItems;
		ShellUtils::createShellItemArrayFromPaths( _pParentTab->getSelectedItemsPathFull(), &psiaItems );

		if( psiaItems )
		{
			IDataObject *ppdtobj = NULL;
			psiaItems->BindToHandler( NULL, BHID_DataObject, IID_PPV_ARGS( &ppdtobj ) );
			psiaItems->Release();

			DWORD effect = 0;
			SHDoDragDrop( _hWnd, ppdtobj, NULL, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect );

			ppdtobj->Release();
		}
	}

	//
	// On system drop operation
	//
	bool CListView::onDrop( const std::vector<std::wstring>& items, EFcCommand cmd )
	{
		if( cmd == EFcCommand::LinkItem )
			ShellUtils::createShortcut( items[0], _controlUserData + PathUtils::stripPath( items[0] ) + L".lnk" );
		else //	FCS::inst().getFastCopyManager().start( cmd, items, _controlUserData );
		{
			BOOL operationAborted = FALSE;

			CComPtr<IFileOperation> pfo;
			auto hr = CoCreateInstance( CLSID_FileOperation, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS( &pfo ) );

			if( SUCCEEDED( hr ) )
			{
				CComPtr<IDataObject> ppdtobj;
				CComPtr<IShellItem> psiTo;
				CComPtr<IShellItemArray> psiaItems;

				// create an IShellItem from the supplied source path
				hr = SHCreateItemFromParsingName( _controlUserData.c_str(), nullptr, IID_PPV_ARGS( &psiTo ) );

				if( SUCCEEDED( hr ) )
				{
					hr = pfo->SetOwnerWindow( FCS::inst().getFcWindow() );

					hr = ShellUtils::createShellItemArrayFromPaths( items, &psiaItems );
					hr = psiaItems->BindToHandler( NULL, BHID_DataObject, IID_PPV_ARGS( &ppdtobj ) );

					// add the operation
					if( cmd == EFcCommand::CopyItems )
						hr = pfo->CopyItems( ppdtobj, psiTo );
					else
						hr = pfo->MoveItems( ppdtobj, psiTo );

					if( SUCCEEDED( hr ) )
					{
						// perform the operation to copy/move the file
						hr = pfo->PerformOperations();
						hr = pfo->GetAnyOperationsAborted( &operationAborted );
					}
				}
			}
		}

		return true;
	}

	//
	// Handle keyboard actions without any modificator key pressed
	//
	void CListView::handleKeyboardAction( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_RETURN:
			_pParentTab->processCommand( EFcCommand::ExecuteItem );
			break;
		case VK_F2:
			_pParentTab->processCommand( EFcCommand::RenameItem );
			break;
		case VK_F3:
			_pParentTab->processCommand( EFcCommand::ViewFile );
			break;
		case VK_F4:
			_pParentTab->processCommand( EFcCommand::EditFile );
			break;
		case VK_F5:
			_pParentTab->processCommand( EFcCommand::CopyItems );
			break;
		case VK_F6:
			_pParentTab->processCommand( EFcCommand::MoveItems );
			break;
		case VK_F7:
			_pParentTab->processCommand( EFcCommand::MakeDir );
			break;
		case VK_F8:
		case VK_DELETE:
			_pParentTab->processCommand( EFcCommand::RecycleItems );
			break;
		case VK_F11:
			_pParentTab->processCommand( EFcCommand::ConnectNetDrive );
			break;
		case VK_F12:
			_pParentTab->processCommand( EFcCommand::DisconnectNetDrive );
			break;
		case VK_BACK:
			_pParentTab->processCommand( EFcCommand::GoUpper );
			break;
		case VK_ADD:
			_pParentTab->processCommand( EFcCommand::MarkFiles );
			break;
		case VK_SUBTRACT:
			_pParentTab->processCommand( EFcCommand::UnmarkFiles );
			break;
		case VK_DIVIDE:
			_pParentTab->processCommand( EFcCommand::ShowCmdPrompt );
			break;
		case VK_MULTIPLY:
			_pParentTab->processCommand( EFcCommand::InvertSelectionFiles );
			break;
		case VK_SPACE:
		case VK_INSERT:
			markFocusedItem();
			break;
		}
	}

	//
	// Handle keyboard action with Ctrl+Shift key pressed
	//
	void CListView::handleKeyboardActionWithCtrlAndShift( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_F9:
			_pParentTab->processCommand( EFcCommand::ShowSharedDirs );
			break;
		case VK_ADD:
			_pParentTab->processCommand( EFcCommand::SelectSameName );
			break;
		case VK_SUBTRACT:
			_pParentTab->processCommand( EFcCommand::UnselectSameName );
			break;
		case 0x43: // "C - calculate checksums"
			_pParentTab->processCommand( EFcCommand::ChecksumCalc );
			break;
		case 0x54: // "T - shortcut target"
			_pParentTab->processCommand( EFcCommand::GoShortcutTarget );
			break;
		case 0x56: // "V - verify checksums"
			_pParentTab->processCommand( EFcCommand::ChecksumVerify );
			break;
		}
	}

	//
	// Handle keyboard action with Ctrl+Alt key pressed
	//
	void CListView::handleKeyboardActionWithCtrlAndAlt( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_INSERT:
			_pParentTab->processCommand( EFcCommand::ClipCopyPath );
			break;
		case VK_F5:
			_pParentTab->processCommand( EFcCommand::RepackItem );
			break;
		case 0x54: // "T - open new tab in opposite panel"
			_pParentTab->processCommand( EFcCommand::OpenTabOpposite );
			break;
		}
	}

	//
	// Handle keyboard action with Alt+Shift key pressed
	//
	void CListView::handleKeyboardActionWithAltAndShift( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_INSERT:
			_pParentTab->processCommand( EFcCommand::ClipCopyName );
			break;
		}
	}

	//
	// Handle keyboard action with Ctrl key pressed
	//
	bool CListView::handleKeyboardActionWithCtrl( LPNMLVKEYDOWN vKey )
	{
		//PrintDebug("key: 0x%04X", vKey->wVKey);
		switch( vKey->wVKey )
		{
		case VK_F2:
			_pParentTab->processCommand( EFcCommand::ChangeAttr );
			break;
		case VK_F3:
			_pParentTab->processCommand( EFcCommand::SortByName );
			break;
		case VK_F4:
			_pParentTab->processCommand( EFcCommand::SortByExt );
			break;
		case VK_F5:
			_pParentTab->processCommand( EFcCommand::SortByTime );
			break;
		case VK_F6:
			_pParentTab->processCommand( EFcCommand::SortBySize );
			break;
		case VK_F7:
			_pParentTab->processCommand( EFcCommand::ChangeCase );
			break;
		case VK_F10:
			_pParentTab->processCommand( EFcCommand::CompareDirs );
			break;
		case VK_F12:
			_pParentTab->processCommand( EFcCommand::CreateIso );
			break;
		case VK_MULTIPLY:
			_pParentTab->processCommand( EFcCommand::InvertSelectionAll );
			break;
		case VK_ADD:
		case 0x41: // "A - select all"
			_pParentTab->processCommand( EFcCommand::SelectAll );
			break;
		case VK_SUBTRACT:
		case 0x44: // "D - unselect all"
			_pParentTab->processCommand( EFcCommand::UnselectAll );
			break;
		case 0x49: // "I - combine file"
			_pParentTab->processCommand( EFcCommand::CombineFile );
			break;
		case 0x4A: // "J - download file"
			_pParentTab->processCommand( EFcCommand::DownloadFile );
			break;
		case 0x4D: // "M - files list to clipboard"
			_pParentTab->processCommand( EFcCommand::MakeFileList );
			break;
		case 0x4F: // "O - compare two files content"
			_pParentTab->processCommand( EFcCommand::CompareFiles );
			break;
		case VK_F9:
		case 0x52: // "R - refresh"
			_pParentTab->refresh();
			break;
		case 0x53: // "S - split file"
			_pParentTab->processCommand( EFcCommand::SplitFile );
			break;
		case 0x54: // "T - open new tab"
			_pParentTab->processCommand( EFcCommand::OpenTab );
			return true;
		case 0x55: // "U - swap panels"
			_pParentTab->processCommand( EFcCommand::SwapPanels );
			break;
		case 0x57: // "W - close tab"
			_pParentTab->processCommand( EFcCommand::CloseTab );
			return true;
		case 0x43: // "C - copy"
			_pParentTab->processCommand( EFcCommand::ClipCopy );
			break;
		case 0x56: // "V - paste"
			_pParentTab->processCommand( EFcCommand::ClipPaste );
			break;
		case 0x58: // "X - cut"
			_pParentTab->processCommand( EFcCommand::ClipCut );
			break;
		case 0xBE: // ". - go to other panel's path"
			_pParentTab->processCommand( EFcCommand::GoPathOtherPanel );
			break;
		case 0xDC: // "\ - go to root"
			_pParentTab->processCommand( EFcCommand::GoRoot );
			break;
		}

		return false;
	}

	//
	// Handle keyboard action with Alt key pressed
	//
	bool CListView::handleKeyboardActionWithAlt( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_LEFT:
			_pParentTab->processCommand( EFcCommand::HistoryBackward );
			break;
		case VK_RIGHT:
			_pParentTab->processCommand( EFcCommand::HistoryForward );
			break;
		case VK_RETURN:
			_pParentTab->processCommand( EFcCommand::ShowProperties );
			break;
		case VK_INSERT:
			_pParentTab->processCommand( EFcCommand::ClipCopyPathFull );
			break;
		case 0x32: // '2'
			_pParentTab->processCommand( EFcCommand::ViewModeBrief );
			break;
		case 0x33: // '3'
			_pParentTab->processCommand( EFcCommand::ViewModeDetailed );
			break;
		case VK_F3:
			_pParentTab->processCommand( EFcCommand::ViewFileHex );
			break;
		case VK_F5:
			_pParentTab->processCommand( EFcCommand::PackItem );
			break;
		case VK_F7:
			_pParentTab->processCommand( EFcCommand::FindFiles );
			break;
		case VK_F8:
			_pParentTab->processCommand( EFcCommand::FindDuplicates );
			break;
		case VK_F9:
			_pParentTab->processCommand( EFcCommand::UnpackItem );
			break;
		default:
			return false;
		}

		return true;
	}

	//
	// Handle keyboard action with Shift key pressed
	//
	bool CListView::handleKeyboardActionWithShift( LPNMLVKEYDOWN vKey )
	{
		switch( vKey->wVKey )
		{
		case VK_F3:
			_pParentTab->processCommand( EFcCommand::OpenFolderActive );
			break;
		case VK_F4:
			_pParentTab->processCommand( EFcCommand::MakeFile );
			break;
		case VK_F7:
			_pParentTab->processCommand( EFcCommand::ChangeDir );
			break;
		case VK_F8:
		case VK_DELETE:
			_pParentTab->processCommand( EFcCommand::DeleteItems );
			break;
		case VK_BACK:
			_pParentTab->processCommand( EFcCommand::GoRoot );
			break;
		case VK_ADD:
			_pParentTab->processCommand( EFcCommand::SelectSameExt );
			break;
		case VK_SUBTRACT:
			_pParentTab->processCommand( EFcCommand::UnselectSameExt );
			break;
		default: // quick-change current drive (A-Z)
			if( vKey->wVKey > 0x40 && vKey->wVKey < 0x5B )
			{
				WCHAR driveName[] = { (WCHAR)vKey->wVKey, L':', L'\\', L'\0' };
				_pParentTab->changeDrive( driveName );

				return false;
			}
		}

		return true;
	}

	//
	// Handle keyboard navigation (left/right, page up/down)
	//
	void CListView::handleKeyboardNavigation( LPNMLVKEYDOWN vKey )
	{
		int itemCnt = static_cast<int>( _dataMan->getEntryCount() );
		if( isInBriefMode() && itemCnt > 1 )
		{
			int rowCnt = LvUtils::countRows( _hWnd );

			switch( vKey->wVKey )
			{
			case VK_LEFT:
			case VK_PRIOR:
				if( itemCnt == rowCnt || _focusedItemIndex < rowCnt )
				{
					LvUtils::unselectItem( _hWnd, _focusedItemIndex );
					LvUtils::selectItem( _hWnd, 0 );
				}
				break;

			case VK_RIGHT:
			case VK_NEXT:
				if( itemCnt == rowCnt || _focusedItemIndex > ( itemCnt - rowCnt - 1 ) )
				{
					LvUtils::unselectItem( _hWnd, _focusedItemIndex );
					LvUtils::selectItem( _hWnd, itemCnt - 1 );
					LvUtils::ensureVisible( _hWnd, itemCnt - 1 );
				}
				break;
			}
		}
	}

	//
	// Handle keyboard virtual keys
	//
	bool CListView::onHandleVirtualKeys( LPNMLVKEYDOWN vKey )
	{
		if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 &&
			( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
		{
			// CTRL+SHIFT modifier keys
			handleKeyboardActionWithCtrlAndShift( vKey );
		}
		else if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 &&
			( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 )
		{
			// CTRL+ALT modifier keys
			handleKeyboardActionWithCtrlAndAlt( vKey );
		}
		else if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 &&
			( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 )
		{
			// ALT+SHIFT modifier keys
			handleKeyboardActionWithAltAndShift( vKey );
		}
		else if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
		{
			// CTRL modifier key
			return handleKeyboardActionWithCtrl( vKey );
		}
		else if( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 )
		{
			// ALT modifier key
			return handleKeyboardActionWithAlt( vKey );
		}
		else if( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 )
		{
			// SHIFT modifier key
			return handleKeyboardActionWithShift( vKey );
		}
		else
		{
			// no modifier key
			handleKeyboardAction( vKey );
			handleKeyboardNavigation( vKey );
		}

		return false;
	}

	//
	// Handle keyboard char keys - to prevent system beep sound
	//
	bool CListView::onHandleKeyboardChars( WPARAM charCode, LPARAM charData )
	{
		switch( charCode )
		{
		case L'/':
			return ( charData & 0x01000000 ) == 0x01000000;

		case L'*':
			return ( charData & 0x00200000 ) == 0x00200000;

		case L'+':
			return ( charData & 0x00400000 ) == 0x00400000;

		case L'-':
			return ( charData & 0x00400000 ) == 0x00400000;

		default:
			break;
		}

		return false;
	}

	//
	// On WM_NOTIFY message handler
	//
	LRESULT CListView::onNotifyMessage( WPARAM wParam, LPARAM lParam )
	{
		switch( reinterpret_cast<LPNMHDR>( lParam )->code )
		{
		case NM_CUSTOMDRAW:
			return onCustomDraw( reinterpret_cast<LPNMLVCUSTOMDRAW>( lParam ) );

		case NM_DBLCLK:
			_pParentTab->processCommand( EFcCommand::ExecuteItem );
			break;

		case LVN_ITEMCHANGED:
			onSelectionChanged( reinterpret_cast<LPNMLISTVIEW>( lParam ) );
			break;

		case LVN_COLUMNCLICK:
			onColumnClick( reinterpret_cast<LPNMLISTVIEW>( lParam ) );
			break;

		case LVN_KEYDOWN:
			if( onHandleVirtualKeys( reinterpret_cast<LPNMLVKEYDOWN>( lParam ) ) )
				return (LRESULT)true;
			break;

		case LVN_GETDISPINFO:
			return onDrawDispItems( reinterpret_cast<NMLVDISPINFO*>( lParam ) );

		case LVN_ODCACHEHINT:
			onPrepareCache( reinterpret_cast<LPNMLVCACHEHINT>( lParam ) );
			return 0;

		case LVN_ODFINDITEM:
			return _focusedItemIndex;

		case LVN_BEGINLABELEDIT:
			if( _isQuickFindMode )
				startQuickFind();
			else
				return onBeginLabelEdit( reinterpret_cast<NMLVDISPINFO*>( lParam ) );
			break;

		case LVN_ENDLABELEDIT:
			if( _isQuickFindMode )
				stopQuickFind();
			else
				return onEndLabelEdit( reinterpret_cast<NMLVDISPINFO*>( lParam ) );
			break;

		case LVN_BEGINDRAG:
			onBeginDrag( reinterpret_cast<LPNMLISTVIEW>( lParam ) );
			break;

		case LVN_MARQUEEBEGIN: // TODO - not compatible with LVS_SINGLESEL flag
			PrintDebug( "LVN_MARQUEEBEGIN" );
			break;

		case LVN_BEGINSCROLL: // TODO - handle scrolling
			//PrintDebug( "Begin x: %d", reinterpret_cast<LPNMLVSCROLL>( lParam )->dx );
			break;

		case LVN_ENDSCROLL:
			//PrintDebug( "End x: %d", reinterpret_cast<LPNMLVSCROLL>( lParam )->dx );
			break;

		default:
			break;
		}

		return CallWindowProc( _wndProcDefault, _hWnd, WM_NOTIFY, wParam, lParam );
	}

	//
	// System messages to quick find edit-box
	//
	bool CListView::isQuickFindMessage( LPMSG msg )
	{
		/*if (msg->message == WM_LBUTTONDOWN || msg->message == WM_LBUTTONUP)
		{
			PrintDebug("%ls: panel:tab[%u:%u], lv[%d], ed[%d]",
				msg->message == WM_LBUTTONDOWN ? L"WM_LBUTTONDOWN" : L"WM_LBUTTONUP",
				_pParentPanel->id(), _pParentTab->id(),
				msg->hwnd == _hWnd, msg->hwnd == _hWndEditQuickFind);
		}*/

		if( msg->hwnd == _hWndEditQuickFind )
		{
			if( msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN )
			{
				NMLVKEYDOWN keyDown = { 0 };
				keyDown.wVKey = (WORD)msg->wParam;

				switch( msg->wParam )
				{
				case VK_RETURN:
					ShowWindow( _hWndEditQuickFind, SW_HIDE );
					return onHandleVirtualKeys( &keyDown );

				case VK_TAB:
					FCS::inst().getApp().getOppositePanel().setPanelFocus();
					return true;

				default:
					break;
				}
			}
		}

		return false;
	}

	//
	// Subclassed window procedure
	//
	LRESULT CALLBACK CListView::wndProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		// forward messages from workers first (filesytem timer messages as well)
		if( ( message >= ( UM_FIRSTMESSAGE ) && message <= ( UM_LAST_MESSAGE ) ) || message == WM_TIMER )
		{
			// notify reader worker (the timer message is CFtpReader specific)
			if( message != WM_TIMER || ( message == WM_TIMER && wParam == FC_TIMER_KEEPALIVE_ID ) )
				_dataMan->onWorkerNotify( message, lParam, static_cast<int>( wParam ) );

			// notify parent tab
			if( wParam <= 1 || ( message == WM_TIMER && wParam == FC_TIMER_OVERLAYS_ID ) )
				_pParentTab->onDataManagerNotify( message, static_cast<int>( wParam ) );

			return 0;
		}

		switch( message )
		{
		case WM_GETDLGCODE:
			if( lParam )
			{
				// tell the window manager that we want ENTER key to be send
				MSG *msg = reinterpret_cast<MSG*>( lParam );
				if( msg->message == WM_KEYDOWN && msg->wParam == VK_RETURN )
				{
					return DLGC_WANTMESSAGE;
				}
			}
			break;

		case WM_NOTIFY:
			return onNotifyMessage( wParam, lParam );

		case WM_MOUSEWHEEL:
			PrintDebug( "Wheel delta: %d", GET_WHEEL_DELTA_WPARAM( wParam ) );
			break;

		case WM_SETFOCUS:
			_pParentTab->gotFocus();
			break;

		case WM_CTLCOLORSTATIC:
			// customize quick find edit-box colors
			SetTextColor( reinterpret_cast<HDC>( wParam ), getItemColor( _focusedItemIndex ) );
			SetBkColor( reinterpret_cast<HDC>( wParam ), FC_COLOR_FOCUSRECT );
			return reinterpret_cast<LRESULT>( GetStockBrush( LTGRAY_BRUSH ) );

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			if( LvUtils::findItem( _hWnd, POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } ) == 0 )
			{
				if( PathUtils::isDirectoryDoubleDotted( _dataMan->getEntryName( 0 ) ) )
				{
					LvUtils::unselectItem( _hWnd, _focusedItemIndex );
					LvUtils::selectItem( _hWnd, 0 );

					_focusedItemIndex = 0;
				}
			}
			break;

		case WM_MBUTTONDOWN:
			openNewTab( POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } );
			return 0;

		case WM_CONTEXTMENU:
			if( onContextMenu( POINT { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) } ) == 2 )
				_pParentTab->compareFiles();
			break;

		case WM_DRAWITEM:
		case WM_MEASUREITEM:
		case WM_MENUCHAR:
		case WM_INITMENUPOPUP:
		case WM_MENUSELECT:
		{
			auto ret =_contextMenu.handleMessages( _hWnd, message, wParam, lParam );
			if( ret )
				return ret;
			break;
		}

		case WM_KEYDOWN:
			if( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 )
			{
				// CTRL modifier key
				if( wParam == VK_LEFT || wParam == VK_RIGHT )
				{
					_pParentPanel->switchTab( wParam == VK_LEFT ? EFcCommand::PrevTab : EFcCommand::NextTab );
					return 0;
				}
			}
			else if( wParam == VK_SHIFT ) // HACK: to prevent WM_CHAR not firing
				return 0;
			else if( wParam == VK_TAB ) // HACK: when modal dialog is opened somewhere
				FCS::inst().getApp().getOppositePanel().setPanelFocus();
			break;

		case WM_SYSCHAR:
		case WM_CHAR:
			if( !onHandleKeyboardChars( wParam, lParam ) )
				onQuickFindChar( static_cast<WCHAR>( wParam ) );
			return 0;

		case WM_DESTROY:
			TerminateDragDropHelper();
			break;

		default:
			break;
		}

		return CallWindowProc( _wndProcDefault, _hWnd, message, wParam, lParam );
	}
}
