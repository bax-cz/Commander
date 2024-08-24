#include "stdafx.h"

#include "BackgroundWorker.h"

namespace Commander
{
	CBackgroundWorker::CBackgroundWorker()
		: _workerId( 0ull )
		, _hWndNotify( nullptr )
		, _msgId( 0 )
		, _isRunning( false )
		, _isFinished( false )
		, _notifyFinished( false )
	{
	}

	CBackgroundWorker::~CBackgroundWorker()
	{
		stop();
	}

	void CBackgroundWorker::init( std::function<bool()> func, HWND hWnd, DWORD messageId )
	{
		_funcToExecute = func;
		_hWndNotify = hWnd;
		_msgId = messageId;
	}

	bool CBackgroundWorker::start()
	{
		stop();

		// start worker thread
		_isRunning = true;
		_isFinished = false;
		_notifyFinished = false;

		_upWorkerThread = std::make_unique<std::thread>( [this] {
			_workerThreadProc();
		} );

		return _upWorkerThread != nullptr;
	}

	void CBackgroundWorker::stop()
	{
		// signal stop thread and join
		_isRunning = false;

		if( _upWorkerThread && _upWorkerThread->joinable() )
		{
			_upWorkerThread->join();
			_upWorkerThread = nullptr;
		}
	}

	void CBackgroundWorker::stop_async( bool notifyWhenFinished )
	{
		_notifyFinished = notifyWhenFinished;
		_isRunning = false;
	}

	void CBackgroundWorker::_workerThreadProc()
	{
		std::stringstream sstr;
		sstr << std::this_thread::get_id();

		_workerId = std::stoull( sstr.str() );

		// execute the user function
		bool retVal = _funcToExecute();

		if( _isRunning || _notifyFinished )
		{
			_isRunning = false;
			_isFinished = true;

			// send notification
			sendNotify( static_cast<WPARAM>( retVal ), static_cast<LPARAM>( _workerId ) );
		}
		else
			_isFinished = true;
	}
}
