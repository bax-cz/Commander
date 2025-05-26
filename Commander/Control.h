#pragma once

/*
	Abstract class for panel's UI controls
*/

namespace Commander
{
	// forward declaration
	class CFcPanel;
	class CPanelTab;

	class CControl
	{
	public:
		CControl();
		virtual ~CControl() = default;

		void updateLayout();
		void show( int cmdShow = SW_SHOW );
		void focus();

		HWND getHwnd() { return _hWnd; }
		RECT& getRect() { return _rect; }

		virtual bool init( CFcPanel *const panel, CPanelTab *const panelTab ) = 0;
		virtual LRESULT CALLBACK wndProc( UINT message, WPARAM wParam, LPARAM lParam ) = 0;

	public:
		// responsible for routing messages to appropriate child windows
		static LRESULT CALLBACK controlsWndProcMain( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	protected:
		void registerWndProc( CFcPanel *const panel, CPanelTab *const panelTab, CControl *const control );

	protected:
		std::wstring _controlUserData;

		CFcPanel    *_pParentPanel;
		CPanelTab   *_pParentTab;

		WNDPROC      _wndProcDefault;
		HWND         _hWnd;
		HWND         _hWndTip;
		RECT         _rect;
	};
}
