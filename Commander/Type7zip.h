#pragma once

#include "Archiver.h"

#include "7zip/Common/FileStreams.h"

#include "7zip/Archive/IArchive.h"
#include "7zip/Archive/7z/7zHandler.h"

namespace Commander
{
	class CArch7zip : public CArchiver
	{
		friend class CArchiveOpenCallback;
		friend class CArchiveExtractCallback;
		friend class CArchiveUpdateCallback;

	private:
		struct EntryData
		{
			UInt64 Size;
			FILETIME CTime;
			FILETIME ATime;
			FILETIME MTime;
			UString Name;
			FString FullPath;
			UInt32 Attrib;

			bool isDir() const { return ( Attrib & FILE_ATTRIBUTE_DIRECTORY ) != 0; }
		};

	public:
		CArch7zip();
		~CArch7zip();

		virtual bool readEntries() override;
		virtual bool extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir ) override;
		virtual bool packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetName ) override;

	private:
		void prepareIndicesFromEntries( IInArchive *archive, const std::vector<std::wstring>& entries );
		void prepareEntriesInfo( const std::vector<std::wstring>& entries );
		void addFileEntry( const std::wstring& fileName, WIN32_FIND_DATA& wfd );
		void addDirEntry( const std::wstring& dirName );
		void getEntryInfo( IInArchive *archive, int fileIdx, WIN32_FIND_DATA *wfd );
		std::wstring getErrorMessage( int errorCode );

	private:
		CObjectVector<CArch7zip::EntryData> _entriesData;
		std::vector<std::wstring> _entryNames;
		std::vector<UInt32> _entryIndices;
		size_t _localPathIdx;

		UString Password;
		bool PasswordIsDefined;
	};

	//
	// Archive Open callback class
	//
	class CArchiveOpenCallback Z7_final: 
		public IArchiveOpenCallback,
		public ICryptoGetTextPassword,
		public CMyUnknownImp
	{
		Z7_IFACES_IMP_UNK_2( IArchiveOpenCallback, ICryptoGetTextPassword )

	public:
		bool PasswordIsDefined;
		UString Password;

		CArchiveOpenCallback() : _pArchiver( nullptr ), PasswordIsDefined( false ) {}
		inline void Init( CArch7zip *pArchiver ) { _pArchiver = pArchiver; }

	private:
		CArch7zip *_pArchiver;
	};

	//
	// Archive Extracting callback class
	//
	class CArchiveExtractCallback Z7_final: 
		public IArchiveExtractCallback,
		public ICryptoGetTextPassword,
		public CMyUnknownImp
	{
		Z7_IFACES_IMP_UNK_2( IArchiveExtractCallback, ICryptoGetTextPassword ) 
		Z7_IFACE_COM7_IMP( IProgress )

	private:
		CArch7zip *_pArchiver;
		CMyComPtr<IInArchive> _archiveHandler;
		FString _directoryPath;  // Output directory
		UString _filePath;       // name inside arcvhive
		FString _diskFilePath;   // full path to file on disk
		bool _extractMode;
		struct CProcessedFileInfo
		{
			FILETIME MTime;
			UInt32 Attrib;
			bool isDir;
			bool AttribDefined;
			bool MTimeDefined;
		} _processedFileInfo;

		COutFileStream *_outFileStreamSpec;
		CMyComPtr<ISequentialOutStream> _outFileStream;

	public:
		void Init( CArch7zip *pArchiver, IInArchive *archiveHandler, const FString &directoryPath );

		UInt64 NumErrors;
		bool PasswordIsDefined;
		UString Password;

		CArchiveExtractCallback()
			: _pArchiver( nullptr )
			, _extractMode( false )
			, PasswordIsDefined( false )
			, NumErrors( 0ull )
		{}
	};

	//
	// Archive Creating callback class
	//
	class CArchiveUpdateCallback Z7_final: 
		public IArchiveUpdateCallback2,
		public ICryptoGetTextPassword2,
		public CMyUnknownImp
	{
		Z7_IFACES_IMP_UNK_2( IArchiveUpdateCallback2, ICryptoGetTextPassword2 ) 
		Z7_IFACE_COM7_IMP( IProgress ) 
		Z7_IFACE_COM7_IMP( IArchiveUpdateCallback ) 

	public:
		CRecordVector<UInt64> VolumesSizes;
		UString VolName;
		UString VolExt;

		FString DirPrefix;
		const CObjectVector<CArch7zip::EntryData> *DirItems;

		bool PasswordIsDefined;
		UString Password;
		bool AskPassword;

		bool _NeedBeClosed;

		FStringVector FailedFiles;
		CRecordVector<HRESULT> FailedCodes;

		CArchiveUpdateCallback() :
			_pArchiver( nullptr ),
			DirItems( nullptr ),
			PasswordIsDefined( false ),
			AskPassword( false )
		{}

		~CArchiveUpdateCallback() { Finalize(); }
		HRESULT Finalize();

		inline void Init( CArch7zip *pArchiver, const CObjectVector<CArch7zip::EntryData> *entries )
		{
			_pArchiver = pArchiver;
			DirItems = entries;
			_NeedBeClosed = false;
			FailedFiles.Clear();
			FailedCodes.Clear();
		}

	private:
		CArch7zip *_pArchiver;
	};
}
