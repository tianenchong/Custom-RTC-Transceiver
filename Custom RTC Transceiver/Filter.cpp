#include "filter.h"
#include "messagemap.h"
#include "CThreadedDlg.h"
#include "netsetdlg.h"
#include "vp8decoderpin.h"

extern HINSTANCE g_hInst;

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
	&MEDIATYPE_Video,       // Major type
	&WebmTypes::MEDIASUBTYPE_VP80     // Minor type
};

const AMOVIESETUP_PIN psudPins[] =
{
	{ L"Input 0",             // Pin's string name
	FALSE,                // Is it rendered
	FALSE,                // Is it an output
	FALSE,                // Allowed none
	TRUE,                // Allowed many
	&CLSID_NULL,          // Connects to filter
	NULL,				// Connects to pin
	1,                   // Number of types
	&sudPinTypes },      // Pin information
	{ L"Input 1",             // Pin's string name
	FALSE,                // Is it rendered
	FALSE,                // Is it an output
	FALSE,                // Allowed none
	TRUE,                // Allowed many
	&CLSID_NULL,          // Connects to filter
	NULL,				// Connects to pin
	1,                   // Number of types
	&sudPinTypes },      // Pin information
	{ L"Output 0",             // Pin's string name
	FALSE,                // Is it rendered
	TRUE,                // Is it an output
	FALSE,                // Allowed none
	TRUE,                // Allowed many
	&CLSID_NULL,        // Connects to filter
	NULL,				// Connects to pin
	1,                   // Number of types
	&sudPinTypes },      // Pin information
	{ L"Output 1",             // Pin's string name
	FALSE,                // Is it rendered
	TRUE,                // Is it an output
	FALSE,                // Allowed none
	TRUE,                // Allowed many
	&CLSID_NULL,          // Connects to filter
	NULL,				// Connects to pin
	1,                   // Number of types
	&sudPinTypes }      // Pin information
};
const AMOVIESETUP_FILTER sudBallax =
{
	&CLSID_CustomRTCTransceiver,    // Filter CLSID
	FILTER_NAME,            // String name
	MERIT_DO_NOT_USE,       // Filter merit
	4,                      // Number pins
	psudPins               // Pin details
};

CFactoryTemplate g_Templates[] =
{
	{
		FILTER_NAME,
		&CLSID_CustomRTCTransceiver,
		CustomRTCTransceiver::CreateInstance,
		NULL,
		&sudBallax
	}
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2(FALSE);
}

//
// DllEntryPoint
//

//extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

//BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
//extern "C"
//BOOL WINAPI filterDllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
//{
//	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
//}

// Create a new instance of this class
CUnknown * WINAPI CustomRTCTransceiver::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	ASSERT(phr);

	//  DLLEntry does the right thing with the return code and
	//  the returned value on failure
	return new CustomRTCTransceiver(FILTER_NAME, pUnk, phr);
}

CustomRTCTransceiver::CustomRTCTransceiver(TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr) :
	m_transmit(0),
	m_receive(0),
	m_process(0),
	m_estbNetwork(0),
	//m_distribute(0),
	m_pThrdDlg(0),
	m_quit(false),
	m_extReady(false),
	CBaseFilter(
		pName,	// Object description
		pUnk,	// IUnknown of delegating object
		this,	// Object who maintains lock
		CLSID_CustomRTCTransceiver)	// The clsid to be used to serialize this filter
{
	
	bool ret = SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	assert(ret);
	m_syncingTMT.clear(std::memory_order_relaxed);
	m_resendFlag.clear(std::memory_order_relaxed);
	m_hQuit = CreateEvent(NULL, NULL, NULL, NULL);
	m_hIncoming = CreateEvent(NULL, NULL, NULL, NULL);
	m_hOutgoing = CreateEvent(NULL, NULL, NULL, NULL);
	m_resendReq = CreateEvent(NULL, NULL, NULL, NULL);
	m_inPin = NULL;
	m_inPin = new CRTInPin*[2];
	m_inPin[0] = NULL;
	m_inPin[0] = new CRTInPin(NAME("Input Pin"), this, phr, L"Input 0", 0);
	m_inPin[1] = NULL;
	m_inPin[1] = new CRTInPin(NAME("Input Pin"), this, phr, L"Input 1", 1);
	m_outPin = NULL;
	m_outPin = new CRTOutPin*[2];
	m_outPin[0] = NULL;
	m_outPin[0] = new CRTOutPin(NAME("Output Pin"), this, phr, L"Output 0", 0);
	m_outPin[1] = NULL;
	m_outPin[1] = new CRTOutPin(NAME("Output Pin"), this, phr, L"Output 1", 1);
	//form = MAKEINTRESOURCE(IDD_NETWORKFORMVIEW);
	//inst = &g_hInst;
	//AfxSetResourceHandle(g_hInst);
	//HWND nsdb = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_FORMVIEWTEST), NULL, dialogProc);
	//HWND nsdb = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FORMVIEWTEST), NULL, dialogProc, NULL);
	//DWORD err = GetLastError();
	//ShowWindow(nsdb, SW_SHOW);
	HRESULT hr = net_init();
	if (SUCCEEDED(hr))
	{
		//AfxBeginThread(establishNetwork, this);
		m_estbNetwork = CreateThread(NULL, 0, establishNetwork, this, 0, NULL);
	}
}

CustomRTCTransceiver::~CustomRTCTransceiver()
{
	// Send quit signal
	m_quit = true;
	SetEvent(m_hQuit);
	//m_pResend->ExitInstance();
	// Ensure all threads have ended

	// could have quit early so need special treatment
	CThreadedDlg* threadDlg = (CThreadedDlg*)m_pThrdDlg;
	//Resend* resend = (Resend*)m_pResend;
	/*
	if(threadDlg)
	{
		DWORD dwEvent = WaitForSingleObject(threadDlg->m_hThread, 10000);
		assert(dwEvent == WAIT_OBJECT_0);
		delete threadDlg;
	}*/

	// might have started or not started at all, but once started all must end together at the same time
	//HANDLE handles[] = { m_transmit, m_receive, m_process, m_estbNetwork, m_distribute, threadDlg->m_hThread, m_resend};
	HANDLE handles[] = { m_transmit, m_receive, m_process, m_estbNetwork, threadDlg->m_hThread, m_resend };
	const char thread_num = (sizeof(handles) / sizeof(*handles));
	HANDLE waits[thread_num];
	char total_wait = 0;
	for (char i = 0; i < thread_num; i++)
	{
		if (handles[i])
		{
			waits[total_wait] = handles[i];
			total_wait++;
		}
	}

	if (total_wait > 0)
	{
		DWORD dwEvent = WaitForMultipleObjects(total_wait, waits, true, 10000);
		assert(dwEvent >= WAIT_OBJECT_0 && dwEvent < (WAIT_OBJECT_0 + total_wait));
	}

	if (threadDlg) 
		delete threadDlg;

	// Clean up
	net_cleanup();
	delete m_inPin[0];
	m_inPin[0] = NULL;
	delete m_inPin[1];
	m_inPin[1] = NULL;
	delete[] m_inPin;
	m_inPin = NULL;
	delete m_outPin[0];
	m_outPin[0] = NULL;
	delete m_outPin[1];
	m_outPin[1] = NULL;
	delete[] m_outPin;
	m_outPin = NULL;
}

int CustomRTCTransceiver::GetPinCount()
{
	return 4;
}

CBasePin* CustomRTCTransceiver::GetPin(int n)
{
	if (n == 0 || n == 1)
		return m_inPin[n];
	else if (n == 2 || n == 3)
	{
		return m_outPin[n-2];
	}
	else
		return NULL;
}

STDMETHODIMP CustomRTCTransceiver::Pause()
{
	CAutoLock cObjectLock(m_pLock);

	// notify all pins of the change to active state
	if (m_State == State_Stopped) {
		//if (!((m_socket.state & CONNECTED) && allTMTSynced()))
		//	return E_FAIL;
		//else
		//{

		//}

		int cPins = GetPinCount();
		for (int c = 0; c < cPins; c++) {

			CBasePin *pPin = GetPin(c);
			if (NULL == pPin) {
				break;
			}

			// Disconnected pins are not activated - this saves pins
			// worrying about this state themselves

			if (pPin->IsConnected()) {
				HRESULT hr = pPin->Active();
				if (FAILED(hr)) {
					return hr;
				}
			}
		}
		GetExtReady();
		if (!m_transmit)
		{
			m_transmit = CreateThread(NULL, 0, transmit, this, 0, NULL);
		}
	}


#ifdef DXMPERF
	PERFLOG_PAUSE(m_pName ? m_pName : L"CBaseFilter", (IBaseFilter *) this, m_State);
#endif // DXMPERF

	m_State = State_Paused;
	return S_OK;
}

int CustomRTCTransceiver::GetInPinCount()
{
	return 2;
}

int CustomRTCTransceiver::GetOutPinCount(int * firstPinOffset)
{
	*firstPinOffset = 2;
	return 2;
}

bool CustomRTCTransceiver::allTMTSynced(void)
{
	for (int i = 0; i < GetInPinCount(); i++)
	{
		CBasePin* pin = GetPin(i);
		if (pin->IsConnected())
		{
			CRTInPin* rtpin = (CRTInPin*)pin;
			if (!rtpin->m_mtSynced)
				return false;
		}
	}
	return true;
}
void CustomRTCTransceiver::GetExtReady()
{
	while (!m_extReady && !m_quit)	//initiator side
	{
		char gr_packet[] = "get ready";
		int bytes_sent;
#ifdef _DEBUG
		wodbgstream os;
		char addr[15];
		inet_ntop(AF_INET, &m_socket.remote.in_addr[HOST].sin_addr, addr, sizeof(addr)), // IPv6/IPv4
			os << FILTER_NAME << "::" << addr << ":" << ntohs(m_socket.remote.in_addr[HOST].sin_port) << " >> " << "Get Ready..." << std::endl;
#endif // _DEBUG
		net_sendto(&m_socket, (c8 *)&gr_packet, strlen(gr_packet) + 1, &bytes_sent, m_socket.remote.in_addr[HOST]);
		Sleep(1000);
	}
}
/*
INT_PTR CALLBACK dialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
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
			SetFocus(ipaddr);
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
}*/

DWORD WINAPI establishNetwork(LPVOID lpParam)
{
	//AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	HRESULT hr = S_OK;
	do
	{
		//AFX_MANAGE_STATE(AfxGetStaticModuleState());
		//HINSTANCE hOldRes = AfxGetResourceHandle();
		//AfxSetResourceHandle(g_hInst);
		//addrDlg* dlg = new addrDlg();
		//HMODULE hModule = GetModuleHandle(L"Custom RTC Transmitter.dll");
		//bool ret = dlg->Create(MAKEINTRESOURCE(IDD_FORMVIEW), NULL);
		//dlg->ShowWindow(SW_SHOW);
		//AfxSetResourceHandle(hOldRes);
		//dlg->P
		//AfxSetResourceHandle(g_hInst);
		//HWND nsdb = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_NETWORKFORMVIEW), NULL, dialogProc);
		//HWND nsdb = CreateDialogParam(ACAPI_GetExtensionInstance(), MAKEINTRESOURCE(IDD_NETWORKFORMVIEW), NULL, dialogProc, NULL);
		//DWORD err = GetLastError();
		//ShowWindow(nsdb, SW_SHOW);
		//UpdateWindow(nsdb);
		//CDialog *BadFileD = new CDialog();
		//BadFileD->Create(IDD_FORMVIEWTEST, NULL);
		//BadFileD->DoModal();
		//BadFileD->ShowWindow(SW_SHOW);
		//BadFileD->UpdateWindow();
		//return 1;

		filter->m_pThrdDlg = (CThreadedDlg*)AfxBeginThread(RUNTIME_CLASS(CThreadedDlg),
				0, 0, CREATE_SUSPENDED);
		CThreadedDlg* dlg = (CThreadedDlg*)filter->m_pThrdDlg;
		dlg->m_bAutoDelete = false;
		dlg->Setup(filter, IDD_NETWORKFORMVIEW, SW_SHOW);
		dlg->ResumeThread();
		
		HANDLE handles[] = { dlg->m_hThread, filter->m_hQuit };
		if (dlg->m_hThread && filter->m_hQuit)
		{
			DWORD dwEvent = WaitForMultipleObjects(2, handles, false, INFINITE);	//	wait forever
			assert(dwEvent >= WAIT_OBJECT_0 && dwEvent < (WAIT_OBJECT_0 + 2));
			if (dwEvent == (WAIT_OBJECT_0 + 1))
			{
				//dlg->m_pDlg->EndDialog(1);
				dlg->m_pDlg->PostMessage(WM_CLOSE);
				//dlg->m_pDlg->DestroyWindow();
				//dlg->ExitInstance();
				//dlg->PostQuitMessage(0);
				return 0;
			}
		}
		//return 1;
		//DWORD dwEvent = WaitForSingleObject(dlg->m_hThread, 5000);	// not going to wait long after asking it to quit
		//assert(dwEvent == WAIT_OBJECT_0);	// assert it quit nicely, not timeout

		//filter->m_pThrdDlg = (void*)mp_ThrdDlg;
		//mp_ThrdDlg->InitInstance();
		//mp_ThrdDlg->ExitInstance();
		//DWORD ExitCode;
		//GetExitCodeThread(mp_ThrdDlg->m_hThread, &ExitCode);
		//AfxEndThread(ExitCode);
		//::TerminateThread(mp_ThrdDlg->m_hThread, 1);
		//WaitForSingleObject(mp_ThrdDlg->m_hThread, INFINITE);
		//filter->m_dlg.Create(IDD_NETWORKFORMVIEW, 0); //parent is the desktop
		//filter->m_dlg.ShowWindow(SW_SHOW);
		//filter->m_dlg.DestroyWindow();


		hr = net_open(&(filter->m_socket), IPv4, UDP);
		BREAK_ON_FAIL(hr);

		//hr = net_bind(&(filter->m_socket), &filter->m_netMan.local_ipaddr, filter->m_netMan.local_port);
		hr = net_bind(&(filter->m_socket), &filter->m_netMan.local_ipaddr, filter->m_netMan.local_port);
		BREAK_ON_FAIL(hr);

		//assert(filter->m_socket.local.in_addr[HOST].sin_addr.s_addr == htonl(INADDR_ANY));

		hr = net_set_read_timeout(&(filter->m_socket), 3000);	//every listening will last for 3 sec until timeout (must be less than 10 sec of the destructor wait)
		BREAK_ON_FAIL(hr);

		hr = net_set_send_timeout(&(filter->m_socket), 0);	//non blocking, don't care if we actually sent something successfully and return immediately
		BREAK_ON_FAIL(hr);

		//Start a new thread to receive
		HANDLE handshake_handle = CreateThread(NULL, 0, handshake, filter, 0, NULL);

		char init_packet[] = "initiate call";
		char accept_packet[] = "call accepted";
		int bytes_sent;

		while (filter->m_socket.phase == SILENT)	//initiator side
		{
			if (filter->m_quit)
				return true;
#ifdef _DEBUG
			wodbgstream os;
			char addr[15];
			inet_ntop(AF_INET, &filter->m_netMan.ext_ipaddr.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
			os << FILTER_NAME << "::" << addr << ":" << ntohs(filter->m_netMan.ext_ipaddr.sin_port) << " >> " << "Initiating call..." << std::endl;
#endif // _DEBUG
			net_sendto(&(filter->m_socket), (c8 *)&init_packet, strlen(init_packet) + 1, &bytes_sent, filter->m_netMan.ext_ipaddr);
			Sleep(1000);
		}

		while (filter->m_socket.phase == RINGING)	//receiver side
		{
			if (filter->m_quit)
				return true;
#ifdef _DEBUG
			wodbgstream os;
			char addr[15];
			inet_ntop(AF_INET, &filter->m_socket.remote.in_addr[HOST].sin_addr, addr, sizeof(addr)), // IPv6/IPv4
			os << FILTER_NAME << "::" << addr << ":" << ntohs(filter->m_socket.remote.in_addr[HOST].sin_port) << " >> " << "call accepted" << std::endl;
#endif // _DEBUG
net_sendto(&(filter->m_socket), (c8 *)&accept_packet, strlen(accept_packet) + 1, &bytes_sent, filter->m_socket.remote.in_addr[HOST]);
			Sleep(1000);
		}

		while (!(filter->m_socket.state & CONNECTED) && !filter->m_quit);	//initiator side

		if (filter->m_quit)
			return true;

		if (!filter->allTMTSynced())	//not all sync(only those that are connected)
		{
			/*
			for (int i = 0; i < filter->GetPinCount(); i++)
			{
			CBasePin* pin = filter->GetPin(i);
			CRTInPin* rtpin = (CRTInPin*)pin;
			if (pin->IsConnected() && rtpin->m_mtSynced == 0)	//find out which pin is already connected but still hasn't started handle for syncing media type
			CreateThread(NULL, 0, syncTMT, rtpin, 0, NULL);	//all handle will terminate eventually when such pin is synced. Note: this handle can receive sync confirmation of not such pin, but the other pins, and vice versa but such pin will still aware when other pins received its confirmation.
			}*/
			while (filter->m_syncingTMT.test_and_set(std::memory_order_relaxed));  // acquire lock
			CreateThread(NULL, 0, syncTMT, filter, 0, NULL);	// no infinite loop, so no need to wait for it to end
		}
		filter->m_receive = CreateThread(NULL, 0, receive, filter, 0, NULL);
		filter->m_process = CreateThread(NULL, 0, processRaw, filter, 0, NULL);
		//filter->m_distribute = CreateThread(NULL, 0, distribute, filter, 0, NULL);
		filter->m_resend = CreateThread(NULL, 0, resend, filter, 0, NULL);
	} while (FALSE);

	if (SUCCEEDED(hr))
		return true;
	else
	{
		net_cleanup();
		return false;
	}
}

DWORD WINAPI handshake(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	c8 one_packet[8000];
	int bytes_read;
	sockaddr_in sa_from;
	clock_t start, end;

	while (!(filter->m_socket.state & CONNECTED))
	{
		hr = net_recvfrom(&(filter->m_socket), one_packet, sizeof(one_packet), &bytes_read, &sa_from);

		if (SUCCEEDED(hr) && bytes_read > 0)
		{
#ifdef _DEBUG
			wodbgstream os;
			char* data = new char[bytes_read + 1];
			strncpy_s(data, bytes_read + 1, (const char*)&one_packet, bytes_read);
			data[bytes_read] = '\0';
			char addr[15];
			inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
			//assert(sa_from.sin_port == filter->m_socket.remote.in_addr[HOST].sin_port);
			//assert(sa_from.sin_port != filter->m_socket.local.in_addr[HOST].sin_port);
			//os << " Our filter is " << FILTER_NAME << "::" << ntohs(filter->m_socket.local.in_addr[HOST].sin_port) << std::endl;
			os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " << " << data << std::endl;
			delete[] data;
#endif // _DEBUG
			if (strncmp(one_packet, "call accepted", 13) == 0 && !(filter->m_socket.state & CONNECTED))	//initiator side
			{
				filter->m_socket.phase = ACCEPTED;
				char confirm_packet[] = "call confirmed";
				int bytes_sent;
				net_sendto(&(filter->m_socket), (c8 *)&confirm_packet, strlen(confirm_packet) + 1, &bytes_sent, sa_from);
				//Round trip time/ping test
				char ping_packet[] = "ping";
				start = clock();
				net_sendto(&(filter->m_socket), (c8 *)&ping_packet, strlen(ping_packet) + 1, &bytes_sent, sa_from);
#ifdef _DEBUG
				char addr[15];
				inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
				os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " >> " << "ping" << std::endl;
				os << FILTER_NAME << "::" << "Connected to another party." << std::endl;
#endif // _DEBUG
			}
			else if (strncmp(one_packet, "initiate call", 13) == 0)	//receiver side
			{
				filter->m_socket.phase = RINGING;
			}
			else if (strncmp(one_packet, "call confirmed", 14) == 0)	//receiver side
			{
				filter->m_socket.phase = ACCEPTED;
				int bytes_sent;
				//Round trip time/ping test
				char ping_packet[] = "ping";
				start = clock();
				net_sendto(&(filter->m_socket), (c8 *)&ping_packet, strlen(ping_packet) + 1, &bytes_sent, sa_from);
#ifdef _DEBUG
				char addr[15];
				inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
				os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " >> " << "ping" << std::endl;
				os << FILTER_NAME << "::" << "Connected from another party." << std::endl;
#endif // _DEBUG
			}
			else if (strncmp(one_packet, "ping", 4) == 0)	//receiver side
			{
				char pong_packet[] = "pong";
				int bytes_sent;
				net_sendto(&(filter->m_socket), (c8 *)&pong_packet, strlen(pong_packet) + 1, &bytes_sent, sa_from);
				//filter->m_socket.state |= CONNECTED;
#ifdef _DEBUG
				char addr[15];
				inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
				os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " >> " << "pong" << std::endl;
#endif // _DEBUG
			}
			else if (strncmp(one_packet, "pong", 4) == 0)	//receiver side
			{
				end = clock();
				int rtt = ceil((double)(end - start) / CLOCKS_PER_SEC * 1000);	//rtt in millisecond
				filter->rtt = (rtt > 0) ? rtt : 1;
				//wchar_t str[80];
				//swprintf(str, wcslen(str), L"Round trip time is: %d ms", filter->rtt);
				//MessageBox(NULL, str, NULL, NULL);
				//done
				filter->m_socket.state |= CONNECTED;
#ifdef _DEBUG
				char addr[15];
				inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
				os << FILTER_NAME << "::" << "rtt/ping test done. Connection finalized." << std::endl;
#endif // _DEBUG
			}
		}
	}
	return 1;
}

unsigned _power(unsigned val, unsigned _pow = 0) {
	if (_pow <= 0)
		return 1;
	return val * _power(val, _pow - 1);
};

DWORD WINAPI syncTMT(LPVOID lpParam)
{
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	unsigned char pins = 0;


	for (int i = 0; i < filter->GetInPinCount(); i++)
	{
		CBasePin* pin = filter->GetPin(i);
		CRTInPin* rtpin = (CRTInPin*)pin;
		//assert(i!=0 || (i == 0 && pin->IsConnected() && !rtpin->m_mtSynced));
		if (pin->IsConnected() && !rtpin->m_mtSynced)	//find out which pin is already connected but still hasn't started handle for syncing media type
			pins |= _power((i + 1), 2);
	}
	//assert(pins == 1);
	c8 one_packet[8000];
	int bytes_sent, bytes_read;
	sockaddr_in sa_from;

	if (!filter->allTMTSynced())
	{
		for (int i = 0; i < filter->GetInPinCount(); i++)
		{
			if (!(pins & _power((i + 1), 2)))
				continue;

			CBasePin* cpin = filter->GetPin(i);
			CRTInPin* pin = (CRTInPin*)cpin;
			pinMT pinmt;
			pinmt.pinNum = pin->GetPinNum();
			pinmt.mt = *(pin->GetMediaType());
			pinmt.mt.pbFormat = NULL;
			pinmt.vih = *(VIDEOINFOHEADER*)(pin->GetMediaType()->pbFormat);

			const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(pinmt.vih);
			const BITMAPINFOHEADER& bmih = vih.bmiHeader;

			//send mt
			net_sendto(&(pin->m_pCustomRTCTransceiver->m_socket), (c8*)(&pinmt), sizeof(pinMT), &bytes_sent, pin->m_pCustomRTCTransceiver->m_socket.remote.in_addr[HOST]);
		}
	}
	filter->m_syncingTMT.clear(std::memory_order_release);
	return 0;
}

DWORD WINAPI processRaw(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	c8 one_packet[PACKET_DATA_SIZE];
	int bytes_sent, bytes_read;
	sockaddr_in sa_from = filter->m_socket.remote.in_addr[HOST];
	int offset = 0;
	filter->GetOutPinCount(&offset);	// hard coded pin count
#ifdef _DEBUG
	wodbgstream os;
#endif // _DEBUG
		while (true)
		{
			//HANDLE handles[] = { filter->m_hIncoming, filter->m_hQuit };
			DWORD dwEvent = WaitForSingleObject(filter->m_hIncoming, 1000);
			//if (dwEvent == WAIT_OBJECT_0 + 1)	// quit event
			//{
			//	return 0;
			//}
			//else if (dwEvent == WAIT_TIMEOUT)
			//{
			//	if (filter->m_quit)
			//		return 0;
			//}
			//else
			if(dwEvent == WAIT_OBJECT_0)
			{
				int size = filter->m_rrawQueue.size();
				if (size > 0 && !filter->m_rrawQueue.end_of_queue())
				{
					for (int i = 0; i < size; i++)
					{
						if (filter->m_rrawQueue.front_ready())
						{
							c8* one_packet;
							ULONG len;
							filter->m_rrawQueue.front(&one_packet, &len);

							//process
							if (len == sizeof(PACKET_UNION))	// data/xor packet is the most important item
							{
								//if ((rand() % 100) < 75)
								//{
									//distribute
									PACKET_UNION* pkt = (PACKET_UNION*)one_packet;
									CBasePin* pin = filter->GetPin(pkt->c.ssrc + offset);
									if (pin)
									{
										if (pkt->p.ssrc == 0 || pkt->p.ssrc == 1)
										{
											CRTOutPin* crtpin = (CRTOutPin*)pin;
											crtpin->m_queue.push(pkt, len);
											SetEvent(crtpin->m_packetReady);
#ifdef _DEBUG
											os << "Pushed to outpin queue " << ((pkt->c.type == DATA_PACKET) ? "DATA" : "XOR") << " seq: " << pkt->c.seq << std::endl;
#endif
										}
									}
								//}
								//else
								//{
#ifdef _DEBUG
									//PACKET_UNION* pkt = (PACKET_UNION*)one_packet;
									//os << "Dropped by random " << ((pkt->c.type == DATA_PACKET) ? "DATA" : "XOR") << " seq: " << pkt->c.seq << std::endl;
#endif
								//}
								//if ((rand() % 100) < 75)
								//{
									//filter->m_rQueue.push((PACKET_UNION*)one_packet, sizeof(PACKET_UNION));
								//}

							}
							else if (strncmp(one_packet, "resend", 6) == 0)	// need to get this out of this thread on a new thread
							{
								PACKET_RESEND* resend_pkt = (PACKET_RESEND*)one_packet;
								//while (filter->m_resendFlag.test_and_set(std::memory_order_relaxed));  // acquire lock
								//filter->pr = *resend_pkt;
								//filter->m_resendFlag.clear(std::memory_order_relaxed);
								//SetEvent(filter->m_resendReq);
								if (filter->m_resendFlag.test_and_set(std::memory_order_relaxed) == 0)	// resend thread finished its job
								{
									filter->pr = *resend_pkt;
									SetEvent(filter->m_resendReq);
								}
								// else we ignore the resend request
							}
							else
							{

								int offset = 0;
								int outpin_num = filter->GetOutPinCount(&offset);

								if (len == 6 && (strncmp(one_packet, "ready", 5) == 0))
								{
#ifdef _DEBUG
									char addr[15];
									inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
										os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " << " << one_packet << std::endl;
#endif // _DEBUG
									filter->m_extReady = true;
								}
								else if (len == 10 && (strncmp(one_packet, "get ready", 9) == 0))
								{
									filter->m_extReady = true;	// of course the initiater is already ready. we need to get the downstream decoder ready(set it to pause manually even though the graph manager has not set the filter chain to pause)
									if (!filter->allTMTSynced())	//not all sync(only those that are connected)
									{
										while (filter->m_syncingTMT.test_and_set(std::memory_order_relaxed));  // acquire lock
										HANDLE sync_mt = CreateThread(NULL, 0, syncTMT, filter, 0, NULL);	// no infinite loop, so no need to wait for it to end
										DWORD dwEvent = WaitForSingleObject(sync_mt, 10000);
										assert(dwEvent == WAIT_OBJECT_0);
									}
									for (int i = 2; i < (outpin_num + offset); i++)
									{
										CBasePin* pin = filter->GetPin(i);
										CRTOutPin* rtpin = (CRTOutPin*)pin;
										//assert(i!=0 || (i == 0 && pin->IsConnected() && !rtpin->m_mtSynced));
										if (pin->IsConnected() && rtpin->m_mtSynced)	//find out which pin is already connected but still hasn't started handle for syncing media type
										{
											IPin* pPin = NULL;
											rtpin->ConnectedTo(&pPin);
											IBaseFilter* decoder = (IBaseFilter*)((VP8DecoderLib::Pin*)pPin)->m_pFilter;
											decoder->Pause();
										}
									}
									filter->Pause();
									char ready_packet[] = "ready";
									int bytes_sent;
									net_sendto(&(filter->m_socket), (c8 *)&ready_packet, strlen(ready_packet) + 1, &bytes_sent, sa_from);
#ifdef _DEBUG
									char addr[15];
									inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
										os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " >> " << "ready" << std::endl;
#endif // _DEBUG
								}
								else if (len == sizeof(pinMT))
								{
									pinMT* mt_data = (pinMT*)one_packet;
									CBasePin* pin = filter->GetPin(mt_data->pinNum + offset);
									CRTOutPin* rtpin = (CRTOutPin*)pin;

									if (!(rtpin->m_mtSynced))
									{
										//repackage mt
										AM_MEDIA_TYPE mt;
										mt = mt_data->mt;
										mt.pbFormat = (BYTE*)&(mt_data->vih);
										//set mt
										hr = rtpin->SetPreferredMTV(mt);

										const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*(rtpin->GetPreferredMTV().pbFormat));
										const BITMAPINFOHEADER& bmih = vih.bmiHeader;
										if (SUCCEEDED(hr))
										{
											rtpin->m_mtSynced = true;
											if (!rtpin->m_hSkipManager)
											{
												rtpin->skip_manager();
											}
											if (!rtpin->m_consume)
											{
												rtpin->m_consume = CreateThread(NULL, 0, consume, rtpin, 0, NULL);
											}
											char received_mt[20];
											sprintf_s(received_mt, "%s %d", "received mt", mt_data->pinNum);
											int bytes_sent;
#ifdef _DEBUG
											char addr[15];
											inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
												os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " >> " << "received mt " << mt_data->pinNum << std::endl;
#endif // _DEBUG
											net_sendto(&(filter->m_socket), (c8 *)&received_mt, strlen(received_mt) + 1, &bytes_sent, sa_from);
										}
									}
								}
								else if (len > 0)
								{
#ifdef _DEBUG
									//char* data = new char[len + 1];
									//strncpy_s(data, len + 1, (const char*)&one_packet, len);
									//data[len] = '\0';
									char addr[15];
									inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
										os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " << " << one_packet << std::endl;
									//delete[] data;
#endif // _DEBUG
									if (strncmp(one_packet, "received mt", 11) == 0)
									{
										char num = one_packet[12];
										int pinNum = atoi(&num);
										CBasePin* cpin = filter->GetPin(pinNum);
										if (cpin)
										{
											CRTInPin* pin = (CRTInPin*)cpin;
											pin->m_mtSynced = true;
										}
									}
								}
							}
							//pop
							filter->m_rrawQueue.pop();
						}
					}
				}
			}
			if (filter->m_quit)
				return 0;
		}
	return 0;
}

DWORD WINAPI receive(LPVOID lpParam)
{
	bool ret = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	assert(ret);

	HRESULT hr = S_OK;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	c8 one_packet[8000];
	int bytes_sent, bytes_read;
	sockaddr_in sa_from;
#ifdef _DEBUG
	wodbgstream os;
#endif // _DEBUG

	while (true)
	{
		hr = net_recvfrom(&(filter->m_socket), one_packet, sizeof(one_packet), &bytes_read, &sa_from);
		if (hr == S_OK && bytes_read > 0)
		{
			filter->m_rrawQueue.push(one_packet, bytes_read);
			SetEvent(filter->m_hIncoming);
		}
		if (filter->m_quit)
			break;
	}
	return 0;
}

DWORD WINAPI resend(LPVOID lpParam)
{
	CustomRTCTransceiver* filter = (CustomRTCTransceiver*)lpParam;
	HANDLE waits[] = { filter->m_hQuit, filter->m_resendReq };

	while (true)
	{
		DWORD dwEvent = WaitForMultipleObjects(2, waits, false, INFINITE);
		assert(dwEvent >= WAIT_OBJECT_0 && dwEvent < (WAIT_OBJECT_0 + 2));

		if (dwEvent == WAIT_OBJECT_0)
			return 0;
		else
		{

			//while (filter->m_resendFlag.test_and_set(std::memory_order_relaxed));  // acquire lock
			PACKET_RESEND resend_packet = filter->pr;
			//filter->m_resendFlag.clear(std::memory_order_relaxed);

			int ssrc = resend_packet.ssrc;
			unsigned int* ls = resend_packet.list;
			CRTInPin* pin = (CRTInPin*)(filter->GetPin(ssrc));	// ok for inPin
																	//acquire a lock before accessing packetizer due to different thread
			//while (pin->p.m_inUse.test_and_set(std::memory_order_acquire));
			bool prevState = pin->p.m_inUse.test_and_set(std::memory_order_acquire);
			//assert(prevState == false);
			for (int i = 0; i < resend_packet.count; i++)
			{
				//if (((ls[i] - pin->p.send_ptr) & PSM) <= ((pin->p.add_ptr - pin->p.send_ptr) & PSM))	//make sure packet is already sent, not those just added and not yet sent
				//{
				PACKET_UNION pkt = pin->p.u_packet[ls[i] & PSM];
				filter->m_tQueue.push(&pkt, sizeof(PACKET_UNION));	//add to concurrent queue
				//}
			}
			pin->p.m_inUse.clear(std::memory_order_release);	//release lock
			filter->m_resendFlag.clear(std::memory_order_relaxed);
			SetEvent(filter->m_hOutgoing);
		}
	}
	return 0;
}

DWORD WINAPI transmit(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	int bytes_sent;
	int count = 0;
	sockaddr_in sa_from;

	
		//char gr_packet[] = "get ready";
		//net_sendto(&(filter->m_socket), (c8 *)&gr_packet, strlen(gr_packet), &bytes_sent, filter->m_socket.remote.in_addr[HOST]);

		while (true)
		{
			DWORD dwEvent = WaitForSingleObject(filter->m_hOutgoing, 1000);

			if (dwEvent == WAIT_OBJECT_0)
			{
				int size = filter->m_tQueue.size();
				while (size > 0 && !filter->m_tQueue.end_of_queue())
				{
					for (int i = 0; i < size; i++)
					{
						if (filter->m_tQueue.front_ready())
						{
							PACKET_UNION* data;
							ULONG len;
							filter->m_tQueue.front(&data, &len);
							hr = net_sendto(&(filter->m_socket), (c8*)(data), len, &bytes_sent, filter->m_socket.remote.in_addr[HOST]);
							assert(SUCCEEDED(hr));
							filter->m_tQueue.pop();
						}
					}
				}
			}
			if (filter->m_quit)
			{
#ifdef _DEBUG
				wodbgstream os;
				os << "transmission thread closed" << std::endl;
#endif
				return 0;
			}
		}

	return 0;
}

/*
DWORD WINAPI receive(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	int bytes_sent;
	int count = 0;
	sockaddr_in sa_from;
	c8 one_packet[8000];
	int bytes_read;
#ifdef _DEBUG
	wodbgstream os;
#endif
	do {
		//trying to receive resend request
		HRESULT hr = net_recvfrom(&(filter->m_socket), one_packet, sizeof(one_packet), &bytes_read, &sa_from);

		if (SUCCEEDED(hr) && bytes_read > 0)
		{
#ifdef _DEBUG
			char* data = new char[bytes_read + 1];
			strncpy_s(data, bytes_read + 1, (const char*)&one_packet, bytes_read);
			data[bytes_read] = '\0';
			char addr[15];
			inet_ntop(AF_INET, &sa_from.sin_addr, addr, sizeof(addr)), // IPv6/IPv4
			os << FILTER_NAME << "::" << addr << ":" << ntohs(sa_from.sin_port) << " << " << data << std::endl;
			delete[] data;
#endif // _DEBUG

			if (strncmp(one_packet, "resend", 6) == 0)
			{
				PACKET_RESEND* resend_pkt = (PACKET_RESEND*)&one_packet;
				int ssrc = resend_pkt->ssrc;
				unsigned int* ls = resend_pkt->list;
				CRTInPin* pin = (CRTInPin*)(filter->GetPin(ssrc));

				//acquire a lock before accessing packetizer due to different thread
				while (pin->p.m_inUse.test_and_set(std::memory_order_acquire));
				for (int i = 0; i < resend_pkt->count; i++)
				{
					//if (((ls[i] - pin->p.send_ptr) & PSM) <= ((pin->p.add_ptr - pin->p.send_ptr) & PSM))	//make sure packet is already sent, not those just added and not yet sent
					//{
					PACKET_UNION pkt = pin->p.u_packet[ls[i] & PSM];
					filter->m_tQueue.push(&pkt, sizeof(PACKET_UNION));	//add to concurrent queue
																		//}
				}

				pin->p.m_inUse.clear(std::memory_order_release);	//release lock
			}
		}

		if (filter->m_quit)
		{
			break;
		}

	} while (true);

#ifdef _DEBUG
	os << "listening thread closed" << std::endl;
#endif
	return 0;
}*/

DWORD WINAPI distribute(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)lpParam;
	while (true)
	{
		int size = filter->m_rQueue.size();
		if (size > 0 && !filter->m_rQueue.end_of_queue())
		{
			for (int i = 0; i < size; i++)
			{
				if (filter->m_rQueue.front_ready())
				{
					PACKET_UNION* pkt;
					ULONG len;
					filter->m_rQueue.front(&pkt, &len);
					//distribute
					int offset = 0;
					filter->GetOutPinCount(&offset);	// hard coded pin count
					CBasePin* pin = filter->GetPin(pkt->c.ssrc + offset);
					if (pin)
					{
						if (pkt->p.ssrc == 0 || pkt->p.ssrc == 1)
						{
							CRTOutPin* crtpin = (CRTOutPin*)pin;
							//assert(pkt->p.ssrc == crtpin->GetPinNum());
	#ifdef _DEBUG
							//wodbgstream os;
							//os << "Packet ssrc: " << pkt->p.ssrc << "; target: pin " << crtpin->GetPinNum() << "; seq: " << pkt->c.seq << "; type: " << ((pkt->c.type == DATA_PACKET) ? "data" : "xor") << std::endl;
	#endif
							//if(pkt->c.seq != 17 && pkt->c.seq != 18)
							//if ((rand() % 100) < 75)	// random packet dropping simulation
														//if (pkt->c.seq != 2 && pkt->c.seq != 4 && pkt->c.seq != 17 && pkt->c.seq != 21 && pkt->c.seq != 25 && pkt->c.seq != 28 && pkt->c.seq != 32 && pkt->c.seq != 37 && pkt->c.seq != 40 && pkt->c.seq != 44 && pkt->c.seq != 48 && pkt->c.seq != 52 && pkt->c.seq != 56)
							//{
								//crtpin->ReceivePacket(pkt);
							crtpin->m_queue.push(pkt, len);
							SetEvent(crtpin->m_packetReady);
							//}
							//else
							//{
	#ifdef _DEBUG
								//wodbgstream os;
								//os << "Dropped by random " << ((pkt->c.type == DATA_PACKET) ? "DATA" : "XOR") << " seq: " << pkt->c.seq << std::endl;
	#endif
							//}
						}
					}
					//pop
					filter->m_rQueue.pop();
				}
			}
		}
		if (filter->m_quit) break;
		Sleep(1);
	}
	return 0;
}