#pragma once

#include "Control.h"
#include "DataManager.h"
#include "ListViewUtils.h"
#include "ShellContextMenu.h"
#include "DragDropUtils.h"

namespace Commander
{
	//
	// Auxiliary class for panel items List-view control
	//
	class CListView: public CControl, public CDragDropHelper
	{
	public:
		CListView();
		~CListView();

		virtual bool init( CFcPanel *const panel, CPanelTab *const panelTab ) override;
		virtual LRESULT CALLBACK wndProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

		LRESULT CALLBACK wndProcQuickFind( UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId );

		// IUnknown interface methods - for drag&drop
		inline IFACEMETHODIMP QueryInterface( REFIID riid, void **ppv ) override {
			static const QITAB qit[] = { QITABENT( CListView, IDropTarget ), { 0 } };
			return QISearch( this, qit, riid, ppv );
		}

		inline IFACEMETHODIMP_(ULONG) AddRef() override {
			return InterlockedIncrement( &_cRef );
		}

		inline IFACEMETHODIMP_(ULONG) Release() override {
			long cRef = InterlockedDecrement( &_cRef );
			if( !cRef )
				delete this;
			return cRef;
		}

	private:
		virtual bool onDrop( const std::vector<std::wstring>& items, EFcCommand cmd ) override;

	public:
		void updateItems( const std::wstring& focusItemName = L"" );
		void updateProperties();
		int  getFocusedItemIndex() { return _focusedItemIndex; }
		void setViewMode( EViewMode mode );
		void markItems( const std::wstring& mask, bool select = true, bool filesOnly = true );
		void markFocusedItem();
		void invertMarkedItems( bool filesOnly = true );
		void refresh();
		void cutSelectedItems();
		bool isItemMarked( int itemIndex );
		bool isItemCut( int itemIndex );
		bool isQuickFindMessage( LPMSG msg );

	public:
		inline bool isInBriefMode()    const { return _currentViewMode == EViewMode::Brief;    }
		inline bool isInDetailedMode() const { return _currentViewMode == EViewMode::Detailed; }

	private:
		void startQuickFind();
		void stopQuickFind();

		void onQuickFindChar( WCHAR ch );

		int  onQuickFindSubstring( const std::wstring& str, int from );
		void onQuickFindFocusItem( int index );

		void onQuickFindCharEditbox( WCHAR ch );
		void onQuickFindPrevious( const std::wstring& str, int from );
		void onQuickFindNext( const std::wstring& str, int from );

	private:
		bool onBeginLabelEdit( NMLVDISPINFO *lvDispInfo );
		bool onEndLabelEdit( NMLVDISPINFO *lvDispInfo );
		bool onDrawDispItems( NMLVDISPINFO *lvDisp );
		void onPrepareCache( LPNMLVCACHEHINT lvCache );
		void onSelectionChanged( LPNMLISTVIEW lvData );
		void onColumnClick( LPNMLISTVIEW lvData );
		void onBeginDrag( LPNMLISTVIEW lvData );
		bool onHandleVirtualKeys( LPNMLVKEYDOWN vKey );
		bool onHandleKeyboardChars( WPARAM charCode, LPARAM charData );
		LRESULT onNotifyMessage( WPARAM wParam, LPARAM lParam );
		LRESULT onCustomDraw( LPNMLVCUSTOMDRAW lvcd );
		UINT onContextMenu( POINT& pt );
		void openNewTab( POINT& pt );

		COLORREF getItemColor( int itemIndex );
		UINT getItemFlags( int itemIndex );

		void handleKeyboardAction( LPNMLVKEYDOWN vKey );
		bool handleKeyboardActionWithCtrl( LPNMLVKEYDOWN vKey );
		void handleKeyboardActionWithCtrlAndShift( LPNMLVKEYDOWN vKey );
		void handleKeyboardActionWithCtrlAndAlt( LPNMLVKEYDOWN vKey );
		bool handleKeyboardActionWithAlt( LPNMLVKEYDOWN vKey );
		void handleKeyboardActionWithAltAndShift( LPNMLVKEYDOWN vKey );
		bool handleKeyboardActionWithShift( LPNMLVKEYDOWN vKey );
		void handleKeyboardNavigation( LPNMLVKEYDOWN vKey );

		void updateFocusedItem( const std::wstring& itemName );
		void updateColumnWidth();

	private:
		CShellContextMenu _contextMenu;
		CDataManager *_dataMan;
		EViewMode _currentViewMode;

		std::vector<int> _cutItems;

		HWND _hWndEditQuickFind;
		bool _isQuickFindMode;

		std::wstring _quickFindStr;
		std::wstring _focusedItemName;

		int _focusedItemIndex;
		int _prevFocusedItemIndex;

		bool _markedItemsChanged;
		bool _directoryChanged;

		long _cRef;

	private:
		static LRESULT CALLBACK wndProcQuickFindEdit( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CListView*>( data )->wndProcQuickFind( message, wParam, lParam, subclassId );
		}
	};
}
