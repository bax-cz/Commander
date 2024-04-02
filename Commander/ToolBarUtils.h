#pragma once

/*
	ToolBar utils - helper functions for dealing with toolbars
*/

namespace Commander
{
	struct TbUtils
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
		// Toolbar enable/disable item
		//
		static bool enableItem( HWND hToolBar, UINT itemId, bool enable = true )
		{
			return !!SendMessage( hToolBar, TB_ENABLEBUTTON, (WPARAM)itemId, (LPARAM)LOWORD( enable ) );
		}


		// do not instantiate this class/struct
		TbUtils() = delete;
		TbUtils( TbUtils const& ) = delete;
		void operator=( TbUtils const& ) = delete;
	};
}
