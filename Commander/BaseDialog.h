#pragma once

#include <gdiplus.h>

#include "Resource.h"

namespace Commander
{
	class CBaseDialog
	{
	public:
		CBaseDialog() : _hDlg( NULL ), _hDlgParent( NULL ), _dlgUserDataPtr( NULL ), _isFullscreen( false ) {}
		virtual ~CBaseDialog() = default;

		virtual void onInit() { ShowWindow( _hDlg, SW_SHOWNORMAL ); }
		virtual bool onOk() { return true; }
		virtual bool onClose() { return true; }
		virtual void onDestroy() { }

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) { return (INT_PTR)false; }

	public:
		inline std::wstring result() const { return _dlgResult; }
		// intended for modeless dialogs
		inline bool close( bool force = false ) const { return SendMessage( _hDlg, WM_CLOSE, force, 0 ) == 0; }
		inline bool destroy() const { return !!DestroyWindow( _hDlg ); }
		inline bool visible() const { return !!IsWindowVisible( _hDlg ); }
		inline bool show( int nShow = SW_SHOWNORMAL ) const { return !!ShowWindow( _hDlg, nShow ); }
		inline bool fullscreen( bool gofull = true )
		{
			if( !_isFullscreen )
			{
				_isMaximized = !!IsZoomed( _hDlg );
				if( _isMaximized ) // hack: when zoomed, taskbar might not hide itself
					SendMessage( _hDlg, WM_SYSCOMMAND, SC_RESTORE, 0 );

				GetWindowRect( _hDlg, &_dlgRect );
				_dlgStyle = GetWindowStyle( _hDlg );
				_dlgStyleEx = GetWindowExStyle( _hDlg );
			}

			_isFullscreen = gofull;

			if( gofull )
			{
				SetWindowLongPtr( _hDlg, GWL_STYLE, _dlgStyle & ~( WS_CAPTION | WS_THICKFRAME ) );
				SetWindowLongPtr( _hDlg, GWL_EXSTYLE, _dlgStyleEx & ~( WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE ) );

				MONITORINFO mi = { sizeof( mi ) };
				GetMonitorInfo( MonitorFromWindow( _hDlg, MONITOR_DEFAULTTONEAREST), &mi );

				Gdiplus::Rect rct{ mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top };
				SetWindowPos( _hDlg, NULL, rct.X, rct.Y, rct.Width, rct.Height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
			}
			else
			{
				// restore dialog's state
				SetWindowLongPtr( _hDlg, GWL_STYLE, _dlgStyle );
				SetWindowLongPtr( _hDlg, GWL_EXSTYLE, _dlgStyleEx );

				Gdiplus::Rect rct{ _dlgRect.left, _dlgRect.top, _dlgRect.right - _dlgRect.left, _dlgRect.bottom - _dlgRect.top };
				SetWindowPos( _hDlg, NULL, rct.X, rct.Y, rct.Width, rct.Height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );

				if( _isMaximized )
					SendMessage( _hDlg, WM_SYSCOMMAND, SC_MAXIMIZE, 0 );
			}

			return _isFullscreen;
		}

	protected:
		HWND _hDlg;
		HWND _hDlgParent;
		LPARAM _dlgUserDataPtr;
		bool _isFullscreen;
		std::wstring _dlgResult;

	private:
		LONG_PTR _dlgStyle;
		LONG_PTR _dlgStyleEx;
		RECT _dlgRect;
		bool _isMaximized;

	public:
		//
		// Create modeless dialog and store its instance
		//
		template<typename T>
		static T *createModeless( HWND hWndParent = NULL, LPARAM userDataPtr = NULL )
		{
			T *inst = new T;
			inst->_hDlgParent = hWndParent;
			inst->_dlgUserDataPtr = userDataPtr;

			FCS::inst().getApp().getDialogs().push_back( inst );

			CreateDialogParam( FCS::inst().getFcInstance(), MAKEINTRESOURCE( T::resouceIdTemplate ), hWndParent, &_dlgProcModeless<T>, reinterpret_cast<LPARAM>( inst ) );

			return inst;
		}

		//
		// Create modal dialog
		//
		template<typename T>
		static std::wstring createModal( HWND hWndParent = NULL, LPARAM userDataPtr = NULL )
		{
			T *inst = new T;
			inst->_hDlgParent = hWndParent;
			inst->_dlgUserDataPtr = userDataPtr;

			std::wstring dlgResult;

			if( DialogBoxParam( FCS::inst().getFcInstance(), MAKEINTRESOURCE( T::resouceIdTemplate ), hWndParent, &_dlgProcModal<T>, reinterpret_cast<LPARAM>( inst ) ) == IDOK )
			{
				dlgResult = inst->_dlgResult;
			}

			delete inst;

			return dlgResult;
		}

		//
		// Global modeless dialog's procedure as template function
		//
		template<typename T>
		static INT_PTR CALLBACK _dlgProcModeless( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
		{
			T *dialogInstance = reinterpret_cast<T*>( GetWindowLongPtr( hDlg, DWLP_USER ) );

			switch( message )
			{
			case WM_ACTIVATE:
				FCS::inst().setActiveDialog( LOWORD( wParam ) == WA_INACTIVE ? NULL : hDlg );
				break;

			case WM_SHOWWINDOW: // TODO ?
				break;

			case WM_INITDIALOG: // store this dialog instance in dialog's user data
				dialogInstance = reinterpret_cast<T*>( lParam );
				SetWindowLongPtr( hDlg, DWLP_USER, lParam );
				dialogInstance->_hDlg = hDlg;
				dialogInstance->onInit();
				return (INT_PTR)true;

			case WM_COMMAND:
				switch( LOWORD( wParam ) )
				{
				case IDOK:
					if( dialogInstance->onOk() )
						return (INT_PTR)dialogInstance->close();
					break;

				case IDCANCEL:
					return (INT_PTR)dialogInstance->close();
				}
				break;

			case WM_CLOSE:
				if( dialogInstance->onClose() || wParam /* force close */ )
				{
					DestroyWindow( hDlg );
				}
				return (INT_PTR)true;

			case WM_DESTROY:
				dialogInstance->onDestroy();
				break;

			case WM_NCDESTROY:
				SetWindowLongPtr( hDlg, DWLP_USER, NULL );
				FCS::inst().getApp().getDialogs().remove( dialogInstance );
				delete dialogInstance; dialogInstance = nullptr;
				return (INT_PTR)false;

			default:
				break;
			}

			if( dialogInstance )
				return dialogInstance->dialogProc( message, wParam, lParam );

			return (INT_PTR)false;
		}

		//
		// Global modal dialog's procedure as template function
		//
		template<typename T>
		static INT_PTR CALLBACK _dlgProcModal( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
		{
			auto dialogInstance = reinterpret_cast<T*>( GetWindowLongPtr( hDlg, DWLP_USER ) );

			switch( message )
			{
			case WM_ACTIVATE:
				// TODO: what to do with multiple modal dialogs(e.g. find text in file viewer)!?
				break;

			case WM_INITDIALOG: // store this dialog instance in dialog's user data
				dialogInstance = reinterpret_cast<T*>( lParam );
				SetWindowLongPtr( hDlg, DWLP_USER, lParam );
				dialogInstance->_hDlg = hDlg;
				dialogInstance->onInit();
				return (INT_PTR)true;

			case WM_COMMAND:
				switch( LOWORD( wParam ) )
				{
				case IDOK:
					if( dialogInstance->onOk() )
					{
						EndDialog( hDlg, (INT_PTR)IDOK );
					}
					return (INT_PTR)true;

				case IDCANCEL:
					if( dialogInstance->onClose() )
					{
						EndDialog( hDlg, (INT_PTR)IDCANCEL );
					}
					return (INT_PTR)true;
				}
				break;

			case WM_DESTROY:
				dialogInstance->onDestroy();
				break;

			default:
				break;
			}

			if( dialogInstance )
				return dialogInstance->dialogProc( message, wParam, lParam );

			return (INT_PTR)false;
		}
	};
}
