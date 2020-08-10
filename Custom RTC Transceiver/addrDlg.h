#pragma once
#include "resource.h"

// addrDlg dialog

class addrDlg : public CDialogEx
{
	DECLARE_DYNAMIC(addrDlg)

public:
	addrDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~addrDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FORMVIEW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
};
