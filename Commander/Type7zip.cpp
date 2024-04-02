#include "stdafx.h"

#include "Commander.h"
#include "Type7zip.h"

#include "MiscUtils.h"

#include "Common/MyWindows.h"
#include "Common/MyInitGuid.h"
#include "Common/Defs.h"
#include "Common/IntToString.h"
#include "Common/StringConvert.h"

#ifdef IsDir
#undef IsDir
#endif

#include "Windows/FileDir.h"
#include "Windows/FileFind.h"
#include "Windows/FileName.h"
#include "Windows/NtCheck.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConv.h"

//#include "7zip/IPassword.h"
//#include "7zVersion.h"

namespace Commander
{
	using namespace NWindows;
	using namespace NFile;
	using namespace NDir;

	//
	// Constructor/destructor
	//
	CArch7zip::CArch7zip() : _localPathIdx( 0ull ), PasswordIsDefined( false )
	{
	}

	CArch7zip::~CArch7zip()
	{
	}

	// You can find the list of all GUIDs in Guid.txt file.
	// use another CLSIDs, if you want to support other formats (zip, rar, ...).
	// {23170F69-40C1-278A-1000-000110xx0000}

	DEFINE_GUID( CLSID_CFormat7z,  0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x07, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatBz2, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x02, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatArj, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x04, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatZ,   0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x05, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatLzh, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x06, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatCab, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x08, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatNsis,0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x09, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatLzma,0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x0A, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatXz,  0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x0C, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatPpmd,0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x0D, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatXar, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xE1, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatDmg, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xE4, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatWim, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xE6, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatIso, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xE7, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatRpm, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xEB, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatDeb, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xEC, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatTar, 0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xEE, 0x00, 0x00 );
	DEFINE_GUID( CLSID_CFormatGz,  0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0xEF, 0x00, 0x00 );

	// Forward declaration - defined in DllExports2.cpp
	STDAPI CreateObject( const GUID *clsid, const GUID *iid, void **outObject );

	const GUID *getClsidFromFileName( const std::wstring& fileName )
	{
		switch( ArchiveType::getType( fileName ) )
		{
		case ArchiveType::EArchType::FMT_7ZIP:
			return &CLSID_CFormat7z;
		case ArchiveType::EArchType::FMT_ARJ:
			return &CLSID_CFormatArj;
		case ArchiveType::EArchType::FMT_Z:
			return &CLSID_CFormatZ;
		case ArchiveType::EArchType::FMT_LZH:
			return &CLSID_CFormatLzh;
		case ArchiveType::EArchType::FMT_ISO:
			return &CLSID_CFormatIso;
		case ArchiveType::EArchType::FMT_WIM:
			return &CLSID_CFormatWim;
		case ArchiveType::EArchType::FMT_CAB:
			return &CLSID_CFormatCab;
		case ArchiveType::EArchType::FMT_DMG:
			return &CLSID_CFormatDmg;
		case ArchiveType::EArchType::FMT_XAR:
			return &CLSID_CFormatXar;
		case ArchiveType::EArchType::FMT_RPM:
			return &CLSID_CFormatRpm;
		case ArchiveType::EArchType::FMT_DEB:
			return &CLSID_CFormatDeb;
		case ArchiveType::EArchType::FMT_NSIS:
			return &CLSID_CFormatNsis;
		case ArchiveType::EArchType::FMT_LZMA:
			return &CLSID_CFormatLzma;
		case ArchiveType::EArchType::FMT_PPMD:
			return &CLSID_CFormatPpmd;
		default:
			return nullptr;
		}
	}

	static HRESULT IsArchiveItemProp( IInArchive *archive, UInt32 index, PROPID propID, bool &result )
	{
		NCOM::CPropVariant prop;
		RINOK( archive->GetProperty( index, propID, &prop ) )

		if (prop.vt == VT_BOOL)
			result = VARIANT_BOOLToBool( prop.boolVal );
		else if( prop.vt == VT_EMPTY )
			result = false;
		else
			return E_FAIL;

		return S_OK;
	}

	static HRESULT IsArchiveItemFolder( IInArchive *archive, UInt32 index, bool &result )
	{
		return IsArchiveItemProp( archive, index, kpidIsDir, result );
	}

	//
	// Archive Open callback class methods
	//
	Z7_COM7F_IMF( CArchiveOpenCallback::SetTotal( const UInt64 * /* files */, const UInt64 * /* bytes */ ) )
	{
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveOpenCallback::SetCompleted( const UInt64 * /* files */, const UInt64 * /* bytes */ ) )
	{
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveOpenCallback::CryptoGetTextPassword( BSTR *password ) )
	{
		if( !PasswordIsDefined )
		{
			// You can ask real password here from user
			//Password = GetPassword(OutStream);
			std::wstring pass( MAXPASSWORD, L'\0' ); // TODO: limited to 512 wchars - RAR specific

			_pArchiver->_pWorker->sendMessage( FC_ARCHNEEDPASSWORD, reinterpret_cast<LPARAM>( &pass[0] ) );
			pass.resize( wcslen( &pass[0] ) );

			if( !pass.empty() )
			{
				Password.SetFrom( pass.c_str(), static_cast<unsigned int>( pass.length() ) );
				_pArchiver->PasswordIsDefined = PasswordIsDefined = true;
				_pArchiver->Password = Password;
			}
			else
			{
				PrintDebug("Password is not defined");
				return E_ABORT;
			}
		}

		return StringToBstr( Password, password );
	}

	//
	// Archive Extracting callback class methods
	//
	void CArchiveExtractCallback::Init( CArch7zip *pArchiver, IInArchive *archiveHandler, const FString &directoryPath )
	{
		NumErrors = 0;
		_pArchiver = pArchiver;
		_archiveHandler = archiveHandler;
		_directoryPath = directoryPath;
		NName::NormalizeDirPathPrefix( _directoryPath );
	}

	Z7_COM7F_IMF( CArchiveExtractCallback::SetTotal( UInt64 sizeTotal ) )
	{
		_pArchiver->_pWorker->sendNotify( FC_ARCHBYTESTOTAL, sizeTotal );
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveExtractCallback::SetCompleted( const UInt64 *completeValue ) )
	{
		_pArchiver->_pWorker->sendNotify( FC_ARCHBYTESABSOLUTE, *completeValue );
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveExtractCallback::GetStream( UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode ) )
	{
		*outStream = 0;
		_outFileStream.Release();

		if( !_pArchiver->_pWorker->isRunning() )
			return E_ABORT;

		/*{
			// Get Name
			NCOM::CPropVariant prop;
			RINOK( _archiveHandler->GetProperty( index, kpidPath, &prop ) )

			UString fullPath;
			if( prop.vt == VT_EMPTY )
				fullPath = L"[Content]";
			else
			{
				if( prop.vt != VT_BSTR )
					return E_FAIL;

				fullPath = prop.bstrVal;
			}
			_filePath = fullPath;
		}*/

		// Get Filename
		auto it = std::find( _pArchiver->_entryIndices.begin(), _pArchiver->_entryIndices.end(), index );

		if( it != _pArchiver->_entryIndices.end() )
			_filePath = _pArchiver->_entryNames[it - _pArchiver->_entryIndices.begin()].c_str();
		else
			_filePath = L"[Content]";

		if( askExtractMode != NArchive::NExtract::NAskMode::kExtract )
			return S_OK;

		{
			// Get Attrib
			NCOM::CPropVariant prop;
			RINOK( _archiveHandler->GetProperty( index, kpidAttrib, &prop ) )
			if( prop.vt == VT_EMPTY )
			{
				_processedFileInfo.Attrib = 0;
				_processedFileInfo.AttribDefined = false;
			}
			else
			{
				if( prop.vt != VT_UI4 )
					return E_FAIL;

				_processedFileInfo.Attrib = prop.ulVal;
				_processedFileInfo.AttribDefined = true;
			}
		}

		RINOK( IsArchiveItemFolder( _archiveHandler, index, _processedFileInfo.isDir ) )

		{
			// Get Modified Time
			NCOM::CPropVariant prop;
			RINOK( _archiveHandler->GetProperty( index, kpidMTime, &prop ) )
			_processedFileInfo.MTimeDefined = false;
			switch( prop.vt )
			{
			case VT_EMPTY:
				// _processedFileInfo.MTime = _utcMTimeDefault;
				break;
			case VT_FILETIME:
				_processedFileInfo.MTime = prop.filetime;
				_processedFileInfo.MTimeDefined = true;
				break;
			default:
				return E_FAIL;
			}

		}
		{
			// Get Size
			NCOM::CPropVariant prop;
			RINOK( _archiveHandler->GetProperty( index, kpidSize, &prop ) )
			UInt64 newFileSize;
			/* bool newFileSizeDefined = */ ConvertPropVariantToUInt64( prop, newFileSize );
		}


		{
			// Create folders for file
			int slashPos = _filePath.ReverseFind_PathSepar();
			if (slashPos >= 0)
				CreateComplexDir(_directoryPath + us2fs(_filePath.Left(slashPos)));
		}

		FString fullProcessedPath = _directoryPath + us2fs(_filePath);
		_diskFilePath = fullProcessedPath;

		if (_processedFileInfo.isDir)
		{
			CreateComplexDir(fullProcessedPath);
		}
		else
		{
			NFind::CFileInfo fi;
			if (fi.Find(fullProcessedPath))
			{
				if (!DeleteFileAlways(fullProcessedPath))
				{
				//	PrintError("Cannot delete output file", fullProcessedPath);
					return E_ABORT;
				}
			}

			_outFileStreamSpec = new COutFileStream;
			CMyComPtr<ISequentialOutStream> outStreamLoc(_outFileStreamSpec);
			if (!_outFileStreamSpec->Open(fullProcessedPath, CREATE_ALWAYS))
			{
			//	PrintError("Cannot open output file", fullProcessedPath);
				return E_ABORT;
			}
			_outFileStream = outStreamLoc;
			*outStream = outStreamLoc.Detach();
		}
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveExtractCallback::PrepareOperation( Int32 askExtractMode ) )
	{
		_extractMode = false;
		switch( askExtractMode )
		{
		case NArchive::NExtract::NAskMode::kExtract:
			_extractMode = true;
			break;
		};

		switch( askExtractMode )
		{
		case NArchive::NExtract::NAskMode::kExtract:
			//Print(kExtractingString);
			break;
		case NArchive::NExtract::NAskMode::kTest:
			//Print(kTestingString);
			break;
		case NArchive::NExtract::NAskMode::kSkip:
			//Print(kSkippingString);
			break;
		};

		_pArchiver->_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)_filePath.GetBuf() );

		PrintDebug( "%ls", _filePath.GetBuf() );

		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveExtractCallback::SetOperationResult( Int32 operationResult ) )
	{
		switch( operationResult )
		{
		case NArchive::NExtract::NOperationResult::kOK:
			break;
		default:
		{
			NumErrors++;
			std::wstring s;
			switch( operationResult )
			{
			case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
				s = L"Unsupported Method.";
				break;
			case NArchive::NExtract::NOperationResult::kCRCError:
				s = L"CRC Failed.";
				break;
			case NArchive::NExtract::NOperationResult::kDataError:
				s = L"Data Error.";
				break;
			case NArchive::NExtract::NOperationResult::kUnavailable:
				s = L"Unavailable data.";
				break;
			case NArchive::NExtract::NOperationResult::kUnexpectedEnd:
				s = L"Unexpected end of data.";
				break;
			case NArchive::NExtract::NOperationResult::kDataAfterEnd:
				s = L"There are some data after the end of the payload data.";
				break;
			case NArchive::NExtract::NOperationResult::kIsNotArc:
				s = L"Is not archive.";
				break;
			case NArchive::NExtract::NOperationResult::kHeadersError:
				s = L"Headers Error.";
				break;
			default:
				s = _pArchiver->getErrorMessage( operationResult );
				break;
			}

			_pArchiver->_errorMessage = L"CArch7zip: " + s;

			if( !s.empty() )
				PrintDebug("Error: %ls", s.c_str());
			else
				PrintDebug("Error: #%d", operationResult);
		}
		}

		if( _outFileStream )
		{
			if( _processedFileInfo.MTimeDefined )
				_outFileStreamSpec->SetMTime( &_processedFileInfo.MTime );

			RINOK( _outFileStreamSpec->Close() )
		}

		_outFileStream.Release();

		if( _extractMode && _processedFileInfo.AttribDefined )
			SetFileAttrib_PosixHighDetect( _diskFilePath, _processedFileInfo.Attrib );

		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveExtractCallback::CryptoGetTextPassword( BSTR *password ) )
	{
		if( !PasswordIsDefined )
		{
			// You can ask real password here from user
			// Password = GetPassword(OutStream);
			// PasswordIsDefined = true;
		//	PrintError("Password is not defined");
			return E_ABORT;
		}
		return StringToBstr( Password, password );
	}

	//
	// Archive Creating callback class methods
	//
	Z7_COM7F_IMF( CArchiveUpdateCallback::SetTotal( UInt64 sizeTotal ) )
	{
		_pArchiver->_pWorker->sendNotify( FC_ARCHBYTESTOTAL, sizeTotal );
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::SetCompleted( const UInt64 *completeValue ) )
	{
		_pArchiver->_pWorker->sendNotify( FC_ARCHBYTESABSOLUTE, *completeValue );
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::GetUpdateItemInfo( UInt32 /* index */, Int32 *newData, Int32 *newProperties, UInt32 *indexInArchive ) )
	{
		if( newData )
			*newData = BoolToInt( true );
		if( newProperties )
			*newProperties = BoolToInt( true );
		if( indexInArchive )
			*indexInArchive = (UInt32)(Int32)-1;
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::GetProperty( UInt32 index, PROPID propID, PROPVARIANT *value ) )
	{
		NCOM::CPropVariant prop;

		if( propID == kpidIsAnti )
		{
			prop = false;
			prop.Detach( value );
			return S_OK;
		}

		{
			const CArch7zip::EntryData &dirItem = (*DirItems)[index];
			switch( propID )
			{
			case kpidPath:
				prop = dirItem.Name;
				break;
			case kpidIsDir:
				prop = dirItem.isDir();
				break;
			case kpidSize:
				prop = dirItem.Size;
				break;
			case kpidAttrib:
				prop = dirItem.Attrib;
				break;
			case kpidCTime:
				prop = dirItem.CTime;
				break;
			case kpidATime:
				prop = dirItem.ATime;
				break;
			case kpidMTime:
				prop = dirItem.MTime;
				break;
			}
		}
		prop.Detach( value );
		return S_OK;
	}

	HRESULT CArchiveUpdateCallback::Finalize()
	{
		if( _NeedBeClosed )
		{
		//	PrintNewLine();
			_NeedBeClosed = false;
		}
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::GetStream( UInt32 index, ISequentialInStream **inStream ) )
	{
		RINOK( Finalize() )

		if( !_pArchiver->_pWorker->isRunning() )
			return E_ABORT;

		const CArch7zip::EntryData &dirItem = (*DirItems)[index];

		//if( dirItem.Name.IsEmpty() )
		//	dirItem.Name.SetFrom( L"[Content]", 10 );

		if( dirItem.isDir() )
			return S_OK;

		{
			CInFileStream *inStreamSpec = new CInFileStream;
			CMyComPtr<ISequentialInStream> inStreamLoc( inStreamSpec );
			FString path = DirPrefix + dirItem.FullPath;
			if( !inStreamSpec->Open( path ) )
			{
				DWORD errorId = GetLastError();
				FailedCodes.Add( errorId );
				FailedFiles.Add( path );
				// if (systemError == ERROR_SHARING_VIOLATION)
				{
				//	PrintNewLine();
				//	PrintError("WARNING: can't open file");
					// Print(NError::MyFormatMessageW(systemError));
					return S_FALSE;
				}
				// return errorId;
			}

			_pArchiver->_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)path.GetBuf() );

			*inStream = inStreamLoc.Detach();
		}
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::SetOperationResult( Int32 /* operationResult */ ) )
	{
		_NeedBeClosed = true;
		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::GetVolumeSize( UInt32 index, UInt64 *size ) )
	{
		if( VolumesSizes.Size() == 0 )
			return S_FALSE;

		if( index >= (UInt32)VolumesSizes.Size() )
			index = VolumesSizes.Size() - 1;

		*size = VolumesSizes[index];

		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::GetVolumeStream( UInt32 index, ISequentialOutStream **volumeStream ) )
	{
		wchar_t temp[16];
		ConvertUInt32ToString( index + 1, temp );

		UString res = temp;
		while( res.Len() < 2 )
			res.InsertAtFront( L'0' );

		UString fileName = VolName;
		fileName += '.';
		fileName += res;
		fileName += VolExt;

		COutFileStream *streamSpec = new COutFileStream;
		CMyComPtr<ISequentialOutStream> streamLoc( streamSpec );

		if( !streamSpec->Create( us2fs( fileName ), false ) )
			return ::GetLastError();

		*volumeStream = streamLoc.Detach();

		return S_OK;
	}

	Z7_COM7F_IMF( CArchiveUpdateCallback::CryptoGetTextPassword2( Int32 *passwordIsDefined, BSTR *password ) )
	{
		if( !PasswordIsDefined )
		{
			if( AskPassword )
			{
				// You can ask real password here from user
				// Password = GetPassword(OutStream);
				// PasswordIsDefined = true;
			//	PrintError("Password is not defined");
				return E_ABORT;
			}
		}

		*passwordIsDefined = BoolToInt( PasswordIsDefined );

		return StringToBstr( Password, password );
	}

	//
	// Open archive
	//
	bool CArch7zip::readEntries()
	{
		const GUID *clsid = getClsidFromFileName( _fileName );

		CMyComPtr<IInArchive> archive;
		if( CreateObject( clsid, &IID_IInArchive, (void **)&archive ) != S_OK )
		{
			_errorMessage = L"Cannot get class object";
			return false;
		}

		CInFileStream *fileSpec = new CInFileStream;
		CMyComPtr<IInStream> file = fileSpec;

		if( !fileSpec->Open( PathUtils::getExtendedPath( _fileName ).c_str() ) )
		{
			_errorMessage = L"Cannot open archive file";
			return false;
		}

		{
			CArchiveOpenCallback *openCallbackSpec = new CArchiveOpenCallback;
			CMyComPtr<IArchiveOpenCallback> openCallback( openCallbackSpec );
			openCallbackSpec->Init( this );

			openCallbackSpec->PasswordIsDefined = PasswordIsDefined;
			openCallbackSpec->Password = Password;

			const UInt64 scanSize = 1 << 23;
			if( archive->Open( file, &scanSize, openCallback ) != S_OK )
			{
				_errorMessage = L"Cannot open file as archive";
				return false;
			}
		}

		//{
			// Is archive encrypted?
		//	NCOM::CPropVariant prop;
		//	archive->GetProperty( 0, kpidEncrypted, &prop );

		//	_passwordIsDefined = !!prop.boolVal;
		//	_password = _password;
		//}

		{
			Commander::EntryData entry = { 0 };
			wchar_t tmpName[MAX_PATH] = { 0 };

			// List command
			UInt32 numItems = 0;
			archive->GetNumberOfItems( &numItems );
			for( UInt32 i = 0; i < numItems && _pWorker->isRunning(); i++ )
			{
				getEntryInfo( archive, i, &entry.wfd );
				_dataEntries.push_back( entry );
			}
		}

		return true;

		/*ISzAlloc allocImp, allocTempImp;
		allocImp.Alloc = allocTempImp.Alloc = SzAlloc;
		allocImp.Free = allocTempImp.Free = SzFree;
  
		CFileInStream archiveStream;
		InFile_OpenW( &archiveStream.file, _fileName.c_str() );
		FileInStream_CreateVTable( &archiveStream );

		CLookToRead2 lookStream;
		LookToRead2_CreateVTable( &lookStream, False );
		lookStream.buf = (Byte*)ISzAlloc_Alloc( &allocImp, _buffSize );
		lookStream.bufSize = _buffSize;
		lookStream.realStream = &archiveStream.vt;
		LookToRead2_Init( &lookStream );

		CrcGenerateTable();

		CSzArEx db;
		SzArEx_Init( &db );

		SRes res = SzArEx_Open( &db, &lookStream.vt, &allocImp, &allocTempImp );
		if( res == SZ_OK )
		{
			WIN32_FIND_DATA wfd = { 0 };
			wchar_t tmpName[MAX_PATH] = { 0 };

			for( UInt32 i = 0; i < db.NumFiles; ++i )
			{
				if( !_pWorker->isRunning() ) {
					res = SZ_ERROR_PROGRESS;
					break;
				}

				auto len = SzArEx_GetFileNameUtf16( &db, i, reinterpret_cast<UInt16*>( tmpName ) );
				_ASSERTE( len < sizeof( tmpName ) && L"Buffer too small." );

				for( size_t j = 0; j < len; ++j )
					wfd.cFileName[j] = ( tmpName[j] == L'/' ? L'\\' : tmpName[j] );

				getEntryInfo( db, i, &wfd );
				_dataEntries.push_back( wfd );
			}
		}
		else
			_errorMessage = getErrorMessage( res );

		SzArEx_Free( &db, &allocImp );
		ISzAlloc_Free( &allocImp, lookStream.buf );

		File_Close( &archiveStream.file );

		return ( res == SZ_OK );*/
	}

	std::wstring CArch7zip::getErrorMessage( int errorCode )
	{
		std::wostringstream outMessage;
		outMessage << L"Error: (" << errorCode << L") ";

		switch( errorCode )
		{
		case SZ_OK:
			outMessage << L"Ok.";
			break;
		case SZ_ERROR_DATA:
			outMessage << L"Data error.";
			break;
		case SZ_ERROR_MEM:
			outMessage << L"Memory Error.";
			break;
		case SZ_ERROR_CRC:
			outMessage << L"Crc error.";
			break;
		case SZ_ERROR_UNSUPPORTED:
			outMessage << L"Unsupported error.";
			break;
		case SZ_ERROR_PARAM:
			outMessage << L"Params error.";
			break;
		case SZ_ERROR_INPUT_EOF:
			outMessage << L"Eof error.";
			break;
		case SZ_ERROR_OUTPUT_EOF:
			outMessage << L"Output error.";
			break;
		case SZ_ERROR_READ:
			outMessage << L"Read error.";
			break;
		case SZ_ERROR_WRITE:
			outMessage << L"Write error.";
			break;
		case SZ_ERROR_PROGRESS:
			outMessage << L"Progress error.";
			break;
		case SZ_ERROR_FAIL:
			outMessage << L"Fail error.";
			break;
		case SZ_ERROR_THREAD:
			outMessage << L"Thread error.";
			break;
		case SZ_ERROR_ARCHIVE:
			outMessage << L"Archive error.";
			break;
		case SZ_ERROR_NO_ARCHIVE:
			outMessage << L"No archive error.";
			break;
		}

		return outMessage.str();
	}

	void CArch7zip::prepareIndicesFromEntries( IInArchive *archive, const std::vector<std::wstring>& entries )
	{
		_entryIndices.clear();
		_entryNames.clear();

		std::wstring entryName;
		entryName.resize( MAX_WPATH );

		UInt32 numItems = 0;
		archive->GetNumberOfItems( &numItems );

		for( UInt32 i = 0; i < numItems && _pWorker->isRunning(); i++ )
		{
			// Get name of file
			NCOM::CPropVariant prop;
			archive->GetProperty( i, kpidPath, &prop );

			if( prop.vt == VT_BSTR )
				entryName = prop.bstrVal;

			if( shouldExtractEntry( entries, entryName ) )
			{
				_entryNames.push_back( entryName );
				_entryIndices.push_back( i );
			}
		}
	}

	//
	// Extract archive to given directory
	//
	bool CArch7zip::extractEntries( const std::vector<std::wstring>& entries, const std::wstring& targetDir )
	{
		const GUID *clsid = getClsidFromFileName( _fileName );

		CMyComPtr<IInArchive> archive;
		if( CreateObject( clsid, &IID_IInArchive, (void **)&archive ) != S_OK )
		{
			_errorMessage = L"CArch7zip: Cannot get class object.";
			return false;
		}

		CInFileStream *fileSpec = new CInFileStream;
		CMyComPtr<IInStream> file = fileSpec;

		if( !fileSpec->Open( PathUtils::getExtendedPath( _fileName ).c_str() ) )
		{
			_errorMessage = L"CArch7zip: Cannot open archive file.";
			return false;
		}

		{
			CArchiveOpenCallback *openCallbackSpec = new CArchiveOpenCallback;
			CMyComPtr<IArchiveOpenCallback> openCallback( openCallbackSpec );
			openCallbackSpec->Init( this );
			openCallbackSpec->PasswordIsDefined = PasswordIsDefined;
			openCallbackSpec->Password = Password;

			const UInt64 scanSize = 1 << 23;
			if( archive->Open( file, &scanSize, openCallback ) != S_OK )
			{
				_errorMessage = L"CArch7zip: Cannot open file as an archive.";
				return false;
			}
		}

		{
			// Extract command
			CArchiveExtractCallback *extractCallbackSpec = new CArchiveExtractCallback;
			CMyComPtr<IArchiveExtractCallback> extractCallback( extractCallbackSpec );
			extractCallbackSpec->Init( this, archive, PathUtils::getExtendedPath( targetDir ).c_str() ); // second parameter is output folder path

			FsUtils::makeDirectory( targetDir );

			CMyComPtr<IProgress> progressCallback;
			HRESULT result = archive->QueryInterface( IID_IProgress, (void **)&progressCallback );

			extractCallbackSpec->PasswordIsDefined = PasswordIsDefined;
			extractCallbackSpec->Password = Password;

			// set properties
			const wchar_t *names[] = { L"mt", L"mtf" };
			const unsigned kNumProps = sizeof( names ) / sizeof( names[0] );
			NCOM::CPropVariant values[kNumProps] = { (UInt32)1, false };
			CMyComPtr<ISetProperties> setProperties;
			archive->QueryInterface( IID_ISetProperties, (void **)&setProperties );
			if( setProperties )
				setProperties->SetProperties( names, values, kNumProps );

			// do extract content
			prepareIndicesFromEntries( archive, entries );
			result = archive->Extract( &_entryIndices[0], (UInt32)_entryIndices.size(), BoolToInt( false ), extractCallback );

			if( FAILED( result ) || !_errorMessage.empty() )
			{
				if( _errorMessage.empty() )
					_errorMessage = SysUtils::getErrorMessage( result );

				return false;
			}
		}

		return true;

		/*ISzAlloc allocImp, allocTempImp;
		allocImp.Alloc = allocTempImp.Alloc = SzAlloc;
		allocImp.Free = allocTempImp.Free = SzFree;
  
		CFileInStream archiveStream;
		InFile_OpenW( &archiveStream.file, _fileName.c_str() );
		FileInStream_CreateVTable( &archiveStream );

		CLookToRead2 lookStream;
		LookToRead2_CreateVTable( &lookStream, False );
		lookStream.buf = (Byte*)ISzAlloc_Alloc( &allocImp, _buffSize );
		lookStream.bufSize = _buffSize;
		lookStream.realStream = &archiveStream.vt;
		LookToRead2_Init( &lookStream );

		CrcGenerateTable();

		CSzArEx db;
		SzArEx_Init( &db );

		SRes res = SzArEx_Open( &db, &lookStream.vt, &allocImp, &allocTempImp );
		if( res == SZ_OK )
		{
			UInt32 blockIdx = 0xFFFFFFFF; // it can have any value before first call (if outBuffer = 0)
			Byte *outBuffer = 0; // it must be 0 before first call for each new archive.
			size_t outBufferSize = _buffSize;  // it can have any value before first call (if outBuffer = 0)
			size_t offset = 0;
			size_t outSizeProcessed = 0;
			wchar_t fileName[MAX_PATH] = { 0 };

			for( UInt32 i = 0; i < db.NumFiles && _pWorker->isRunning(); ++i )
			{
				auto len = SzArEx_GetFileNameUtf16( &db, i, reinterpret_cast<UInt16*>( fileName ) );

				std::wstring outName = fileName;
				PathUtils::unifyDelimiters( outName );

				if( shouldExtractEntry( entries, outName ) )
				{
					_pWorker->sendMessage( FC_ARCHPROCESSINGPATH, (LPARAM)outName.c_str() );

					offset = outSizeProcessed = 0;
					res = SzArEx_Extract( &db, &lookStream.vt, i, &blockIdx, &outBuffer, &outBufferSize, &offset, &outSizeProcessed, &allocImp, &allocTempImp );

					if( res != SZ_OK )
						break;

					_pWorker->sendNotify( FC_ARCHBYTESRELATIVE, (LPARAM)outSizeProcessed );

					auto path = PathUtils::getFullPath( targetDir + outName );
					auto attr = SzBitWithVals_Check( &db.Attribs, i ) ? db.Attribs.Vals[i] : 0;

					FsUtils::makeDirectory( ( attr & FILE_ATTRIBUTE_DIRECTORY ) ? path : PathUtils::stripFileName( path ) );

					CSzFile outFile;
					OutFile_OpenW( &outFile, path.c_str() );
					File_Write( &outFile, outBuffer + offset, &outSizeProcessed );
					File_Close( &outFile );
				}
			}

			IAlloc_Free( &allocImp, outBuffer );
		}
		else
			_errorMessage = getErrorMessage( res );

		SzArEx_Free( &db, &allocImp );
		ISzAlloc_Free( &allocImp, lookStream.buf );

		File_Close( &archiveStream.file );

		return ( res == SZ_OK );*/
	}

	//
	// Get entries' full info from paths
	//
	void CArch7zip::prepareEntriesInfo( const std::vector<std::wstring>& entries )
	{
		_entriesData.Clear();
		_localPathIdx = PathUtils::stripFileName( entries[0] ).length() + 1;

		for( size_t i = 0; i < entries.size(); i++ )
		{
			const auto& name = entries[i];

			WIN32_FIND_DATA wfd = { 0 };
			HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( name ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
			if( hFile != INVALID_HANDLE_VALUE )
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					addFileEntry( name, wfd );
					addDirEntry( name );
				}
				else
					addFileEntry( name, wfd );

				FindClose( hFile );
			}
			else
			{
				PrintDebug( "Can't find file '%ls'", name.c_str() );
				continue;
			}
		}
	}

	void CArch7zip::addFileEntry( const std::wstring& fileName, WIN32_FIND_DATA& wfd )
	{
		CArch7zip::EntryData di;

		di.Attrib = wfd.dwFileAttributes;
		di.Size = FsUtils::getFileSize( &wfd );
		di.CTime = wfd.ftCreationTime;
		di.ATime = wfd.ftLastAccessTime;
		di.MTime = wfd.ftLastWriteTime;
		di.Name = fileName.substr( _localPathIdx ).c_str();
		di.FullPath = PathUtils::getExtendedPath( fileName ).c_str();

		_entriesData.Add( di );
	}

	void CArch7zip::addDirEntry( const std::wstring& dirName )
	{
		auto pathFind = PathUtils::addDelimiter( dirName );

		WIN32_FIND_DATA wfd = { 0 };
		HANDLE hFile = FindFirstFileEx( ( PathUtils::getExtendedPath( pathFind ) + L"*" ).c_str(), FindExInfoStandard, &wfd, FindExSearchNameMatch, NULL, FCS::inst().getApp().getFindFLags() );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
				{
					if( !( wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT )
						&& !PathUtils::isDirectoryDotted( wfd.cFileName )
						&& !PathUtils::isDirectoryDoubleDotted( wfd.cFileName ) )
					{
						addFileEntry( pathFind + wfd.cFileName, wfd );
						addDirEntry( pathFind + wfd.cFileName );
					}
				}
				else
				{
					addFileEntry( pathFind + wfd.cFileName, wfd );
				}
			} while( FindNextFile( hFile, &wfd ) && _pWorker->isRunning() );

			FindClose( hFile );
		}
	}

	//
	// Pack entries into archive to given directory
	//
	bool CArch7zip::packEntries( const std::vector<std::wstring>& entries, const std::wstring& targetNane )
	{
		if( !entries.empty() )
		{
			COutFileStream *outFileStreamSpec = new COutFileStream;
			CMyComPtr<IOutStream> outFileStream = outFileStreamSpec;
			if( !outFileStreamSpec->Create( PathUtils::getExtendedPath( targetNane ).c_str(), false ) )
			{
				_errorMessage = L"CArch7zip: Cannot create archive file.";
				return false;
			}

			CMyComPtr<IOutArchive> outArchive;
			if( CreateObject( &CLSID_CFormat7z, &IID_IOutArchive, (void **)&outArchive ) != S_OK )
			{
				_errorMessage = L"CArch7zip: Cannot get class object.";
				return false;
			}

			CArchiveUpdateCallback *updateCallbackSpec = new CArchiveUpdateCallback;
			CMyComPtr<IArchiveUpdateCallback2> updateCallback( updateCallbackSpec );
			prepareEntriesInfo( entries );
			updateCallbackSpec->Init( this, &_entriesData );
			updateCallbackSpec->PasswordIsDefined = PasswordIsDefined;
			updateCallbackSpec->Password = Password;

			{
				// set archiver properties
				const wchar_t *names[] =
				{
				  L"s",
				  L"x"
				};
				const unsigned kNumProps = Z7_ARRAY_SIZE( names );
				NCOM::CPropVariant values[kNumProps] =
				{
				  false,    // solid mode OFF
				  (UInt32)9 // compression level = 9 - ultra
				};
				CMyComPtr<ISetProperties> setProperties;
				outArchive->QueryInterface( IID_ISetProperties, (void **)&setProperties );
				if( !setProperties )
				{
					_errorMessage = L"CArch7zip: ISetProperties unsupported";
					return false;
				}
				else
					setProperties->SetProperties( names, values, kNumProps );
			}

			HRESULT result = outArchive->UpdateItems( outFileStream, _entriesData.Size(), updateCallback );
			updateCallbackSpec->Finalize();

			if( FAILED( result ) )
				_errorMessage = SysUtils::getErrorMessage( result );
			else
				return true;
		}

		return false;
	}

	//
	// Convert entry header to WIN32_FIND_DATA
	//
	void CArch7zip::getEntryInfo( IInArchive *archive, int fileIdx, WIN32_FIND_DATA *wfd )
	{
		{
			// Get name of file
			NCOM::CPropVariant prop;
			archive->GetProperty( fileIdx, kpidPath, &prop );
			if( prop.vt == VT_BSTR )
				wcsncpy( wfd->cFileName, prop.bstrVal, MAX_PATH );
				//for( size_t j = 0; j < len; ++j )
				//	wfd.cFileName[j] = ( tmpName[j] == L'/' ? L'\\' : tmpName[j] );
			//else if( prop.vt != VT_EMPTY )
			//	;//Print("ERROR!");
		}
		{
			// Get uncompressed size of file
			NCOM::CPropVariant prop;
			archive->GetProperty( fileIdx, kpidSize, &prop );
			wfd->nFileSizeLow = prop.uhVal.LowPart;
			wfd->nFileSizeHigh = prop.uhVal.HighPart;
		}
		{
			// Get attributes
			NCOM::CPropVariant prop;
			archive->GetProperty( fileIdx, kpidAttrib, &prop );
			wfd->dwFileAttributes = prop.uintVal;
			//char s[32];
			//ConvertPropVariantToShortString( prop, s );
		}
		{
			// Get date and time of file
			NCOM::CPropVariant prop;
			archive->GetProperty( fileIdx, kpidMTime, &prop );
			wfd->ftLastWriteTime = prop.filetime;
		}
	}
}
