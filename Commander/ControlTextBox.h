#pragma once

#include "Control.h"

namespace Commander
{
	//
	// Auxiliary class for UI Text-box control
	//
	class CTextBox : public CControl
	{
	public:
		CTextBox();
		~CTextBox();

		virtual bool init( CFcPanel *const panel, CPanelTab *const panelTab = nullptr ) override;
		virtual LRESULT CALLBACK wndProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

		void updateText( const std::wstring& text );
		void updateToolTip( const std::wstring& tip );
	};
}
