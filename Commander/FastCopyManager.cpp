#include "stdafx.h"

#include "Commander.h"
#include "FastCopyManager.h"

namespace Commander
{
	//
	// Constructor/destructor
	//
	CFastCopyManager::CFastCopyManager()
	{
	}

	CFastCopyManager::~CFastCopyManager()
	{
		cleanUp();
	}

	//
	// Cleanup FastCopy instances
	//
	void CFastCopyManager::cleanUp()
	{
		for( auto it = _fcpArray.begin(); it != _fcpArray.end(); /**/ )
		{
			(*it)->close();
			it = _fcpArray.begin();
		}
	}

	//
	// Create FastCopy instance and start job
	//
	void CFastCopyManager::start( EFcCommand cmd, std::vector<std::wstring> items, std::wstring dirTo )
	{
		auto pTaskBarList = FCS::inst().getApp().getTaskbarList();

		if( pTaskBarList )
		{
			pTaskBarList->SetProgressState( FCS::inst().getFcWindow(), TBPF_NORMAL );
			pTaskBarList->SetProgressValue( FCS::inst().getFcWindow(), 0, 100 );
		}

		auto it = _fcpArray.insert( CBaseDialog::createModeless<CFileProgress>() );
		(*it.first)->runJob( cmd, items, dirTo );
	}

	//
	// Stop FastCopy jobs
	//
	bool CFastCopyManager::stop()
	{
		if( _fcpArray.size() )
		{
			if( MessageBox( NULL, L"Stop operation?", L"Operation in progress", MB_YESNO | MB_ICONQUESTION ) == IDYES )
			{
				cleanUp();
			}
			else
				return false;
		}

		return true;
	}

	//
	// FastCopy successfully destroyed - remove from list
	//
	void CFastCopyManager::onDestroyed( CFileProgress *fastCopy )
	{
		if( fastCopy )
		{
			_fcpArray.erase( fastCopy );

			auto pTaskBarList = FCS::inst().getApp().getTaskbarList();

			if( pTaskBarList )
				pTaskBarList->SetProgressState( FCS::inst().getFcWindow(), TBPF_NOPROGRESS );
		}
	}

	//
	// Update progress at taskbar - TODO multiple instances, calc average
	//
	void CFastCopyManager::reportProgress( int percent )
	{
		auto value = percent;
		if( _fcpArray.size() > 1 )
		{
			double sum = 0.0;
			for( auto fcp : _fcpArray )
			{
				sum += static_cast<double>( fcp->getCurrentProgressValue() );
			}
			value = (int)( sum / (double)_fcpArray.size() );
		}

		auto pTaskBarList = FCS::inst().getApp().getTaskbarList();

		if( pTaskBarList )
			pTaskBarList->SetProgressValue( FCS::inst().getFcWindow(), value, 100 );
	}

	//
	// Get fastcopy mode (copy, move, delete)
	//
	FastCopy::Mode CFastCopyManager::getModeName( EFcCommand cmd, std::wstring& title, std::wstring& text )
	{
		FastCopy::Mode mode;
		switch( cmd )
		{
		case EFcCommand::CopyItems:
			mode = FastCopy::DIFFCP_MODE;
			title = L"Copy";
			text = L"Copying";
			break;

		case EFcCommand::MoveItems:
			mode = FastCopy::MOVE_MODE;
			title = L"Move";
			text = L"Moving";
			break;

		case EFcCommand::RecycleItems:
		case EFcCommand::DeleteItems:
			mode = FastCopy::DELETE_MODE;
			title = L"Delete";
			text = L"Deleting";
			break;

		default:
			mode = FastCopy::SYNCCP_MODE;
			title = L"";
			text = L"";
			_ASSERTE( "Command not specified." );
			break;
		}

		return mode;
	}
}
