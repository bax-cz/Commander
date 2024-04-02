#pragma once

/*
	Registry data provider interface for reading data from windows registry
*/

#include "BaseDataProvider.h"

namespace Commander
{
	class CRegistryDataProvider : public CBaseDataProvider
	{
	public:
		CRegistryDataProvider();
		~CRegistryDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

	private:
		void getKeyName( const std::wstring& path, const std::wstring& name );
		//bool queryKey( HKEY hKey, const std::wstring& path );
		//bool readSubKeys( HKEY hKey, DWORD count );
		bool _readDataCore();

	private:
		std::map<std::wstring, HKEY> _rootKeys;
		std::string _data;
		std::wstring _rootName;
		std::wstring _keyName;
		std::wstring _valName;
	};
}
