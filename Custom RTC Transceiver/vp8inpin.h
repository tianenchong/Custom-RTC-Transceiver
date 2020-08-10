#pragma once

#include "Filter.h"
#include "cstreams.h"
#include <initguid.h>
#include "webmtypes.h"
#include "odbgstream.h"
#include "rtp.h"
#include "comdef.h"

class CustomRTCTransceiver;

//not using memory allocator here, it is provided by upstream filter

class CRTInPin : public CBaseInputPin, public Packetizer
{
public:

	// Constructor and destructor
	CRTInPin(TCHAR *pName, CustomRTCTransceiver *pCustomRTCTransceiver, HRESULT *phr, LPCWSTR pPinName, int pinNum);
	~CRTInPin();

	HRESULT STDMETHODCALLTYPE Receive(IMediaSample* pSample);

	//CBaseInputPin Methods
	HRESULT CheckMediaType(const CMediaType *pmt);

	HRESULT CopyMT(AM_MEDIA_TYPE* tgt, const AM_MEDIA_TYPE& src);
	void DestroyMT(AM_MEDIA_TYPE* tgt);
	AM_MEDIA_TYPE* GetMediaType() { return &m_mt; };
	int GetPinNum() { return m_pinNum; };

	CustomRTCTransceiver* m_pCustomRTCTransceiver;	//Main filter object
	bool m_mtSynced;
private:
	int m_pinNum;	//Pin Number
	AM_MEDIA_TYPE m_mt;
};