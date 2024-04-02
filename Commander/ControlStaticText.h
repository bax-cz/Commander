#pragma once

#include "Control.h"

namespace Commander
{
	//
	// Auxiliary class for UI Static-text control
	//
	class CStaticText : public CControl
	{
	public:
		CStaticText();
		~CStaticText();

		virtual bool init( CFcPanel *const panel, CPanelTab *const panelTab = nullptr ) override;
		virtual LRESULT CALLBACK wndProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

		void updateText( const std::wstring& properties );
		void updateToolTip( const std::wstring& tip );
	};
}
