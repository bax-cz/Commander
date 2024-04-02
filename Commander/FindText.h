#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CFindText : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_FINDTEXT;

	public:
		//
		// Helper struct to pass params into dialog box
		//
		struct CParams {
			bool matchCase;
			bool wholeWords;
			bool regexMode;
			bool hexMode;
			bool reverse;
		};

	public:
		CFindText();
		~CFindText();

	public:
		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
		LRESULT CALLBACK wndProcControls( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	public:
		void updateText( const std::wstring& text, const std::wstring& replaceText = L"" );
		void updateParams( const CParams& params, bool replace = false );

		inline CParams& getParams() { return _params; }
		inline std::wstring getReplaceText() { return _replaceText; }

	public:
		void showMessageBox( const std::wstring& text );
		void enableHexMode( bool enable );
		void messageBoxClosed() { _messageBoxShown = false; }

	private:
		void readParams();
		void onReplace( bool all = false );
		void switchFindReplaceTabs();
		void tabSelectionChanged();

	private:
		CParams _params;
		std::wstring _replaceText;
		std::wstring _message;

		bool _messageBoxShown;
		bool _replaceMode;
		bool _fireEvent;

	private:
		static LRESULT CALLBACK wndProcControlsSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CFindText*>( data )->wndProcControls( hWnd, message, wParam, lParam );
		}
	};
}
