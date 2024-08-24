#pragma once

/*
	CBackgroundWorker - runs jobs in background (in another thread)
*/

#include <thread>
#include <atomic>
#include <memory>
#include <functional>

namespace Commander
{
	//
	// Auxiliary class for async actions
	//
	class CBackgroundWorker
	{
	public:
		CBackgroundWorker();
		~CBackgroundWorker();

	public:
		void init( std::function<bool()> func, HWND hWnd, DWORD messageId );
		bool start();
		void stop(); // signal stop and 'join' thread
		void stop_async( bool notifyWhenFinished = false ); // signal stop without 'join'-ing thread

		inline bool isRunning() const { Sleep( 0 ); /* yield */ return _isRunning; }
		inline bool isFinished() const { return _isFinished; }
		inline bool checkId( ULONGLONG id ) const { return id == _workerId; }

		inline bool sendNotify( WPARAM wPar, LPARAM lPar ) const { return !!SendNotifyMessage( _hWndNotify, _msgId, wPar, lPar ); }
		inline LRESULT sendMessage( WPARAM wPar, LPARAM lPar ) const { return _isRunning ? SendMessage( _hWndNotify, _msgId, wPar, lPar ) : 0; }

		inline HWND getHwnd() const { return _hWndNotify; }

	private:
		void _workerThreadProc();

	private:
		ULONGLONG _workerId;

		std::unique_ptr<std::thread> _upWorkerThread;
		std::atomic<bool> _isRunning;
		std::atomic<bool> _isFinished;
		std::atomic<bool> _notifyFinished;
		std::function<bool()> _funcToExecute;

		HWND   _hWndNotify;
		DWORD  _msgId;
	};
}
