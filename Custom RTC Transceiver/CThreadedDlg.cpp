#include "CThreadedDlg.h"
#include "odbgstream.h"
#include "messagemap.h"
#include "resource.h"
#include "netsetDlg.h"

IMPLEMENT_DYNCREATE(CThreadedDlg, CWinThread)
/*
INT_PTR CALLBACK cdialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

INT_PTR CALLBACK cdialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
	wodbgstream os;
	if (wmTranslation.find(message) == wmTranslation.end()) {
		os << "dialog message: " << message << " W: " << wParam << " L: " << lParam << std::endl;
	}
	else {
		os << "dialog message: " << wmTranslation[message] << " W: " << wParam << " L: " << lParam << std::endl;
	}
	//os << "dialog message: " << wmTranslation[message] << std::endl;
#endif
	switch (message)
	{
	case WM_INITDIALOG:
	{
		CheckRadioButton(hwndDlg, IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
		HWND ipaddr = GetDlgItem(hwndDlg, IDC_IPADDRESS1);
		EnableWindow(ipaddr, false);
		HWND port = GetDlgItem(hwndDlg, IDC_EDIT1);
		SetFocus(port);
		return TRUE;
	}
	case WM_PAINT:
		PAINTSTRUCT ps;
		BeginPaint(hwndDlg, &ps);
		EndPaint(hwndDlg, &ps);
		return TRUE;

	//case WM_QUIT:
	//	EndDialog(hwndDlg, IDOK);
	//	break;
	case WM_DESTROY:
		EndDialog(hwndDlg, 1);
		PostQuitMessage(0);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_RADIO1:
		{
			HWND ipaddr = GetDlgItem(hwndDlg, IDC_IPADDRESS1);
			HWND port = GetDlgItem(hwndDlg, IDC_EDIT1);
			EnableWindow(ipaddr, false);
			SetFocus(port);
		}
			break;
		case IDC_RADIO2:
		{
			HWND ipaddr = GetDlgItem(hwndDlg, IDC_IPADDRESS1);
			EnableWindow(ipaddr, true);
			SetFocus(ipaddr);
			break;
		}
		case IDOK:
			EndDialog(hwndDlg, IDOK);
			PostQuitMessage(0);
			break;
		case IDCANCEL:
			EndDialog(hwndDlg, IDCANCEL);
			break;
		}
		break;
	default:
		return 0;
	}
	return 0;
}
*/
CThreadedDlg::CThreadedDlg() {};
CThreadedDlg::~CThreadedDlg() {
	/*
	m_pActiveWnd->Detach();
	m_pActiveWnd->DestroyWindow();
	delete m_pActiveWnd;
	m_pFilter->m_ThrdDlgDone = true;*/

	//if(m_pDlg) m_pDlg->DestroyWindow();
	//m_pFilter->m_ThrdDlgDone = true;
	//DestroyWindow(m_hwnd);
}

void CThreadedDlg::Setup(CustomRTCTransceiver* filter, UINT nIDTemplate, int nCmdShow /*=SW_SHOW*/) {
	m_nIDTemplate = nIDTemplate;
	m_nCmdShow = nCmdShow;
	m_pFilter = filter;
};
BOOL CThreadedDlg::InitInstance() {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	m_pDlg = new netsetDlg();
	((netsetDlg*)m_pDlg)->m_pFilter = m_pFilter;
	//m_pActiveWnd = m_pDlg;
	m_pDlg->Create(IDD_NETWORKFORMVIEW, 0); //parent is the desktop
	m_pDlg->ShowWindow(m_nCmdShow);
	m_pDlg->UpdateWindow();

	/*
	m_hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(m_nIDTemplate), NULL, cdialogProc);
	m_pActiveWnd = new CWnd;
	m_pActiveWnd->Attach(m_hwnd);
	m_pActiveWnd->ShowWindow(m_nCmdShow);*/
	return TRUE;
}

int CThreadedDlg::ExitInstance() {
	//m_pDlg->DestroyWindow();
	//m_pDlg->EndDialog(true);
	return CWinThread::ExitInstance();
}
