#pragma once

#include "Control.h"

namespace Commander
{
	//
	// Auxiliary class for UI tool-bar control
	//
	class CToolBar : public CControl
	{
	public:
		CToolBar();
		~CToolBar();

		virtual bool init( CFcPanel *const panel, CPanelTab *const panelTab = nullptr ) override;
		virtual LRESULT CALLBACK wndProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

		inline int getImageIndex() { return _imageIndex; }

		void showDropDownList(); // let user select drive
		void updateToolbarImage( const std::wstring& reqDrive );

	private:
		void showDropDownMenu( LPNMTOOLBAR tbHdr );
		void addToolbarButton();
		void handleCommands( UINT cmdId );

	private:
		int _imageIndex;
	};
}
