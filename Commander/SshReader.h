#pragma once

/*
	Ssh reader interface for reading ssh servers
*/

#include "BaseReader.h"

#include "PuttyInterface.h"

namespace Commander
{
	class CSshReader : public CBaseReader
	{
	private:
		enum class EOsTypes { Default, Linux, Macos, FreeBsd, NetBsd, OpenBsd, Solaris };

	public:
		struct SshData {
			bcb::TSessionData session;
			bcb::TSecureShell shell;
			SshData() : shell( &session ) {}
		};

	public:
		CSshReader( ESortMode mode, HWND hwndNotify, std::shared_ptr<SshData> spData );
		~CSshReader();

		virtual bool isPathValid( const std::wstring& path ) override;
		virtual bool readEntries( std::wstring& path ) override;
		virtual bool copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool renameEntry( const std::wstring& path, const std::wstring& newName ) override;
		virtual bool deleteEntries( const std::vector<std::wstring>& entries ) override;
		virtual bool calculateSize( const std::vector<int>& entries ) override;

		virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		void onCaptureOutput( const std::wstring& str, bcb::TCaptureOutputType outputType );
		void parseEntries( const std::vector<std::wstring>& entries );
		void parseFreeSpace( const std::vector<std::wstring>& diskFree );
		EntryData parseEntry( const std::vector<std::wstring>& entry );
		EntryData parseLine( std::wstring line );
		EOsTypes getOsType( const std::wstring& str );

		bool _sshReadCore();
		bool _sshCalcSizeCore();

	private:
		std::shared_ptr<SshData> _spSshData;
		std::wstring _currentDirectory;
		std::wstring _lang;
		EOsTypes _osType;

		std::vector<int> _markedEntries;
		bool _contentLoaded;
		bool _isFirstRun;
	};
}
