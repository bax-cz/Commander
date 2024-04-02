#pragma once

/*
	TabView utils - helper functions for dealing with tab-controls
*/

namespace Commander
{
	struct TabUtils
	{
		//
		// Set extended style 
		//
		static void setExtStyle( const HWND hw, const DWORD extStyleNew )
		{
			LONG_PTR exStyle = SendMessage( hw, TB_GETEXTENDEDSTYLE, 0, 0 );
			exStyle |= extStyleNew;
			SendMessage( hw, TB_SETEXTENDEDSTYLE, 0, (LPARAM)exStyle );
		}

		//
		// Tab-view insert new item
		//
		static void insertItem( HWND hw, const std::wstring& label )
		{
			TCITEM tci = { 0 };
			tci.mask = TCIF_TEXT;
			tci.pszText = const_cast<LPWSTR>( &label[0] );
			tci.cchTextMax = (int)wcslen(tci.pszText);

			TabCtrl_InsertItem( hw, TabCtrl_GetItemCount( hw ), (LPARAM)&tci );
		}

		//
		// Tab-view remove existing item
		//
		static void removeItem( HWND hw, int index )
		{
			TabCtrl_DeleteItem( hw, index );
		}

		//
		// Tab-view set item label
		//
		static void setItemLabel( HWND hw, int index, const std::wstring& label )
		{
			TCITEM tci = { 0 };
			tci.mask = TCIF_TEXT;
			tci.pszText = const_cast<LPWSTR>( &label[0] );
			tci.cchTextMax = (int)wcslen(tci.pszText);

			TabCtrl_SetItem( hw, index, (LPARAM)&tci );
		}

		//
		// Tab-view find item index at position
		//
		static int findItem( HWND hw, POINT& pt )
		{
			TCHITTESTINFO tchti = { 0 };
			tchti.flags = TCHT_ONITEM;
			tchti.pt = pt;

			return (int)SendMessage( hw, TCM_HITTEST, 0, reinterpret_cast<LPARAM>( &tchti ) );
		}


		// do not instantiate this class/struct
		TabUtils() = delete;
		TabUtils( TabUtils const& ) = delete;
		void operator=( TabUtils const& ) = delete;
	};
}
