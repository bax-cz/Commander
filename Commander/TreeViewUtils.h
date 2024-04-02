#pragma once

/*
	Tree-View utils - helper functions to make life with tree-view easier
*/

namespace Commander
{
	struct TvUtils
	{
		//
		// Set extended style 
		//
		static void setExtStyle( const HWND hw, const DWORD extStyle )
		{
			TreeView_SetExtendedStyle( hw, extStyle, extStyle );
		}

		//
		// Delete all items
		//
		static int cleanUp( const HWND hw ) { return TreeView_DeleteAllItems( hw ); }

		//
		// Disable redrawing during update
		//
		static void beginUpdate( const HWND hw ) { SendMessage( hw, WM_SETREDRAW, static_cast<WPARAM>( false ), 0 ); }

		//
		// Enable redrawing after update finished
		//
		static void endUpdate( const HWND hw ) { SendMessage( hw, WM_SETREDRAW, static_cast<WPARAM>( true ), 0 ); }

		//
		// Get selected item
		//
		static TV_ITEM getSelectedItem( const HWND hw )
		{
			TV_ITEM item = { 0 };

			item.mask = TVIF_PARAM;
			item.hItem = TreeView_GetSelection( hw );

			TreeView_GetItem( hw, &item );

			return item;
		}

		//
		// Add item to tree-view
		//
		static HTREEITEM addItem( const HWND hw, HTREEITEM hRoot, HTREEITEM hAfter, UINT mask, LPWSTR text, LPARAM lParam = NULL )
		{
			TV_INSERTSTRUCT tvis = { 0 };

			tvis.hParent = hRoot;
			tvis.hInsertAfter = hAfter;
			tvis.item.mask = mask;
			tvis.item.pszText = text;
			tvis.item.lParam = lParam;

			return TreeView_InsertItem( hw, &tvis );
		}

		//
		// Find item by position, return HTREEITEM
		//
		static HTREEITEM findItem( const HWND hw, const POINT& pt )
		{
			TVHITTESTINFO tvhti = { 0 };
			tvhti.flags = TVHT_ONITEM;
			tvhti.pt = pt;

			return (HTREEITEM)SendMessage( hw, TVM_HITTEST, 0, reinterpret_cast<LPARAM>( &tvhti ) );
		}

		//
		// Find root item by text
		//
		static HTREEITEM findRootItem( const HWND hw, const LPWSTR itemText )
		{
			HTREEITEM hItem = TreeView_GetRoot( hw );
			TVITEMEX tvi = { 0 };
			WCHAR buff[MAX_PATH];

			while( hItem )
			{
				tvi.mask = TVIF_TEXT;
				tvi.hItem = hItem;
				tvi.pszText = buff;
				tvi.cchTextMax = MAX_PATH - 1;

				TreeView_GetItem( hw, &tvi );

				if( wcsicmp( tvi.pszText, itemText ) )
					hItem = TreeView_GetNextSibling( hw, hItem );
				else
					break;
			}

			return hItem;
		}

		//
		// Find item by text, return index
		//
		static HTREEITEM findChildItem( const HWND hw, HTREEITEM hRoot, const LPWSTR itemText )
		{
			HTREEITEM hItem = TreeView_GetChild( hw, hRoot );
			TVITEMEX tvi = { 0 };
			WCHAR buff[MAX_PATH];

			while( hItem )
			{
				tvi.mask = TVIF_TEXT;
				tvi.hItem = hItem;
				tvi.pszText = buff;
				tvi.cchTextMax = MAX_PATH - 1;

				TreeView_GetItem( hw, &tvi );

				if( wcsicmp( tvi.pszText, itemText ) )
					hItem = TreeView_GetNextSibling( hw, hItem );
				else
					break;
			}

			return hItem;
		}

		/*
		//
		// Set tree-view image-list
		//
		static HIMAGELIST setImageList( const HWND hw, const HIMAGELIST himl, int type ) { return TreeView_SetImageList( hw, himl, type ); }

		//
		// Initiate label edit
		//
		static HWND editItemLabel( const HWND hw, int itemIndex ) { return TreeView_EditLabel( hw, itemIndex ); }

		//
		// Cancel label edit
		//
		static void cancelEditItemLabel( const HWND hw ) { TreeView_CancelEditLabel( hw ); }

		//
		// Delete item from tree-view
		//
		static void deleteItem( const HWND hw, int itemIndex ) { TreeView_DeleteItem( hw, itemIndex ); }

		//
		// Get virtual number of items
		//
		static int getItemCount( const HWND hw ) { return TreeView_GetItemCount( hw ); }

		//
		// Set virtual number of items
		//
		static void setItemCount( const HWND hw, const size_t count, const DWORD flags ) { TreeView_SetItemCountEx( hw, count, flags ); }

		//
		// Get select-marked item
		//
		static int getMarkedItem( const HWND hw ) { return TreeView_GetSelectionMark( hw ); }

		//
		// Get selected and also focused item
		//
		static int getSelectedItem( const HWND hw ) { return TreeView_GetNextItem( hw, -1, LVNI_SELECTED | LVNI_FOCUSED ); }

		//
		// Get only focused item
		//
		static int getFocusedItem( const HWND hw ) { return TreeView_GetNextItem( hw, -1, LVNI_FOCUSED ); }

		//
		// Focus item only and ensure its visibility
		//
		static void focusItem( const HWND hw, int index ) { setItemState( hw, index, LVIS_FOCUSED ); ensureVisible( hw, index ); }

		//
		// Select and focus item
		//
		static void selectItem( const HWND hw, int index ) { setItemState( hw, index, LVIS_FOCUSED | LVIS_SELECTED ); }

		//
		// Unselect item
		//
		static void unselectItem( const HWND hw, int index ) { setItemState( hw, index, 0 ); }

		//
		// Get item bounding rectangle
		//
		static bool getItemRect( const HWND hw, const size_t itemIndex, const RECT *rct, int code )
		{
			return TreeView_GetItemRect( hw, itemIndex, rct, code ) ? true : false;
		}

		//
		// Set state of the item - selected, focused, cut.
		//
		static void setItemState( const HWND hw, int itemIndex, const DWORD flags )
		{
			// last parameter is bitmask, telling tree-view which bits we want to set
			TreeView_SetItemState( hw, itemIndex, flags, LVIS_CUT | LVIS_FOCUSED | LVIS_SELECTED );
		}

		//
		// Ensure that the item is visible
		//
		static void ensureVisible( const HWND hw, int itemIndex, const bool noRedraw = false )
		{
			if( noRedraw )
				beginUpdate( hw );

			TreeView_EnsureVisible( hw, itemIndex, false ); // we don't want only partial view

			if( noRedraw )
				endUpdate( hw );
		}

		//
		// Get item text at given index
		//
		static std::wstring getItemText( const HWND hw, const size_t itemIndex, int column = 0 )
		{
			TCHAR itemText[MAX_PATH] = { 0 };
			TreeView_GetItemText( hw, itemIndex, column, itemText, MAX_PATH );
			return std::wstring( itemText );
		}

		//
		// Get selected items array
		//
		static std::vector<int> getSelectedItems( const HWND hw )
		{
			std::vector<int> indices;

			int index = -1;
			while( ( index = TreeView_GetNextItem( hw, index, LVNI_SELECTED ) ) != -1 )
				indices.push_back( index );

			return indices;
		}

		//
		// Add subitem to tree-view
		//
		static void addSubItemText( const HWND hw, int idx, int column, std::wstring itemText )
		{
			TreeView_SetItemText( hw, idx, column, &itemText[0] );
		}

		//
		// Find item by position, return index
		//
		static int findItem( const HWND hw, const POINT& pt )
		{
			LVHITTESTINFO lvhti = { 0 };
			lvhti.flags = LVHT_ONITEM;
			lvhti.pt = pt;

			return (int)SendMessage( hw, LVM_HITTEST, 0, reinterpret_cast<LPARAM>( &lvhti ) );
		}

		//
		// Find item by text, return index
		//
		static int findItem( const HWND hw, const TCHAR *itemName )
		{
			LVFINDINFO lvfi = { 0 };
			lvfi.flags = LVFI_STRING;
			lvfi.psz = itemName;

			return TreeView_FindItem( hw, -1, &lvfi );
		}

		//
		// Select and focus item by name
		//
		static int selectItem( const HWND hw, const TCHAR *itemName )
		{
			int idx = findItem( hw, itemName );
			selectItem( hw, idx );
			return idx;
		}

		//
		// Select all items in tree-view
		//
		static void selectAllItems( const HWND hw )
		{
			int index = -1;
			while( ( index = TreeView_GetNextItem( hw, index, LVNI_ALL ) ) != -1 )
			{
				auto state = TreeView_GetItemState( hw, index, LVIS_CUT | LVIS_FOCUSED | LVIS_SELECTED );
				setItemState( hw, index, state | LVNI_SELECTED );
			}
		}

		//
		// Add new column into tree-view
		//
		static int addColumn( const HWND hw, std::wstring columnName, int width = 100, bool allignLeft = true )
		{
			LVCOLUMN column = { 0 };
			column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			column.pszText = &columnName[0];
			column.cx = width;
			column.fmt = allignLeft ? LVCFMT_LEFT : LVCFMT_RIGHT;

			return TreeView_InsertColumn( hw, Header_GetItemCount( TreeView_GetHeader( hw ) ), &column );
		}

		//
		// Get tree-view column width
		//
		static int getColumnWidth( const HWND hw, int column = 0 )
		{
			return TreeView_GetColumnWidth( hw, column );
		}

		//
		// Set tree-view column width
		//
		static bool setColumnWidth( const HWND hw, int width, int column = 0 )
		{
			static int COLUMN_PADDING = 30;
			return TreeView_SetColumnWidth( hw, column, width + COLUMN_PADDING ) ? true : false;
		}

		//
		// Get width of tree-view item's text
		//
		static int getStringWidth( const HWND hw, const TCHAR *itemText )
		{
			return TreeView_GetStringWidth( hw, itemText );
		}

		//
		// Get item position - when keyboard used
		//
		static bool getItemPosition( const HWND hw, const size_t itemIndex, POINT *pt )
		{
			if( itemIndex != -1 && pt->x == -1 && pt->y == -1 )
			{
				// calculate item position
				RECT rctList = { 0 }, rctItem = { 0 };
				GetWindowRect( hw, &rctList );
				getItemRect( hw, itemIndex, &rctItem, LVIR_LABEL );

				const LONG PADDING = 2;
				pt->x = rctList.left + rctItem.left + PADDING;
				pt->y = rctList.top + rctItem.bottom + PADDING;

				return true;
			}

			return false;
		}

		//
		// Count rows in tree-view column
		//
		static int countRows( const HWND hw )
		{
			RECT rct, rctTmp;
			getItemRect( hw, 0, &rct, LVIR_ICON );

			int count = 1;
			for( int i = 1; i < getItemCount( hw ); ++i )
			{
				getItemRect( hw, i, &rctTmp, LVIR_ICON );

				if( rct.left == rctTmp.left )
					++count;
				else
					break;
			}

			return count;
		}*/

		// do not instantiate this class/struct
		TvUtils() = delete;
		TvUtils( TvUtils const& ) = delete;
		void operator=( TvUtils const& ) = delete;
	};
}
