#pragma once

/*
	Ftp data provider interface for reading data from ftp servers
*/

#include "BaseDataProvider.h"

#include "../FtpClient/FTPClient.h"

namespace Commander
{
	class CFtpDataProvider : public CBaseDataProvider, nsFTP::CFTPClient::CNotification
	{
	public:
		CFtpDataProvider();
		~CFtpDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

		//virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		//virtual void OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr ) override
		//{
		//	_errorMessage = errorMsg;
		//}

	private:
		bool _ftpReadCore();

	private:
		std::unique_ptr<CFtpReader::FtpData> _upFtpData;

		std::wstring _currentDirectory;
	};
}
