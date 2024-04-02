#pragma once

/*
	Palm data provider interface for decoding .pdb/.mobi/.azw3 e-books
*/

#include "BaseDataProvider.h"

namespace Commander
{
	class CPalmDataProvider : public CBaseDataProvider
	{
	public:
		CPalmDataProvider();
		~CPalmDataProvider();

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) override;

	//	virtual void onWorkerNotify( UINT msgId, ULONGLONG workerId, int retVal ) override;

	private:
		void addImageSources( const std::wstring& fileName );
		const char *processHtml( std::ifstream& fIn, size_t off, std::string& tag, std::ofstream& fOut );
		bool _readDataCore();

	private:
		char _buf[0x8000];
		std::streamsize _read;
		std::wstring _currentDirectory;
	};
}
