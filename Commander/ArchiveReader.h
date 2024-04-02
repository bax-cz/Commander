#pragma once

/*
	Archive reader interface for reading/extracting various archive formats
*/

#include "BaseReader.h"

#include "ArchiveTypes.h"

namespace Commander
{
	class CArchiveReader : public CBaseReader
	{
	public:
		CArchiveReader( ESortMode mode, HWND hwndNotify );
		~CArchiveReader();

		virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;
		virtual bool isPathValid( const std::wstring& path ) override;
		virtual bool readEntries( std::wstring& path ) override;
		virtual bool copyEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool moveEntries( const std::vector<std::wstring>& entries, const std::wstring& path ) override;
		virtual bool renameEntry( const std::wstring& path, const std::wstring& newName ) override;
		virtual bool deleteEntries( const std::vector<std::wstring>& entries ) override;
		virtual bool calculateSize( const std::vector<int>& entries ) override;

	private:
		bool createArchiver( const std::wstring& fileName );
		bool _openArchiveCore();

	private:
		std::unique_ptr<CArchiver> _upArchiver; // archiver (unzip, unrar, gzip..)
		WIN32_FIND_DATA _wfdArchFile;
	};
}
