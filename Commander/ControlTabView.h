#pragma once

#include "Control.h"

namespace Commander
{
	//
	// Auxiliary class for UI tab-view control
	//
	class CTabView : public CControl
	{
	public:
		CTabView();
		~CTabView();

		virtual bool init( CFcPanel *const panel, CPanelTab *const panelTab = nullptr ) override;
		virtual LRESULT CALLBACK wndProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

		void cleanUp();

		inline int getCount() { return static_cast<int>( _tabs.size() ); }

		inline int getCurSel() { return TabCtrl_GetCurSel( _hWnd ); }
		inline void setCurSel( int idx ) { TabCtrl_SetCurSel( _hWnd, idx ); }

		inline std::shared_ptr<CPanelTab> getActive() { return _spActive; }
		CPanelTab *setActive( int index );

		void setLabel( const std::wstring& label, DWORD tabId );
		void createTab( const std::wstring& path, const std::wstring& itemName, bool active = true );
		void removeTab( int index );

	private:
		// responsible for routing messages to appropriate child windows
		static LRESULT CALLBACK _wndProcSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
		{
			CTabView *tabView = reinterpret_cast<CTabView*>( GetWindowLongPtr( hWnd, GWLP_USERDATA ) );
			return tabView->wndProc( message, wParam, lParam );
		}

	private:
		LRESULT onNotifyMessage( WPARAM wParam, LPARAM lParam );

	private:
		DWORD _idTab;

		std::vector<std::shared_ptr<CPanelTab>> _tabs;
		std::shared_ptr<CPanelTab> _spActive;
	};
}
