#pragma once

/*
	Base data provider interface for accessing disk, archive, ftp, ssh, registry data
*/

#include "DataTypes.h"
#include "BackgroundWorker.h"

namespace Commander
{
	class CBaseDataProvider
	{
	public:
		CBaseDataProvider() : _hWndNotify( nullptr ), _readToFile( false ) {}
		virtual ~CBaseDataProvider() = default;

		virtual bool readData( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) = 0;
		virtual bool readToFile( const std::wstring& fileName, std::shared_ptr<CPanelTab> spPanel, HWND hWndNotify ) = 0;

		inline std::wstring getResult() { return _result; }
		inline std::wstring getErrorMessage() { return _errorMessage; }

		//inline EReaderStatus getStatus() { return _status; }
		//inline ULONGLONG getResultSize() { return _resultSize; }
		//inline std::mutex& getMutex() { return _mutex; }

	protected:
		std::shared_ptr<CPanelTab> _spPanel;

		std::mutex _mutex;
		std::wstring _fileName;
		std::wstring _result;
		std::wstring _errorMessage;

		//EReaderStatus _status;
		//ULONGLONG _resultSize;
		HWND _hWndNotify;
		bool _readToFile;

		CBackgroundWorker _worker;
	};
}
