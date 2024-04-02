#include "stdafx.h"

#include "Commander.h"
#include "RegistryReader.h"
#include "IconUtils.h"
#include "StringUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CRegistryReader::CRegistryReader( ESortMode mode, HWND hwndNotify )
		: CBaseReader( mode, hwndNotify )
	{
		_status = EReaderStatus::DataOk;
		_root = ReaderType::getModePrefix( EReadMode::Reged );

		INITIALIZE_ROOTKEY( HKEY_CLASSES_ROOT );
		INITIALIZE_ROOTKEY( HKEY_CURRENT_USER );
		INITIALIZE_ROOTKEY( HKEY_LOCAL_MACHINE );
		INITIALIZE_ROOTKEY( HKEY_USERS );
		INITIALIZE_ROOTKEY( HKEY_PERFORMANCE_DATA );
		INITIALIZE_ROOTKEY( HKEY_CURRENT_CONFIG );

		LvUtils::setImageList( hwndNotify, FCS::inst().getApp().getRegedImageList(), LVSIL_SMALL );
	}

	CRegistryReader::~CRegistryReader()
	{
		LvUtils::setImageList( _hWndNotify, FCS::inst().getApp().getSystemImageList(), LVSIL_SMALL );
	}

	//
	// Add parent (double dotted) directory entry
	//
	void CRegistryReader::addParentDirEntry()
	{
		EntryData data = { 0 };
		data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		wcscpy( data.wfd.cFileName, PathUtils::getDoubleDottedDirectory().c_str() );

		_dirEntries.push_back( std::make_shared<EntryData>( data ) );
	}

	//
	// Query subkey data
	//
	bool CRegistryReader::queryKey( HKEY hKey, const std::wstring& path )
	{
		HKEY hTempKey;
		LSTATUS res = 0;

		if( ( res = RegOpenKeyEx( hKey, path.c_str(), 0, KEY_READ, &hTempKey ) ) == ERROR_SUCCESS )
		{
			//TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
			//DWORD    cbName;                   // size of name string 
			TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
			DWORD    cchClassName = MAX_PATH;  // size of class string 
			DWORD    cSubKeys = 0;               // number of subkeys 
			DWORD    cbMaxSubKey;              // longest subkey size 
			DWORD    cchMaxClass;              // longest class string 
			DWORD    cValues;              // number of values for key 
			DWORD    cchMaxValue;          // longest value name 
			DWORD    cbMaxValueData;       // longest value data 
			DWORD    cbSecurityDescriptor; // size of security descriptor 
			FILETIME ftLastWriteTime;      // last write time 

			// Get the class name and the value count. 
			auto res = RegQueryInfoKey(
				hTempKey,                // key handle 
				achClass,                // buffer for class name 
				&cchClassName,           // size of class string 
				NULL,                    // reserved 
				&cSubKeys,               // number of subkeys 
				&cbMaxSubKey,            // longest subkey size 
				&cchMaxClass,            // longest class string 
				&cValues,                // number of values for this key 
				&cchMaxValue,            // longest value name 
				&cbMaxValueData,         // longest value data 
				&cbSecurityDescriptor,   // security descriptor 
				&ftLastWriteTime);       // last write time 

			readSubKeys( hTempKey, cSubKeys );
			readValues( hTempKey, cValues );

			RegCloseKey( hTempKey );
		}

		return ( res == ERROR_SUCCESS );
	}

	//
	// Enumerate all subkeys
	//
	bool CRegistryReader::readSubKeys( HKEY hKey, DWORD count )
	{
		EntryData data = { 0 };
		data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

		FILETIME ftTemp;
		LSTATUS res = 0;
		DWORD idx = 0;
		DWORD dwKeyLen;

		while( res == ERROR_SUCCESS )
		{
			dwKeyLen = MAX_PATH;
			res = RegEnumKeyEx( hKey, idx++, (LPWSTR)data.wfd.cFileName, &dwKeyLen, NULL, NULL, NULL, &ftTemp );

			if( res == ERROR_SUCCESS )
			{
				data.wfd.ftLastWriteTime = ftTemp;
				data.date = FsUtils::getFormatStrFileDate( &data.wfd );
				data.time = FsUtils::getFormatStrFileTime( &data.wfd );
				data.size = L"KEY";

				_dirEntries.push_back( std::make_shared<EntryData>( data ) );
			}
		}

		return ( res == ERROR_SUCCESS );
	}

	int getKeyType( DWORD type )
	{
		int idx = 0;

		switch( type )
		{
		case REG_SZ:
		case REG_EXPAND_SZ: // use ExpandEnvironmentStrings
		case REG_LINK:
		case REG_MULTI_SZ:
			idx = 1;
			break;
		case REG_NONE:
		case REG_BINARY:
		case REG_DWORD: // same as REG_DWORD_LITTLE_ENDIAN
		case REG_DWORD_BIG_ENDIAN:
		case REG_RESOURCE_LIST:
		case REG_FULL_RESOURCE_DESCRIPTOR:
		case REG_RESOURCE_REQUIREMENTS_LIST:
		case REG_QWORD: // same as REG_QWORD_LITTLE_ENDIAN
			idx = 2;
			break;
		}

		return idx;
	}

	//
	// Enumerate all key's values
	//
	bool CRegistryReader::readValues( HKEY hKey, DWORD count )
	{
		EntryData data = { 0 };

		TCHAR achValue[MAX_VALUE_NAME];
		DWORD cchValue = MAX_VALUE_NAME;
		DWORD dwSize = 0, dwType = 0;

		for( DWORD i = 0, res = ERROR_SUCCESS; i < count; ++i )
		{
			cchValue = MAX_VALUE_NAME;
			achValue[0] = '\0';

			res = RegEnumValue( hKey, i,
				achValue,
				&cchValue,
				NULL,
				&dwType,
				NULL,
				&dwSize );

			if( res == ERROR_SUCCESS )
			{
				data.wfd.nFileSizeLow = dwSize;
				data.imgSystem = getKeyType( dwType );
				data.size = FsUtils::bytesToString2( dwSize );
				wcsncpy( data.wfd.cFileName, ( cchValue ? achValue : L"(Default)" ), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );

				_fileEntries.push_back( std::make_shared<EntryData>( data ) );
			}
		}

		return true;
	}

	//
	// Read entries data
	//
	bool CRegistryReader::readEntries( std::wstring& path )
	{
		if( StringUtils::startsWith( path, ReaderType::getModePrefix( EReadMode::Reged ) ) )
		{
			_dirEntries.clear();
			_fileEntries.clear();

			auto localPath = path.substr( wcslen( ReaderType::getModePrefix( EReadMode::Reged ) ) + 1 );

			// read root keys
			if( localPath.empty() )
			{
				EntryData data = { 0 };
				data.wfd.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

				for( auto& key : _rootKeys )
				{
					wcsncpy( data.wfd.cFileName, key.first.c_str(), ARRAYSIZE( WIN32_FIND_DATA::cFileName ) );
					_dirEntries.push_back( std::make_shared<EntryData>( data ) );
				}
			}
			else // read subkeys from directory
			{
				auto idx = localPath.find_first_of( L'\\' );
				auto root = localPath.substr( 0, idx );

				addParentDirEntry();

				if( _rootKeys.find( root ) != _rootKeys.end() )
				{
					idx = ( idx == std::wstring::npos ) ? root.length() : idx + 1;
					queryKey( _rootKeys[root], localPath.substr( idx ) );
				}
			}

			_status = EReaderStatus::DataOk;

			// do sorting
			sortEntries();

			return true;
		}

		_status = EReaderStatus::Invalid;

		return false;
	}

	//
	// Calculate size - return true means operation successfully done on ui thread
	//
	bool CRegistryReader::calculateSize( const std::vector<int>& entries )
	{
		_resultSize = 0ull;

		for( const auto& idx : entries )
		{
			auto entryData = getEntry( idx );

			if( entryData )
			{
				if( IsDir( entryData->wfd.dwFileAttributes ) )
					;//_upArchiver->calculateDirectorySize( entryData->wfd.cFileName, &_resultSize );
				else
					_resultSize += FsUtils::getFileSize( &entryData->wfd );
			}
		}

		return true;
	}
}
