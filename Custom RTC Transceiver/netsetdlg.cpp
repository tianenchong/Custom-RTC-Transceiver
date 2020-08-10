#include "netsetDlg.h"
#include "resource.h"


netsetDlg::netsetDlg()
{
}


netsetDlg::~netsetDlg()
{
}

void netsetDlg::PostNcDestroy()
{
	delete this;
}

BEGIN_MESSAGE_MAP(netsetDlg, CDialog)
	ON_COMMAND(IDOK, &netsetDlg::OnIdok)
	ON_COMMAND(IDC_LOCAL_RADIO1, &netsetDlg::OnLocalRadio1)
	ON_COMMAND(IDC_LOCAL_RADIO2, &netsetDlg::OnLocalRadio2)
	ON_COMMAND(IDC_EXT_RADIO1, &netsetDlg::OnExtRadio1)
	ON_COMMAND(IDC_EXT_RADIO2, &netsetDlg::OnExtRadio2)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


void netsetDlg::OnIdok()
{
	//data exchange
	UpdateData(TRUE);
	PostMessage(WM_CLOSE);
}

void netsetDlg::OnLocalRadio1()
{
	// TODO: Add your command handler code here
	CWnd* ipLocalAddr = this->GetDlgItem(IDC_LOCAL_IPADDRESS);
	CWnd* ipExtAddr = this->GetDlgItem(IDC_EXT_IPADDRESS);
	CWnd* port = this->GetDlgItem(IDC_LOCAL_PORT);
	CheckRadioButton(IDC_EXT_RADIO1, IDC_EXT_RADIO2, IDC_EXT_RADIO1);
	ipLocalAddr->EnableWindow(false);
	ipExtAddr->EnableWindow(false);
	port->SetFocus();
}


void netsetDlg::OnLocalRadio2()
{
	// TODO: Add your command handler code here
	CWnd* ipLocalAddr = GetDlgItem(IDC_LOCAL_IPADDRESS);
	CWnd* ipExtAddr = GetDlgItem(IDC_EXT_IPADDRESS);
	CheckRadioButton(IDC_EXT_RADIO1, IDC_EXT_RADIO2, IDC_EXT_RADIO2);
	ipLocalAddr->EnableWindow(true);
	ipExtAddr->EnableWindow(true);
	ipLocalAddr->SetFocus();
}

void netsetDlg::OnExtRadio1()
{
	// TODO: Add your command handler code here
	CWnd* ipLocalAddr = GetDlgItem(IDC_LOCAL_IPADDRESS);
	CWnd* ipExtAddr = GetDlgItem(IDC_EXT_IPADDRESS);
	CWnd* port = GetDlgItem(IDC_EXT_PORT);
	CheckRadioButton(IDC_LOCAL_RADIO1, IDC_LOCAL_RADIO2, IDC_LOCAL_RADIO1);
	ipLocalAddr->EnableWindow(false);
	ipExtAddr->EnableWindow(false);
	port->SetFocus();
}


void netsetDlg::OnExtRadio2()
{
	// TODO: Add your command handler code here
	CWnd* ipLocalAddr = GetDlgItem(IDC_LOCAL_IPADDRESS);
	CWnd* ipExtAddr = GetDlgItem(IDC_EXT_IPADDRESS);
	CheckRadioButton(IDC_LOCAL_RADIO1, IDC_LOCAL_RADIO2, IDC_LOCAL_RADIO2);
	ipLocalAddr->EnableWindow(true);
	ipExtAddr->EnableWindow(true);
	ipExtAddr->SetFocus();
}


BOOL netsetDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  Add extra initialization here
	CheckRadioButton(IDC_LOCAL_RADIO1, IDC_LOCAL_RADIO2, IDC_LOCAL_RADIO1);
	CheckRadioButton(IDC_EXT_RADIO1, IDC_EXT_RADIO2, IDC_EXT_RADIO1);
	CWnd* ipLocalAddr = GetDlgItem(IDC_LOCAL_IPADDRESS);
	CWnd* ipExtAddr = GetDlgItem(IDC_EXT_IPADDRESS);
	CWnd* port = GetDlgItem(IDC_LOCAL_PORT);
	ipLocalAddr->EnableWindow(false);
	ipExtAddr->EnableWindow(false);
	port->SetFocus();

	return false;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}


void netsetDlg::OnClose()
{
	DestroyWindow();
}


void netsetDlg::OnDestroy()
{
	PostQuitMessage(0);
}


void netsetDlg::DoDataExchange(CDataExchange* pDX)
{
	// TODO: Add your specialized code here and/or call the base class
	//CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_LOCAL_IPADDRESS, m_pFilter->m_netMan.local_ipaddr_str);
	DDX_Text(pDX, IDC_EXT_IPADDRESS, m_pFilter->m_netMan.ext_ipaddr_str);
	DDX_Text(pDX, IDC_LOCAL_PORT, m_pFilter->m_netMan.local_port);
	DDX_Text(pDX, IDC_EXT_PORT, m_pFilter->m_netMan.ext_port);
	int localRadio = 0;
	int extRadio = 0;
	DDX_Radio(pDX, IDC_LOCAL_RADIO1, localRadio);
	DDX_Radio(pDX, IDC_EXT_RADIO1, extRadio);

	if (localRadio == 0 || extRadio == 0)
	{
		char* to = "localhost";
		char* from = "localhost";
		m_pFilter->m_netMan.local_ipaddr_str = "localhost";
		m_pFilter->m_netMan.ext_ipaddr_str = "localhost";

		net_get_addr_info(from, m_pFilter->m_netMan.local_port, IPv4, UDP, &m_pFilter->m_netMan.local_ipaddr);
		net_get_addr_info(to, m_pFilter->m_netMan.ext_port, IPv4, UDP, &m_pFilter->m_netMan.ext_ipaddr);
	}
	else
	{
		const size_t newfromsizew = (m_pFilter->m_netMan.local_ipaddr_str.GetLength() + 1) * 2;
		char* from = new char[newfromsizew];
		size_t convertedfromCharsw = 0;
		wcstombs_s(&convertedfromCharsw, from, newfromsizew, m_pFilter->m_netMan.local_ipaddr_str, _TRUNCATE);

		const size_t newtosizew = (m_pFilter->m_netMan.ext_ipaddr_str.GetLength() + 1) * 2;
		char* to = new char[newtosizew];
		size_t convertedtoCharsw = 0;
		wcstombs_s(&convertedtoCharsw, to, newtosizew, m_pFilter->m_netMan.ext_ipaddr_str, _TRUNCATE);

		net_get_addr_info(from, m_pFilter->m_netMan.local_port, IPv4, UDP, &m_pFilter->m_netMan.local_ipaddr);
		net_get_addr_info(to, m_pFilter->m_netMan.ext_port, IPv4, UDP, &m_pFilter->m_netMan.ext_ipaddr);
		
		delete[] from;
		delete[] to;
	}
}
