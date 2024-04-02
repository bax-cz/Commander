#pragma once

/*
	Ftp reader interface for reading data from ftp servers
*/

#include "BaseReader.h"

#include "../FtpClient/FTPClient.h"

namespace Commander
{
	class CFtpReader : public CBaseReader, nsFTP::CFTPClient::CNotification
	{
	public:
		struct SessionData {
			bool passive;
			std::wstring initialPath;
			nsFTP::CLogonInfo logonInfo;
		};

		struct FtpData {
			SessionData session;
			nsFTP::CFTPClient ftpClient;
		};

	public:
		static nsFTP::TFTPFileStatusShPtr findEntry( const nsFTP::TFTPFileStatusShPtrVec& entries, const tstring& name );
		static bool isEntryDirectory( const nsFTP::TFTPFileStatusShPtr entry );

	public:
		CFtpReader( ESortMode mode, HWND hwndNotify, std::shared_ptr<FtpData> spData );
		~CFtpReader();

		virtual bool isPathValid( const std::wstring& path ) override;
		virtual bool readEntries( std::wstring& path ) override;
		virtual bool copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool renameEntry( const std::wstring& path, const std::wstring& newName ) override;
		virtual bool deleteEntries( const std::vector<std::wstring>& entries ) override;
		virtual bool calculateSize( const std::vector<int>& entries ) override;

		virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		virtual void OnSendCommand( const nsFTP::CCommand& command, const nsFTP::CArg& arguments ) override;
		virtual void OnResponse( const nsFTP::CReply& reply ) override;
		virtual void OnInternalError( const tstring& errorMsg, const tstring& fileName, DWORD lineNr ) override;

	private:
		void calculateDirectorySize( const std::wstring& dirName, ULONGLONG& fileSizeResult );
		void addParentDirEntry();

		bool _ftpReadCore();
		bool _ftpKeepAlive();
		bool _ftpCalcSizeCore();

	private:
		std::shared_ptr<FtpData> _spFtpData;
		std::unique_ptr<std::pair<nsFTP::CCommand, nsFTP::CReply>> _upCommand;

		std::wstring _currentDirectory;
		std::vector<int> _markedEntries;

		bool _contentLoaded;
	};
}
