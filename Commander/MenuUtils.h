#pragma once

/*
	Menu utils - helper functions for dealing with menus
*/

namespace Commander
{
	struct MenuUtils
	{
		//
		// Enable/disable menu item by id
		//
		static bool enableItem( HMENU hMenu, UINT itemId, bool enable = true )
		{
			return !!EnableMenuItem( hMenu, itemId, MF_BYCOMMAND | ( enable ? MF_ENABLED : MF_GRAYED ) );
		}

		//
		// Enable/disable menu item by position
		//
		static bool enableItemByPos( HMENU hMenu, UINT position, bool enable = true )
		{
			return !!EnableMenuItem( hMenu, position, MF_BYPOSITION | ( enable ? MF_ENABLED : MF_GRAYED ) );
		}

		//
		// Check/Uncheck menu item state (CheckMenuItem function returns previous state)
		//
		static bool checkItem( HMENU hMenu, UINT itemId, bool check = true )
		{
			return CheckMenuItem( hMenu, itemId, MF_BYCOMMAND | ( check ? MF_CHECKED : MF_UNCHECKED ) ) == ( check ? MF_UNCHECKED : MF_CHECKED );
		}

		//
		// Checked/Unchecked menu item state
		//
		static bool isItemChecked( HMENU hMenu, UINT itemId )
		{
			return GetMenuState( hMenu, itemId, MF_BYCOMMAND ) == MF_CHECKED;
		}

		static bool insertItem( HMENU hMenu, const std::wstring& itemText, UINT itemPos, UINT itemId, UINT flags = 0 )
		{
			return !!InsertMenu( hMenu, itemPos, MF_BYPOSITION | flags, itemId, itemText.c_str() );
		}

		static bool removeItem( HMENU hMenu, UINT itemPos )
		{
			return !!RemoveMenu( hMenu, itemPos, MF_BYPOSITION );
		}

		// do not instantiate this class/struct
		MenuUtils() = delete;
		MenuUtils( MenuUtils const& ) = delete;
		void operator=( MenuUtils const& ) = delete;
	};
}
