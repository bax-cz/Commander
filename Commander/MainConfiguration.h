#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CMainConfiguration : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_CONFIGURATION;

	private:
		const wchar_t *_browserSubKeyName = L"SOFTWARE\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION";

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		void setLatestIE();

	private:
		HWND _hToolTip;
	};
}
