#include "stdafx.h"

#include "Commander.h"
#include "AboutBox.h"

#include "ShellUtils.h"
#include "StringUtils.h"
#include "MiscUtils.h"

#include "version.h"

namespace Commander
{
	//
	// About box initialization
	//
	void CAboutBox::onInit()
	{
		std::wstring productName, productVer;
		SysUtils::getProductAndVersion( productName, productVer );

		_hFont = CreateFont(
			19, 0, 0, 0,
			FW_ULTRABOLD,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			TEXT( "Segoe UI" ) );

		SendDlgItemMessage( _hDlg, IDC_PRODUCTNAME, WM_SETFONT, (WPARAM)_hFont, TRUE );

		WCHAR sCopyright[MAX_WPATH];
		LoadString( FCS::inst().getFcInstance(), IDS_COMMANDER_COPYRIGHT, sCopyright, MAX_WPATH );
		SetDlgItemText( _hDlg, IDE_COPYRIGHT3RDPARTY, sCopyright );

		auto text = productName + L" " + productVer.substr( 0, productVer.find_first_of( L'.' ) + 2 );
		text += L" (" + SysUtils::getPlatformName() + L")";
		SetDlgItemText( _hDlg, IDC_PRODUCTNAME, text.c_str() );

		text = L"Version: " + productVer + L", Compiled: " + std::wstring( _BUILD_TIME );
		SetDlgItemText( _hDlg, IDC_BUILDNUMBER, text.c_str() );

		auto title = std::wstring( L"About " ) + productName;
		SetWindowText( _hDlg, title.c_str() );
	}

	void CAboutBox::onDestroy()
	{
		DeleteObject( _hFont );
	}
}
