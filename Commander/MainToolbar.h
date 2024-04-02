#pragma once

/*
	File commander's main tool-bar
*/

#include "ToolBarUtils.h"

namespace Commander
{
	class CMainToolbar
	{
	public:
		CMainToolbar();
		~CMainToolbar();

		inline HWND getHandle() const { return _hToolBar; }
		inline void updateLayout() { SendMessage( _hToolBar, TB_AUTOSIZE, 0, 0 ); }

	public:
		void init();

	private:
		void initImageLists();
		void addToolbarButtons();

	private:
		HWND _hToolBar;

		HIMAGELIST _imgListToolbar;
		HIMAGELIST _imgListToolbarHot;
		HIMAGELIST _imgListToolbarDisabled;
	};
}
