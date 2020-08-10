#pragma once
#include "Filter.h"

extern HINSTANCE g_hInst;

class CustomRTCTransceiver;
class CThreadedDlg;

class CThreadedDlg : public CWinThread
{
	DECLARE_DYNCREATE(CThreadedDlg)
	virtual ~CThreadedDlg();
protected:
	CThreadedDlg();   // protected constructor used by dynamic creation

public:
	void Setup(CustomRTCTransceiver* filter, UINT nIDTemplate, int nCmdShow = SW_SHOW);

	virtual BOOL InitInstance();
	virtual int ExitInstance();

	//HWND m_hwnd;
	CDialog* m_pDlg;
	UINT     m_nIDTemplate;
	int      m_nCmdShow;
	CustomRTCTransceiver* m_pFilter;
};