#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CAboutBox : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_ABOUTBOX;

	public:
		virtual void onInit() override;
		virtual void onDestroy() override;

	private:
		HFONT _hFont;
	};
}
