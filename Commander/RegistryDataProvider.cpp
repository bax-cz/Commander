#include "stdafx.h"

#include "Commander.h"
#include "RegistryDataProvider.h"
#include "MiscUtils.h"
#include "StringUtils.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CRegistryDataProvider::CRegistryDataProvider()
	{
		//_status = EReaderStatus::DataOk;
		//_root = ReaderType::getModePrefix( EReadMode::Reged );

		INITIALIZE_ROOTKEY( HKEY_CLASSES_ROOT );
		INITIALIZE_ROOTKEY( HKEY_CURRENT_USER );
		INITIALIZE_ROOTKEY( HKEY_LOCAL_MACHINE );
		INITIALIZE_ROOTKEY( HKEY_USERS );
		INITIALIZE_ROOTKEY( HKEY_PERFORMANCE_DATA );
		INITIALIZE_ROOTKEY( HKEY_CURRENT_CONFIG );
	}

	CRegistryDataProvider::~CRegistryDataProvider()
	{
	}

	void CRegistryDataProvider::getKeyName( const std::wstring& path, const std::wstring& name )
	{
		auto localPath = path.empty()
			? name.substr( wcslen( ReaderType::getModePrefix( EReadMode::Reged ) ) + 1 )
			: path.substr( wcslen( ReaderType::getModePrefix( EReadMode::Reged ) ) + 1 );

		_rootName = localPath.substr( 0, localPath.find_first_of( L'\\' ) );

		_keyName = path.empty()
			? PathUtils::stripFileName( localPath.substr( _rootName.length() + 1 ) )
			: PathUtils::stripDelimiter( localPath.substr( _rootName.length() + 1 ) );

		_valName = StringUtils::endsWith( name, L"(Default)" ) ? L"" : path.empty()
			? PathUtils::stripPath( name )
			: name.substr( path.length() );
	}

	bool CRegistryDataProvider::_readDataCore()
	{
		HKEY hKey = nullptr;

		LSTATUS ret = RegOpenKeyEx( _rootKeys[_rootName], _keyName.c_str(), 0, KEY_READ, &hKey );

		DWORD valueType, valueLen = 0;

		if( ret == ERROR_SUCCESS )
			ret = RegQueryValueEx( hKey, _valName.c_str(), NULL, &valueType, NULL, &valueLen );

		if( ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA )
		{
			// key does not exist in registry
			if( hKey )
				RegCloseKey( hKey );

			std::wostringstream sstr;
			sstr << L"Root key: " << _rootName << L"\r\n";
			sstr << L"Key: " << _keyName << L"\r\n";
			sstr << L"Value: " << _valName << L"\r\n\r\n";
			sstr << SysUtils::getErrorMessage( ret );

			_errorMessage = sstr.str();

			return false;
		}

		_data.resize( valueLen );
		ret = RegQueryValueEx( hKey, _valName.c_str(), NULL, &valueType, (BYTE*)&_data[0], &valueLen );

		/*if( !_isHex && _requestEncoding == StringUtils::NOBOM && valueType != REG_NONE && valueType != REG_BINARY )
		{
			if( valueType == REG_SZ || valueType == REG_EXPAND_SZ || valueType == REG_MULTI_SZ || valueType == REG_LINK )
			{
				_ASSERTE( ( valueLen % sizeof( WCHAR ) ) == 0 );

				_result.resize( valueLen / sizeof( WCHAR ) );
				ret = RegQueryValueEx( hKey, _valName.c_str(), NULL, &valueType, (BYTE*)&_result[0], &valueLen );

				if( valueType == REG_MULTI_SZ )
					_result = StringUtils::getValueMulti( _result.c_str(), valueLen / sizeof( WCHAR ) );
			}
			else
				_result = MiscUtils::readRegistryValue( hKey, _valName, valueType, valueLen );
		}
		else
		{
			std::string str; str.resize( valueLen );
			RegQueryValueEx( hKey, _valName.c_str(), NULL, &valueType, (BYTE*)&str[0], &valueLen );

			// try to display file as text
			if( !_isHex && !readFileText( std::istringstream( str, valueLen ) ) )
				_isHex = true;

			if( _isHex )
				readFileContentHex( std::istringstream( str, valueLen ) );
		}*/

		RegCloseKey( hKey );

		if( ret == ERROR_SUCCESS && _readToFile )
		{
			// TODO: sanitize filename for _valName (values can contain chars like '\', ':', '|', etc.)
			_result = FCS::inst().getTempPath() + PathUtils::getFullPath( _valName );
			std::ofstream outStr( _result );
			outStr << _data;
		}

		return ( ret == ERROR_SUCCESS );
	}

	bool CRegistryDataProvider::readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );

		getKeyName( spPanel->getDataManager()->getCurrentDirectory(), fileName );
		_readToFile = false;

		return _worker.start();
	}

	bool CRegistryDataProvider::readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify )
	{
		_worker.init( [this] { return _readDataCore(); }, hWndNotify, UM_READERNOTIFY );

		getKeyName( spPanel->getDataManager()->getCurrentDirectory(), fileName );
		_readToFile = true;

		return _worker.start();
	}
}
