#pragma once

#include "BaseDialog.h"

#define SSH_COMMAND_LENGTH_MAX   1024

namespace Commander
{
	class CSshOutput : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_SECURESHELL;

	public:
		CSshOutput();
		~CSshOutput();

		virtual void onInit() override;
		virtual bool onOk() override;
		virtual bool onClose() override;
		virtual void onDestroy() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;
		LRESULT CALLBACK wndProcEdit( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	public:
		void setParent( std::shared_ptr<CPanelTab> spPanel = nullptr );

	private:
		void readStartupMessage();
		void onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType );
		void updateWindowTitle( const std::wstring& command = L"" );
		void updateSecureShellWindow();
		void updateSendButtonStatus();
		void updateCommandsList();
		void summonCommandFromList( bool prev = true );

	private:
		bool _workerProc();

	private:
		std::unique_ptr<CSshReader::SshData> _upSshData;
		CBackgroundWorker _worker;

		std::shared_ptr<CPanelTab> _spPanel;

		WCHAR _buf[SSH_COMMAND_LENGTH_MAX + 1];

		std::vector<std::wstring> _commandList;
		int _commandListPos;

		std::wstring _windowTitle;
		std::wstring _currentCommand;
		std::wstring _currentDirectory;
		std::wstring _secureShellText;
		std::wstring _errorMessage;

	private:
		static LRESULT CALLBACK wndProcEditSubclass( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR data )
		{
			return reinterpret_cast<CSshOutput*>( data )->wndProcEdit( hWnd, message, wParam, lParam );
		}
	};
}
