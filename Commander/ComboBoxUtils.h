#pragma once

/*
	Combo-box utils - helper functions to deal with combo in an easy way
*/

namespace Commander
{
	struct CbUtils
	{
		//
		// Set extended style 
		//
		static void setExtStyle( const HWND hw, const DWORD extStyleNew )
		{
			LONG_PTR exStyle = SendMessage( hw, CBEM_GETEXTENDEDSTYLE, 0, 0 );
			exStyle |= extStyleNew;
			SendMessage( hw, CBEM_SETEXTENDEDSTYLE, 0, (LPARAM)exStyle );
		}

		//
		// Reset combo-box content
		//
		static void cleanUp( const HWND hw ) { SendMessage( hw, CB_RESETCONTENT, 0, 0 ); }

		//
		// Get current selection index
		//
		static int getSelectedItem( const HWND hw ) { return static_cast<int>( SendMessage( hw, CB_GETCURSEL, 0, 0 ) ); }

		//
		// Show drop down combo-box
		//
		static void showDropDown( const HWND hw, const bool show ) { SendMessage( hw, CB_SHOWDROPDOWN, show, 0 ); }

		//
		// Set combo-box image-list
		//
		static void setImageList( const HWND hw, const HIMAGELIST himl ) { SendMessage( hw, CBEM_SETIMAGELIST, 0, (LPARAM)himl ); }

		//
		// Get currently selected item text
		//
		static std::wstring getSelectedItemText( const HWND hw )
		{
			int idx = getSelectedItem( hw );
			return getItemText( hw, idx );
		}

		//
		// Set current selection index
		//
		static void selectItem( const HWND hw, int itemIndex )
		{
			WPARAM wParam = static_cast<WPARAM>( itemIndex );
			SendMessage( hw, CB_SETCURSEL, wParam, 0 );
		}

		//
		// Add item to combo-box
		//
		static void addString( const HWND hw, const TCHAR *itemName )
		{
			LPARAM lParam = reinterpret_cast<LPARAM>( itemName );
			SendMessage( hw, CB_ADDSTRING, 0, lParam );
		}

		//
		// Insert item to combo-box at given index
		//
		static void insertString( const HWND hw, int index, const TCHAR *itemName, LPARAM data = NULL )
		{
			LPARAM lParam = reinterpret_cast<LPARAM>( itemName );
			SendMessage( hw, CB_INSERTSTRING, (WPARAM)index, lParam );

			if( data != NULL )
				SendMessage( hw, CB_SETITEMDATA, (WPARAM)index, data );
		}

		//
		// Get item text at given index
		//
		static std::wstring getItemText( const HWND hw, int itemIndex )
		{
			// get text length excluding null terminating character
			int textLength = static_cast<int>( SendMessage( hw, CB_GETLBTEXTLEN, itemIndex, 0 ) );

			// allocate space for item text
			TCHAR *itemText = new TCHAR[textLength + 1];
			SendMessage( hw, CB_GETLBTEXT, itemIndex, reinterpret_cast<LPARAM>( itemText ) );

			// copy to std string
			std::wstring outText = std::wstring( itemText );

			// clean up memory
			delete[] itemText; itemText = nullptr;

			return outText;
		}

		//
		// Add item to combo-box
		//
		static void addItemEx( const HWND hw, std::wstring itemName, int itemIndex, int imgIndex )
		{
			COMBOBOXEXITEM item = { 0 };
			item.mask = CBEIF_TEXT | CBEIF_INDENT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
			item.iItem = itemIndex;
			item.iImage = imgIndex;
			item.iSelectedImage = imgIndex;
			item.iIndent = 0;
			item.pszText = &itemName[0];
			item.cchTextMax = static_cast<int>( itemName.length() );

			SendMessage( hw, CBEM_INSERTITEM, 0, (LPARAM)&item );
		}

		// do not instantiate this class/struct
		CbUtils() = delete;
		CbUtils( CbUtils const& ) = delete;
		void operator=( CbUtils const& ) = delete;
	};
}
