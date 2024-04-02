#pragma once

/*
	CArchZip interface class to Minizip (zlib)
	---------------------------------------
	Provides interface for ZIP archive de/compressing
*/

#include "Archiver.h"
#include "../Zlib/FcZlib.h"

namespace Commander
{
	class CArchZip : public CArchiver
	{
	public:
		CArchZip();
		~CArchZip();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;
		virtual bool packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetName ) override;

		virtual void setPassword( const char *password ) override;
		virtual int getVersion() override;

	private:
		void getEntryInfo( const unz_file_info64& fileInfo, const char *fileName, WIN32_FIND_DATA& wfd );
		bool packFile( zipFile hZip, const std::wstring& fileName, const WIN32_FIND_DATA& wfd );
		bool packDirectory( zipFile hZip, const std::wstring& dirName );
		int extractFile( const std::wstring& path );
		std::wstring getErrorMessage( int errorCode );

	private:
		unzFile _hArchive;

		unz_global_info64 _globalInfo;
		unz_file_info64 _fileInfo;

		size_t _localPathIdx;

		unsigned char _dataBuf[0x00400000];
		wchar_t _passwd[0x400];
	};
}
