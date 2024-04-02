#pragma once

#include "BackgroundWorker.h"

/*
	Archive utils - RAR, ZIP, 7ZIP, TAR archives
*/

namespace Commander
{
	class CArchiver
	{
	public:
		enum EExtractAction { Overwrite, Skip, Rename };

	public:
		CArchiver();
		virtual ~CArchiver();

		void init( const std::wstring& fileName, std::vector<std::shared_ptr<EntryData>> *pFiles, std::vector<std::shared_ptr<EntryData>> *pDirs, CBackgroundWorker *pWorker, EExtractAction action = Overwrite );

		virtual bool readEntries() = 0;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) = 0;
		virtual bool packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetName ) { _errorMessage = L"Method not supported."; return false; }

		virtual void setPassword( const char *password ) {} // TODO
		virtual int getVersion() { return -1; } // TODO

	public:
		const std::wstring& getName() { return _fileName; }
		const std::wstring& getErrorMessage() { return _errorMessage; }
		const std::vector<EntryData>& getEntriesRaw() { return _dataEntries; }
		bool calculateDirectorySize( const std::wstring& dirName, ULONGLONG *dirSize );
		void readLocalPath( const std::wstring& srcDir );
		void addParentDirEntry( const WIN32_FIND_DATA& wfd );

	private:
		void addEntryFile( EntryData& entry, const std::wstring& fileName );
		void addEntryDirectory( EntryData& entry, const std::wstring& dirName, bool attrValid = true );

	protected:
		bool shouldExtractEntry( const std::vector<std::wstring>& entries, std::wstring& entryName );

	protected:
		std::wstring _fileName;
		std::wstring _localDirectory;
		std::wstring _errorMessage;

		CBackgroundWorker *_pWorker;
		EExtractAction _action;

		std::vector<std::shared_ptr<EntryData>> *_pFiles;
		std::vector<std::shared_ptr<EntryData>> *_pDirs;

		std::vector<EntryData> _dataEntries;
	};
}
