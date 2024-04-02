#pragma once

/*
	FastCopy manager takes care of fastcopyparams
*/

#include "../FastCopy/fastcopy.h"
#include "Commands.h"
#include "FileProgress.h"

#include <set>

namespace Commander
{
	class CFastCopyManager
	{
	public:
		CFastCopyManager();
		~CFastCopyManager();

		void start( EFcCommand cmd, std::vector<std::wstring> items, std::wstring dirTo = L"" );
		bool stop();

		void onDestroyed( CFileProgress *fastCopy );
		void reportProgress( int percent );

		static FastCopy::Mode getModeName( EFcCommand cmd, std::wstring& title, std::wstring& text );

	private:
		void cleanUp();

	private:
		std::set<CFileProgress*> _fcpArray;
	};
}
