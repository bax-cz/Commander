#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CTextInput : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_TEXTINPUTBOX;

	public:
		//
		// Helper struct to pass params into dialog box
		//
		struct CParams {
			bool         _showInputBox;
			bool         _password;
			int          _dialogWidth;
			std::wstring _dialogTitle;
			std::wstring _dialogCaption;
			std::wstring _userText;
			HWND         _hParentWnd;

			inline void setParent( HWND hw ) { _hParentWnd = hw; }
			CParams( std::wstring title, std::wstring caption, std::wstring text, bool show = true, bool pass = false, int width = 261 )
			{
				_showInputBox = show; _password = pass; _dialogWidth = width;
				_dialogTitle = title; _dialogCaption = caption; _userText = text;
				_hParentWnd = NULL;
			}
		};

	public:
		virtual void onInit() override;
		virtual bool onOk() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
	};
}
