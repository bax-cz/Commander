#pragma once

#include "BaseDialog.h"

namespace Commander
{
	class CChangeAttributes : public CBaseDialog
	{
	public:
		static const UINT resouceIdTemplate = IDD_CHANGEATTRIB;

	private:
		struct CAttributeData
		{
			DWORD dwFileAttributes;
			DWORD dwFileAttributesIgnore;
			FILETIME ftCreationTime;
			FILETIME ftLastAccessTime;
			FILETIME ftLastWriteTime;
		};

	public:
		virtual void onInit() override;
		virtual bool onOk() override;

		virtual INT_PTR CALLBACK dialogProc( UINT message, WPARAM wParam, LPARAM lParam ) override;

	private:
		inline UINT get3State( UINT cntAll, UINT cnt ) { return cnt == cntAll ? BST_CHECKED : cnt ? BST_INDETERMINATE : BST_UNCHECKED; }

	private:
		bool doJob( const std::wstring& item, const CAttributeData& attrData );
		void doJobOnItem( const std::wstring& item, const CAttributeData& attrData );
		void setFileAttrEncrypted( const std::wstring& item, const CAttributeData& attrData, DWORD& attrOrig );
		void setFileAttrCompressed( const std::wstring& item, const CAttributeData& attrData, DWORD& attrOrig );
		void updateDateTimePickers( UINT dtpFirstId, UINT dtpSecondId, bool checked );
		void updateSetCurrentButtonStatus( bool checked );
		void setCurrentDateTime();
		void setAttributeData( UINT itemId, DWORD attrib, CAttributeData& attrData );
		void restoreAttributes();
		void restoreDateTimeFromFile( const std::wstring& item );

	private:
		INT_PTR onNotifyMessage( LPNMHDR pNmhdr );
		INT_PTR onDateTimeChange( LPNMDATETIMECHANGE pDtChange );

	private:
		std::vector<std::wstring> _items;
		bool _includeSubDirectories;
	};
}
