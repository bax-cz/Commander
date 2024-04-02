#pragma once

/*
	List-View utils - helper functions to make life with list-view easier
*/

namespace Commander
{
	struct LvUtils
	{
		//
		// Set extended style 
		//
		static void setExtStyle( const HWND hw, const DWORD extStyleNew )
		{
			LONG_PTR exStyle = ListView_GetExtendedListViewStyle( hw );
			exStyle |= extStyleNew;
			ListView_SetExtendedListViewStyle( hw, exStyle );
		}

		//
		// Delete all items
		//
		static int cleanUp( const HWND hw ) { return ListView_DeleteAllItems( hw ); }

		//
		// Disable redrawing during update
		//
		static void beginUpdate( const HWND hw ) { SendMessage( hw, WM_SETREDRAW, static_cast<WPARAM>( false ), 0 ); }

		//
		// Enable redrawing after update finished
		//
		static void endUpdate( const HWND hw ) { SendMessage( hw, WM_SETREDRAW, static_cast<WPARAM>( true ), 0 ); }

		//
		// Set list-view image-list
		//
		static HIMAGELIST setImageList( const HWND hw, const HIMAGELIST himl, int type ) { return ListView_SetImageList( hw, himl, type ); }

		//
		// Initiate label edit
		//
		static HWND editItemLabel( const HWND hw, int itemIndex ) { return ListView_EditLabel( hw, itemIndex ); }

		//
		// Cancel label edit
		//
		static void cancelEditItemLabel( const HWND hw ) { ListView_CancelEditLabel( hw ); }

		//
		// Delete item from list-view
		//
		static void deleteItem( const HWND hw, int itemIndex ) { ListView_DeleteItem( hw, itemIndex ); }

		//
		// Get virtual number of items
		//
		static int getItemCount( const HWND hw ) { return ListView_GetItemCount( hw ); }

		//
		// Set virtual number of items
		//
		static void setItemCount( const HWND hw, const size_t count, const DWORD flags ) { ListView_SetItemCountEx( hw, count, flags ); }

		//
		// Get select-marked item
		//
		static int getMarkedItem( const HWND hw ) { return ListView_GetSelectionMark( hw ); }

		//
		// Get selected and also focused item
		//
		static int getSelectedItem( const HWND hw ) { return ListView_GetNextItem( hw, -1, LVNI_SELECTED | LVNI_FOCUSED ); }

		//
		// Get only focused item
		//
		static int getFocusedItem( const HWND hw ) { return ListView_GetNextItem( hw, -1, LVNI_FOCUSED ); }

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
			return ListView_GetItemRect( hw, itemIndex, rct, code ) ? true : false;
		}

		//
		// Set state of the item - selected, focused, cut.
		//
		static void setItemState( const HWND hw, int itemIndex, const DWORD flags )
		{
			// last parameter is bitmask, telling list-view which bits we want to set
			ListView_SetItemState( hw, itemIndex, flags, LVIS_CUT | LVIS_FOCUSED | LVIS_SELECTED );
		}

		//
		// Ensure that the item is visible
		//
		static void ensureVisible( const HWND hw, int itemIndex, const bool noRedraw = false )
		{
			if( noRedraw )
				beginUpdate( hw );

			ListView_EnsureVisible( hw, itemIndex, false ); // we don't want only partial view

			if( noRedraw )
				endUpdate( hw );
		}

		//
		// Get item text at given index
		//
		static std::wstring getItemText( const HWND hw, const size_t itemIndex, int column = 0 )
		{
			TCHAR itemText[MAX_PATH] = { 0 };
			ListView_GetItemText( hw, itemIndex, column, itemText, MAX_PATH );
			return std::wstring( itemText );
		}

		//
		// Get selected items array
		//
		static std::vector<int> getSelectedItems( const HWND hw )
		{
			std::vector<int> indices;

			int index = -1;
			while( ( index = ListView_GetNextItem( hw, index, LVNI_SELECTED ) ) != -1 )
				indices.push_back( index );

			return indices;
		}

		//
		// Add item to list-view
		//
		static void addItem( const HWND hw, std::wstring itemName, int itemIndex, int imgIndex )
		{
			LV_ITEM item = { 0 };
			item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_INDENT | LVIF_STATE;
			item.iItem = itemIndex;
			item.iImage = imgIndex;
			item.pszText = &itemName[0];
			item.cchTextMax = 256;

			ListView_InsertItem( hw, &item );
		}

		//
		// Add subitem to list-view
		//
		static void addSubItemText( const HWND hw, int idx, int column, std::wstring itemText )
		{
			ListView_SetItemText( hw, idx, column, &itemText[0] );
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

			return ListView_FindItem( hw, -1, &lvfi );
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
		// Select all items in list-view
		//
		static void selectAllItems( const HWND hw )
		{
			int index = -1;
			while( ( index = ListView_GetNextItem( hw, index, LVNI_ALL ) ) != -1 )
			{
				auto state = ListView_GetItemState( hw, index, LVIS_CUT | LVIS_FOCUSED | LVIS_SELECTED );
				setItemState( hw, index, state | LVNI_SELECTED );
			}
		}

		//
		// Add new column into list-view
		//
		static int addColumn( const HWND hw, std::wstring columnName, int width = 100, bool allignLeft = true )
		{
			LVCOLUMN column = { 0 };
			column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			column.pszText = &columnName[0];
			column.cx = width;
			column.fmt = allignLeft ? LVCFMT_LEFT : LVCFMT_RIGHT;

			return ListView_InsertColumn( hw, Header_GetItemCount( ListView_GetHeader( hw ) ), &column );
		}

		//
		// Get list-view column width
		//
		static int getColumnWidth( const HWND hw, int column = 0 )
		{
			return ListView_GetColumnWidth( hw, column );
		}

		//
		// Set list-view column width
		//
		static bool setColumnWidth( const HWND hw, int width, int column = 0 )
		{
			static int COLUMN_PADDING = 30;
			return ListView_SetColumnWidth( hw, column, width + COLUMN_PADDING ) ? true : false;
		}

		//
		// Get width of list-view item's text
		//
		static int getStringWidth( const HWND hw, const TCHAR *itemText )
		{
			return ListView_GetStringWidth( hw, itemText );
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
		// Count rows in list-view column
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
		}

		// do not instantiate this class/struct
		LvUtils() = delete;
		LvUtils( LvUtils const& ) = delete;
		void operator=( LvUtils const& ) = delete;
	};
}
