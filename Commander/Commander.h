#pragma once

/*
	File commander singleton class
	------------------------------
	Holds following objects:
	 - CCommanderApp - takes care of UI controls, layout, etc.
	 - delegates WndProc messages to correct windows
*/

#include "resource.h"

#include "FastCopyManager.h"
#include "CommanderApp.h"

#if defined(_DEBUG) || defined(FC_ENABLE_LOGGING)
  #define PrintDebug( format, ... ) _printDebugImpl( __FUNCTIONW__, _T(format), __VA_ARGS__ )
#else
  #define PrintDebug __noop
#endif

#if defined(_DEBUG) || defined(FC_ENABLE_LOGGING)
void _printDebugImpl( const wchar_t *funcName, const wchar_t *fmt, ... );
#endif

#include "SystemUtils.h"

#if defined(_DEBUG) || defined(FC_ENABLE_LOGGING)
#include "LoggerLite.h"
#endif

/* Format string - printf style */
#define FORMAT( format, ... ) formatStrImpl( (format), __VA_ARGS__ )

namespace Commander
{
	// Commander's temporary data storage directory
	extern std::wstring g_tempPath;

	// Commander's main window procedure
	LRESULT CALLBACK wndProcFcMain( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	// Format string helper function
	std::wstring formatStrImpl( const wchar_t *fmt, ... );

	// Responsible for Menu/Toolbar command processing
	bool processMenuCommand( WORD itemId );

	//
	// Singleton class for Commander
	//
	class FCS
	{
	public:
		//
		// Returns singleton instance
		//
		static FCS& inst()
		{
			static FCS _instance;
			return _instance;
		}

	public:
		//
		// Gets/sets Commander's instance handle
		//
		inline HINSTANCE getFcInstance() const { return _hInstFc; }
		inline void setFcInstance( const HINSTANCE hInst ) { _hInstFc = hInst; }

		//
		// Gets/sets handle to main Commander's window
		//
		inline HWND getFcWindow() const { return _hWndFc; }
		inline void setFcWindow( const HWND hWnd ) { _hWndFc = hWnd; }

		//
		// Gets/sets handle to currently active modeless dialog
		//
		inline HWND getActiveDialog() const { return _hWndDlg; }
		inline void setActiveDialog( const HWND hDlg ) { _hWndDlg = hDlg; }

		//
		// Returns reference to FastCopy manager
		//
		inline CFastCopyManager& getFastCopyManager() { return _fastCopyMngr; }

		//
		// Returns reference to Commander Application object
		//
		inline CCommanderApp& getApp() { return _commanderApp; }

		//
		// Returns reference to temporary directory
		//
		inline const std::wstring& getTempPath() { return g_tempPath; }

	private:
		//
		// Private constructor/destructor
		//
		FCS()
		{
			_hInstFc = nullptr;
			_hWndFc  = nullptr;
			_hWndDlg = nullptr;
		}

		~FCS()
		{
			PrintDebug("FCS destroyed");
		}

		HINSTANCE _hInstFc;  // Commander instance
		HWND      _hWndFc;   // handle to main Commander window
		HWND      _hWndDlg;  // handle to currently active modeless dialog

		// FastCopy manager
		CFastCopyManager _fastCopyMngr;

		// Commander Application object
		CCommanderApp _commanderApp;

	public:
		// stop the compiler generating methods of copy the object
		FCS( FCS const& ) = delete;
		void operator=( FCS const& ) = delete;
	};
}
