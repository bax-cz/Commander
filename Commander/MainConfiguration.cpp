#include "stdafx.h"

#include "Commander.h"
#include "MainConfiguration.h"
#include "MiscUtils.h"

namespace Commander
{
	void setLinkVisited( HWND hLink )
	{
		SetWindowText( hLink, L"<a>Using Latest IE</a>" );

		LITEM item = { 0 };

		item.mask = LIF_ITEMINDEX | LIF_STATE | LIF_ITEMID;
		item.stateMask = LIS_FOCUSED | LIS_ENABLED | LIS_VISITED;
		SendMessage( hLink, LM_GETITEM, 0, (LPARAM)&item );

		item.state = LIS_VISITED;
		SendMessage( hLink, LM_SETITEM, 0, (LPARAM)&item );
	}

	HWND createToolTip( HWND hDlg, UINT ctrlId, PTSTR pszText )
	{
		if( !ctrlId || !hDlg || !pszText )
			return FALSE;

		// get the window of the tool
		HWND hwndTool = GetDlgItem( hDlg, ctrlId );

		// create the tooltip
		HWND hwndTip = CreateWindowEx( NULL, TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_ALWAYSTIP /*| TTS_BALLOON*/,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			hDlg, NULL,
			FCS::inst().getFcInstance(), NULL );

		if( !hwndTool || !hwndTip )
		{
			return (HWND)NULL;
		}

		// associate the tooltip with the tool
		TOOLINFO toolInfo = { 0 };
		toolInfo.cbSize = sizeof( toolInfo );
		toolInfo.hwnd = hDlg;
		toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		toolInfo.uId = (UINT_PTR)hwndTool;
		toolInfo.lpszText = pszText;
		SendMessage( hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo );

		return hwndTip;
	}

	void CMainConfiguration::onInit()
	{
		HKEY hKey = nullptr;

		_hToolTip = createToolTip( _hDlg, IDC_IESYSLINK, L"Adds application name into the FEATURE_BROWSER_EMULATION registry key." );

		/*if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Internet Explorer", 0, KEY_READ, &hKey ) == ERROR_SUCCESS )
		{
			DWORD valueType, valueLen = 0;
			RegQueryValueEx( hKey, L"Version", NULL, &valueType, NULL, &valueLen );

			if( valueType == REG_SZ )
			{
				std::wstring ieVersion; ieVersion.resize( valueLen / sizeof( WCHAR ) );
				RegQueryValueEx( hKey, L"Version", NULL, &valueType, (BYTE*)&ieVersion[0], &valueLen );
			}

			RegCloseKey( hKey );
		}*/

		if( RegOpenKeyEx( HKEY_CURRENT_USER, _browserSubKeyName, 0, KEY_READ, &hKey ) == ERROR_SUCCESS )
		{
			std::wstring product, version;
			SysUtils::getProductAndVersion( product, version );
			StringUtils::replaceAll( product, L" ", L"" );

			DWORD type = REG_DWORD, data = 0, dataSize = sizeof( DWORD );
			if( RegQueryValueEx( hKey, ( product + L".exe" ).c_str(), NULL, &type, (LPBYTE)&data, &dataSize ) == ERROR_SUCCESS )
			{
				setLinkVisited( GetDlgItem( _hDlg, IDC_IESYSLINK ) );
			}

			RegCloseKey( hKey );
		}
	}

	bool CMainConfiguration::onOk()
	{
		PrintDebug( "ok" );
		return true;
	}

	bool CMainConfiguration::onClose()
	{
		PrintDebug( "closing" );
		return true;
	}

	void CMainConfiguration::onDestroy()
	{
		DestroyWindow( _hToolTip );
	}

	void CMainConfiguration::setLatestIE()
	{
		HKEY hKey = nullptr;
		DWORD disp = 0;
		LSTATUS res = 0;

		if( ( res = RegCreateKeyEx( HKEY_CURRENT_USER, _browserSubKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &disp ) ) == ERROR_SUCCESS )
		{
			std::wstring product, version;
			SysUtils::getProductAndVersion( product, version );
			StringUtils::replaceAll( product, L" ", L"" );

			DWORD value = 0x2AF8; // IE11
			if( RegSetKeyValue( hKey, NULL, ( product + L".exe" ).c_str(), REG_DWORD, (LPCWSTR)&value, sizeof( DWORD ) ) == ERROR_SUCCESS)
			{
				setLinkVisited( GetDlgItem( _hDlg, IDC_IESYSLINK ) );
			}

			RegCloseKey( hKey );
		}
		else
		{
			MessageBox( _hDlg, SysUtils::getErrorMessage( res ).c_str(), L"Error Reading Key", MB_ICONERROR | MB_OK );
		}
	}

	INT_PTR CALLBACK CMainConfiguration::dialogProc( UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_NOTIFY:
			switch( ( (LPNMHDR)lParam )->code )
			{
			case NM_CLICK: // Fall through
			case NM_RETURN:
				setLatestIE();
				break;
			}
			break;

		case WM_COMMAND:
			// TODO
			break;
		}

		return (INT_PTR)false;
	}
}
