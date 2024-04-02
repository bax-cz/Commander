#pragma once

/*
	Registry reader interface for reading from windows registry
*/

#include "BaseReader.h"

#define INITIALIZE_ROOTKEY( key ) ( _rootKeys[L#key] = ( key ) )

namespace Commander
{
	class CRegistryReader : public CBaseReader
	{
	private:
		static constexpr DWORD MAX_KEY_LENGTH = 255;
		static constexpr DWORD MAX_VALUE_NAME = 16383;

	public:
		CRegistryReader( ESortMode mode, HWND hwndNotify );
		~CRegistryReader();

		virtual bool readEntries( std::wstring& path ) override;
		virtual bool calculateSize( const std::vector<int>& entries ) override;

	private:
		bool queryKey( HKEY hKey, const std::wstring& path );
		bool readSubKeys( HKEY hKey, DWORD count );
		bool readValues( HKEY hKey, DWORD count );
		void addParentDirEntry();

	private:
		std::map<std::wstring, HKEY> _rootKeys;
	};
}
