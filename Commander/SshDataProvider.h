#pragma once

/*
	Ssh data provider interface for reading data from ssh servers
*/

#include "BaseDataProvider.h"

#include "PuttyInterface.h"

namespace Commander
{
	class CSshDataProvider : public CBaseDataProvider
	{
	private:
		//enum class EOsTypes { Default, Linux, Macos, OpenBsd, FreeBsd, Solaris };

	public:
		CSshDataProvider();
		~CSshDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

		//virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		void onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType );
		//EOsTypes getOsType( const std::wstring& str );

		bool _sshReadCore();

	private:
		std::unique_ptr<CSshReader::SshData> _upSshData;
		std::wstring _currentDirectory;

		bool _isFirstRun;
	};
}
