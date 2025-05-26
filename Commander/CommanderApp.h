#pragma once

/*
	CCommanderApp - takes care of UI controls, modeless dialogs, layout, image lists, fonts, etc.
	              - holds pointers to each panel
*/

#include "Panel.h"
#include "Control.h"
#include "BaseDialog.h"
#include "BackgroundWorker.h"
#include "MainDrivesMenu.h"
#include "MainToolbar.h"
#include "OverlaysLoader.h"

#include <list>
#include <map>
#include <set>

// Dark mode support messages
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE   20
#endif

// Edit-control exstyle - when compiling against older SDKs
#ifndef EM_SETEXTENDEDSTYLE
#define EM_SETEXTENDEDSTYLE ECM_FIRST + 10
#endif // EM_SETEXTENDEDSTYLE

// Edit-control extened messages
#ifndef ES_EX_ZOOMABLE
#define ES_EX_ZOOMABLE 0x0010L
#endif // ES_EX_ZOOMABLE


///////////////////////////////////////////////////////////
// Commander user message definitions
///////////////////////////////////////////////////////////
#define UM_FIRSTMESSAGE       WM_USER + 10

#define UM_CHANGENOTIFY       UM_FIRSTMESSAGE + 0
#define UM_STATUSNOTIFY       UM_FIRSTMESSAGE + 1
#define UM_READERNOTIFY       UM_FIRSTMESSAGE + 2
#define UM_WORKERNOTIFY       UM_FIRSTMESSAGE + 3
#define UM_FSPACENOTIFY       UM_FIRSTMESSAGE + 4
#define UM_FINDLGNOTIFY       UM_FIRSTMESSAGE + 5
#define UM_WEBVW2NOTIFY       UM_FIRSTMESSAGE + 6

#define UM_OVERLAYSDONE       UM_FIRSTMESSAGE + 7
#define UM_CALCSIZEDONE       UM_FIRSTMESSAGE + 8
#define UM_ARCHOPENDONE       UM_FIRSTMESSAGE + 9
#define UM_FINDTEXTDONE       UM_FIRSTMESSAGE + 10
#define UM_NEIGHBORDONE       UM_FIRSTMESSAGE + 11

#define UM_DIRCONTENTCH       UM_FIRSTMESSAGE + 12
#define UM_CARETPOSCHNG       UM_FIRSTMESSAGE + 13
#define UM_REPORTMSGBOX       UM_FIRSTMESSAGE + 14
#define UM_FASTCOPYINFO       UM_FIRSTMESSAGE + 15

#define UM_LAST_MESSAGE       UM_FASTCOPYINFO

// FastCopy related definitions
#define AVE_WATERMARK_MIN ( 4.0   * 1024 )
#define AVE_WATERMARK_MID ( 64.0  * 1024 )

#define FC_TIMER_GUI_ID              100
#define FC_TIMER_GUI_TICK            200

#define FC_TIMER_KEEPALIVE_ID        101
#define FC_TIMER_KEEPALIVE_TICK    30000

#define FC_TIMER_OVERLAYS_ID         102
#define FC_TIMER_OVERLAYS_TICK       100

#define FC_TIMER_NEIGHBORS_ID        103
#define FC_TIMER_NEIGHBORS_TICK    20000

// Default colors definitions
#define FC_COLOR_PROGBAR           RGB( 51, 153, 255 )
#define FC_COLOR_FOCUSRECT         RGB( 232, 232, 232 )
#define FC_COLOR_GREENOK           RGB( 0, 160, 0 )
#define FC_COLOR_REDFAIL           RGB( 190, 0, 0 )
#define FC_COLOR_MARKED            RGB( 0xFF, 0x00, 0x00 )
#define FC_COLOR_COMPRESSED        RGB( 0x00, 0x00, 0xFF )
#define FC_COLOR_ENCRYPTED         RGB( 0x00, 0x7F, 0x00 )
#define FC_COLOR_HARDLINK          RGB( 0x00, 0x7F, 0x7F )
#define FC_COLOR_HIDDEN            RGB( 0x5F, 0x5F, 0x5F )
#define FC_COLOR_TEXT              RGB( 0, 0, 0 )
#define FC_COLOR_DIRBOXBKGND       RGB( 191, 205, 219 )
#define FC_COLOR_DIRBOXBKGNDWARN   RGB( 219, 191, 191 )
#define FC_COLOR_DIRBOXACTIVE      RGB( 153, 180, 209 )
#define FC_COLOR_DIRBOXACTIVEWARN  RGB( 209, 153, 153 )

// Archiver constants
#define FC_ARCHDONEFAIL          0
#define FC_ARCHDONEOK            1
#define FC_ARCHNEEDPASSWORD      2
#define FC_ARCHNEEDNEWNAME       3
#define FC_ARCHNEEDNEXTVOLUME    4
#define FC_ARCHPROCESSINGPATH    5
#define FC_ARCHPROCESSINGVOLUME  6
#define FC_ARCHBYTESTOTAL        7
#define FC_ARCHBYTESRELATIVE     8 /* Bytes processed relative */
#define FC_ARCHBYTESABSOLUTE     9 /* Bytes processed absolute */

namespace Commander
{
	//
	// Commander app object - responsible for panels creation and its controls' window procedures
	//
	class CCommanderApp
	{
	public:
		CCommanderApp();
		~CCommanderApp();

		void initApp(); // init Commander app

		inline std::map<HWND, CControl*>& getControls() { return _controlsList; }
		inline std::list<CBaseDialog*>& getDialogs() { return _dialogs; }
		inline std::vector<std::wstring>& getNeighbors() { return _neighbors; }
		inline bool neighborsFinished() const { return _neighborsFinished; }

		inline HWND  getToolbar() const { return _toolbarMain.getHandle(); }
		inline HMENU getMenuBar() const { return _hMenuBar; }
		inline HFONT getGuiFont() const { return _hGuiFont; }
		inline HFONT getViewFont() const { return _hViewFont; }
		inline DWORD getFindFLags() const { return _findFlags; }

		inline HBRUSH getBkgndBrush( bool warn = false ) const { return warn ? _hBrushBkgndWarn : _hBrushBkgnd; }
		inline HBRUSH getActiveBrush( bool warn = false ) const { return warn ? _hBrushActiveWarn : _hBrushActive; }
		inline HBRUSH getGreenOkBrush() const { return _hBrushGreenOk; }

		inline HIMAGELIST getSystemImageList() const { return _imgListSystem; }
		inline HIMAGELIST getRegedImageList() const { return _imgListReged; }
		inline ITaskbarList4 *getTaskbarList() { return _pTaskbarList; }
		inline CMainDrivesMenu& getDrivesMenu() { return _drivesMenu; }
		inline COverlaysLoader& getOverlaysLoader() { return _overlayLoader; }

		inline BYTE getActivePanelId() const { return _activePanelId; }
		inline void setActivePanelId( BYTE id ) { _activePanelId = id; }

		inline bool isInitialized() { return _isInitialized; }
		inline bool isActivated() { return _isActivated; }

		inline std::wstring& getCompareFile1() { return _compareFile1; }
		inline std::wstring& getCompareFile2() { return _compareFile2; }

	public:
		void updateLayout(); // update controls' layout
		void setAlwaysOnTop();
		void swapPanels();
		void onActivate( bool activated );
		void onSysChangeNotify( HANDLE hChange, DWORD dwProcId );
		void onNeigborsUpdate( int cmd, WCHAR *resourceName );
		void neighborsFinished( bool success );
		void onTimer( int timerId );
		UINT processContextMenu( CShellContextMenu& ctx, HWND hWnd, const std::vector<std::wstring>& items, const POINT& pt, bool isFile );

	public:
		// left and right panels instances
		CFcPanel& getPanelLeft() { return _isSwaped ? _panelRight : _panelLeft; }
		CFcPanel& getPanelRight() { return _isSwaped ? _panelLeft : _panelRight; }

		// get active/opposite panel
		CFcPanel& getActivePanel() { return ( _panelLeft.id() == _activePanelId ) ? _panelLeft : _panelRight; }
		CFcPanel& getOppositePanel() { return ( _panelLeft.id() != _activePanelId ) ? _panelLeft : _panelRight; }

	private:
		void initGdiObjects();
		void initSystemMenu();
		void initImageLists();
		void initOsVersion();
		void cleanUp();

	private:
		bool _loadNetNeighbors();

	private:
		CFcPanel _panelLeft;
		CFcPanel _panelRight;

		CMainToolbar _toolbarMain;
		CMainDrivesMenu _drivesMenu;
		COverlaysLoader _overlayLoader;

		CBackgroundWorker _workerNeighbors;
		std::vector<std::wstring> _neighbors;
		std::vector<std::wstring> _neighborsTemp;

		bool _neighborsFinished;

		bool _isInitialized;  // all controls were initialized
		bool _isActivated;    // application is active
		bool _isSwaped;       // are panels swaped
		BYTE _activePanelId;  // id of active panel
		DWORD _findFlags;     // flags for FindFirstFileEx

		OSVERSIONINFOEX	_ovi;

		HMENU _hMenuBar;
		HFONT _hGuiFont;
		HFONT _hViewFont;

		HBRUSH _hBrushBkgnd;
		HBRUSH _hBrushBkgndWarn;
		HBRUSH _hBrushActive;
		HBRUSH _hBrushActiveWarn;
		HBRUSH _hBrushGreenOk;

		HIMAGELIST _imgListSystem;
		HIMAGELIST _imgListReged;

		ITaskbarList4 *_pTaskbarList;

		std::wstring _compareFile1;
		std::wstring _compareFile2;

		std::map<HWND, CControl*> _controlsList;
		std::list<CBaseDialog*> _dialogs;
	};
}
