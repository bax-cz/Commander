#include "stdafx.h"

#include "Commander.h"
#include "ShellContextMenu.h"

#include "FileSystemUtils.h"
#include "StringUtils.h"
#include "ShellUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CShellContextMenu::CShellContextMenu()
		: SHELLCMD_MIN( 0x0001 )
		, SHELLCMD_MAX( 0x7FFF )
	{
		_contextMenu = NULL;
		_contextMenuType = 0;
	}

	CShellContextMenu::~CShellContextMenu()
	{
		ShellUtils::safeRelease( &_contextMenu );
	}


	//
	// Get context menu interface in highest version possible
	//
	bool CShellContextMenu::getContextMenu( LPSHELLFOLDER shellFolder, LPCITEMIDLIST *pidls, size_t pidlsCount )
	{
		LPCONTEXTMENU icm = NULL;
		_contextMenu = NULL;
	
		if( shellFolder && pidls && pidlsCount )
		{
			shellFolder->GetUIObjectOf( NULL, (UINT)pidlsCount, pidls, IID_IContextMenu, NULL, (void**)&icm );

			if( icm )
			{
				if( icm->QueryInterface( IID_IContextMenu3, reinterpret_cast<void**>( &_contextMenu ) ) == NOERROR )
				{
					_contextMenuType = 3;
				}
				else if( icm->QueryInterface( IID_IContextMenu2, reinterpret_cast<void**>( &_contextMenu ) ) == NOERROR )
				{
					_contextMenuType = 2;
				}

				if( _contextMenu )
				{
					icm->Release();
				}
				else 
				{	
					_contextMenuType = 1;
					_contextMenu = icm;
				}

				return true;
			}
		}

		return false;
	}

	UINT CShellContextMenu::addCustomCommand( const std::wstring& caption, UINT id )
	{
		_customCommands[id] = caption;

		return static_cast<UINT>( _customCommands.size() );
	}

	//
	// Handle context menu messages
	//
	LRESULT CShellContextMenu::handleMessages( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{ 
		case WM_MENUCHAR:
			if( _contextMenu && _contextMenuType == 3 ) // only supported by IContextMenu3
			{
				LRESULT lResult = 0;
				reinterpret_cast<IContextMenu3*>( _contextMenu )->HandleMenuMsg2( message, wParam, lParam, &lResult );
				return lResult;
			}
			break;

		case WM_DRAWITEM:
		case WM_MEASUREITEM:
			if( wParam )
			{
				break;
			}

		case WM_INITMENUPOPUP:
			if( _contextMenu && _contextMenuType == 2 )
			{
				reinterpret_cast<IContextMenu2*>( _contextMenu )->HandleMenuMsg( message, wParam, lParam );
			}
			else if( _contextMenu && _contextMenuType == 3 )
			{
				LRESULT lResult = 0;
				reinterpret_cast<IContextMenu3*>( _contextMenu )->HandleMenuMsg2( message, wParam, lParam, &lResult );
				return lResult;
			}
			return ( message == WM_INITMENUPOPUP ) ? 0 : TRUE;

		case WM_MENUSELECT:
			if( _contextMenu && _contextMenuType == 3 )
			{
				UINT uItem = (UINT)LOWORD( wParam );
				if( ( HIWORD( wParam ) & MF_POPUP ) == 0 && uItem >= SHELLCMD_MIN && uItem <= SHELLCMD_MAX )
				{
					// if this is a shell item, get its verb text
					TCHAR szBuf[MAX_PATH];
					auto icm = reinterpret_cast<IContextMenu3*>( _contextMenu );
					icm->GetCommandString( uItem - SHELLCMD_MIN, GCS_VERB, NULL, (char*)szBuf, MAX_PATH );
					_cmdVerb = szBuf;
				}
			}
			break;

		default:
			break;
		}

		return FALSE;
	}


	//
	// Get result command verb
	//
	std::string CShellContextMenu::getResultVerb()
	{
		return StringUtils::convert2A( _cmdVerb );
	}

	//
	// Show context menu for given items
	//
	UINT CShellContextMenu::show( HWND hWnd, const std::vector<std::wstring>& items, const POINT& pt )
	{
		if( _contextMenu )
		{
			// To prevent creating menu when WM_CONTEXTMENU message is fired twice - fixme??
			return 0;
		}

		_cmdVerb.clear();

		ShellUtils::TPidl pidls;
		auto shellFolder = ShellUtils::getPidlsFromFileList( items, pidls );

		// get context menu interface
		if( !shellFolder || !getContextMenu( shellFolder, (LPCITEMIDLIST*)&pidls[0], pidls.size() ) )
		{
			return 0;
		}

		// create and show shell context menu
		auto hMenu = CreatePopupMenu();
		auto extended = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
		auto flags = CMF_NORMAL | CMF_EXPLORE | CMF_ASYNCVERBSTATE | ( extended ? CMF_EXTENDEDVERBS : 0 );
		_contextMenu->QueryContextMenu( hMenu, 0, SHELLCMD_MIN, SHELLCMD_MAX, flags );

		// insert custom commands into the menu
		if( !_customCommands.empty() )
		{
			int cnt = GetMenuItemCount( hMenu );

			MENUITEMINFO mii = { 0 };
			mii.cbSize = sizeof( MENUITEMINFO );
			mii.fMask = MIIM_FTYPE;

			// try to find the first separator in the context menu
			UINT idx;
			for( idx = 0; idx < cnt; )
			{
				if( GetMenuItemInfo( hMenu, idx++, MF_BYPOSITION, &mii ) && mii.fType == MFT_SEPARATOR )
					break;
			}

			// insert custom commands
			for( auto it = _customCommands.begin(); it != _customCommands.end(); ++it )
			{
				InsertMenu( hMenu, idx++, MF_BYPOSITION | MF_STRING, SHELLCMD_MAX + it->first, it->second.c_str() );
			}

			InsertMenu( hMenu, idx, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );

			_customCommands.clear();
		}

		UINT cmdId = TrackPopupMenu( hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL );

		// process system shell comands
		if( cmdId >= SHELLCMD_MIN && cmdId <= SHELLCMD_MAX )
		{
			ShellUtils::invokeCommand( _contextMenu, ( cmdId - SHELLCMD_MIN ), pt );
		}

		// clean up and free data
		ShellUtils::freePidls( pidls );
		ShellUtils::safeRelease( &_contextMenu );
		ShellUtils::safeRelease( &shellFolder );

		if( hMenu )
		{
			DestroyMenu( hMenu );
			hMenu = NULL;
		}

		return ( cmdId > SHELLCMD_MAX ) ? cmdId - SHELLCMD_MAX : (UINT)~0;
	}
}
