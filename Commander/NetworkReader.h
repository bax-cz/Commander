#pragma once

/*
	Network reader interface for reading neighborhood computers/shared directories
*/

#include "BaseReader.h"

namespace Commander
{
	class CNetworkReader : public CBaseReader
	{
	public:
		CNetworkReader( ESortMode mode, HWND hwndNotify );
		~CNetworkReader();

		virtual bool isPathValid( const std::wstring& path ) override;
		virtual bool readEntries( std::wstring& path ) override;
		virtual bool calculateSize( const std::vector<int>& entries ) override { return true; }
		virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		void addParentDirEntry();
		bool _scanNetCore();
	};
}
