#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CChangeCase: public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_CHANGECASE;

	public:
		virtual void onInit() override;
		virtual bool onOk() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		bool doJob( const std::wstring& item, UINT reqCase, UINT reqFile );
		void doJobOnItem( const std::wstring& item, UINT reqCase, UINT reqFile );
		std::wstring getItemNewName( const std::wstring& item, UINT reqCase, UINT reqFile );

		void updatePreview();

	private:
		void convertToLower( std::wstring& fname, std::wstring& fext, UINT reqFile );
		void convertToUpper( std::wstring& fname, std::wstring& fext, UINT reqFile );
		void convertToPartMixed( std::wstring& fname, std::wstring& fext, UINT reqFile );
		void convertToMixed( std::wstring& fname, std::wstring& fext, UINT reqFile );

	private:
		std::vector<std::wstring> _items;
		bool _includeSubDirectories;
	};
}
