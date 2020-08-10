#pragma once
#include "Filter.h"

class CustomRTCTransceiver;
class netsetDlg;

class netsetDlg :
	public CDialog
{
public:
	netsetDlg();
	~netsetDlg();
	void PostNcDestroy();
	CustomRTCTransceiver* m_pFilter;
	DECLARE_MESSAGE_MAP()
	afx_msg void OnIdok();
	afx_msg void OnLocalRadio1();
	afx_msg void OnLocalRadio2();
	afx_msg void OnExtRadio1();
	afx_msg void OnExtRadio2();
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	virtual void DoDataExchange(CDataExchange* pDX);
};

