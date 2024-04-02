#pragma once

/*
	CShellContextMenu - provides context menu functionality for files, folders, etc.
*/

#include <objbase.h>
#include <shlobj.h>
#include <string>
#include <vector>

namespace Commander
{
	class CShellContextMenu  
	{
	private:
		const UINT SHELLCMD_MIN;
		const UINT SHELLCMD_MAX;

	public:
		CShellContextMenu();
		virtual ~CShellContextMenu();

	public:
		UINT addCustomCommand( const std::wstring& caption, UINT id );
		UINT show( HWND hWnd, const std::vector<std::wstring>& items, const POINT& pt );
		LRESULT handleMessages( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
		std::string getResultVerb();

	private:
		bool getContextMenu( LPSHELLFOLDER shellFolder, LPCITEMIDLIST *pidls, size_t pidlsCount );

	private:
		LPCONTEXTMENU                _contextMenu;
		int                          _contextMenuType;
		std::wstring                 _cmdVerb;
		std::map<UINT, std::wstring> _customCommands;
	};
}
