#pragma once

#include "stdafx.h"
#include "network.h"
#include "cstreams.h"	//stripped down version of streams.h to avoid conflict with between cmediasample in cvp8sample and official cmediasample
#include <initguid.h>
#include "webmtypes.h"
#include "odbgstream.h"
#include "rtp.h"
#include "vp8inpin.h"
#include "vp8outpin.h"
#include "resource.h"

#define FILTER_NAME L"Custom RTC Transceiver"

// {49D7B1F6-F4F5-41D7-BA15-C81156CCB43F}
static const GUID CLSID_CustomRTCTransceiver =
{ 0x49d7b1f6, 0xf4f5, 0x41d7,{ 0xba, 0x15, 0xc8, 0x11, 0x56, 0xcc, 0xb4, 0x3f } };

class CustomRTCTransceiver;
class CRTInPin;
class CRTOutPin;
struct pinMT;

//INT_PTR CALLBACK dialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

DWORD WINAPI establishNetwork(LPVOID lpParam);
DWORD WINAPI handshake(LPVOID lpParam);
DWORD WINAPI syncTMT(LPVOID lpParam);
DWORD WINAPI transmit(LPVOID lpParam);
DWORD WINAPI processRaw(LPVOID lpParam);
DWORD WINAPI receive(LPVOID lpParam);
DWORD WINAPI resend(LPVOID lpParam);
DWORD WINAPI distribute(LPVOID lpParam);

struct pinMT
{
	int pinNum;
	AM_MEDIA_TYPE mt;
	VIDEOINFOHEADER vih;
};


class CustomRTCTransceiver : public CCritSec, public CBaseFilter
{
public:
	//Construction / Destruction
	CustomRTCTransceiver(TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr);
	~CustomRTCTransceiver();
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

	//CBaseFilter methods
	int GetPinCount();	// hard coded pin count
	CBasePin* GetPin(int n);

	//Override CBaseFilter methods
	STDMETHODIMP Pause();	// running in background

	int GetInPinCount();	// hard coded pin count
	int GetOutPinCount(int* firstPinOffset);	// hard coded pin count

	bool allTMTSynced(void);
	void GetExtReady();
	net_set_manual m_netMan;
	s_socket m_socket;	// same socket for transmission and reception
	
	ConcurrentQueue<PACKET_UNION*> m_tQueue;	// queue up by packets from each inpin for transmission
	ConcurrentQueue<PACKET_UNION*> m_rQueue;	// queue up by packets for each outpin for reception
	ConcurrentQueue<c8*> m_rrawQueue;	// queue up by raw packets for reception
	bool m_quit;
	HANDLE m_hQuit;
	int rtt;	// round trip time in millisecond
	std::atomic_flag m_syncingTMT;
	std::atomic_flag m_resendFlag;
	//CDialog m_dlg;
	void* m_pThrdDlg;
	//void* m_pResend;
	HANDLE m_resend;
	HANDLE m_receive;
	HANDLE m_process;
	HANDLE m_distribute;
	HANDLE m_resendReq;
	HANDLE m_hIncoming;
	HANDLE m_hOutgoing;
	PACKET_RESEND pr;
	bool m_extReady;
	//bool m_ThrdDlgDone;
	//wchar_t* form;
private:
	CRTInPin** m_inPin;	// Declare input pins.
	CRTOutPin** m_outPin;	// Declare output Pins.
	HANDLE m_transmit;
	HANDLE m_estbNetwork;
};