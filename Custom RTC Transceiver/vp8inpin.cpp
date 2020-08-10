#include "vp8inpin.h"
#include "cvp8sample.h"

CRTInPin::CRTInPin(TCHAR *pName, CustomRTCTransceiver *pCustomRTCTransceiver, HRESULT *phr, LPCWSTR pPinName, int pinNum) :
	CBaseInputPin(pName, pCustomRTCTransceiver, pCustomRTCTransceiver, phr, pPinName),
	m_pinNum(pinNum),
	m_mtSynced(false)
{
	ASSERT(phr);
	m_pCustomRTCTransceiver = pCustomRTCTransceiver;
	CreatePacketizer(VIDEO_PACKET, 5, pinNum);
}

CRTInPin::~CRTInPin()
{
	DestroyMT(&m_mt);	//in case we got allocate memory
}

HRESULT CRTInPin::Receive(IMediaSample* pSample)
{
	_COM_SMARTPTR_TYPEDEF(IVP8Sample, __uuidof(IVP8Sample));

	const IVP8SamplePtr p(pSample);

	IVP8Sample::Frame& f = p->GetFrame();
	long packetNeeded = f.buflen / PACKET_DATA_SIZE;	//let's say we needed 2 but non double operation returns 1. we are not doing rounding up here with floating point.
	if (PS - ((Packetizer::p.add_ptr - Packetizer::p.send_ptr) & PSM) > packetNeeded)	//available space more than actual required, we have '>' here, so, let's say just nice available is 2, and 2 > 1, true, if available is 1, 1 > 1, false.
	{
		unsigned char frame_type = ((f.key) ? KEY : NORMAL);
		bool discontinuity = pSample->IsDiscontinuity() == S_OK;
		//Let's packetize
		Packetize(f.start / 10000, (unsigned char*)f.buf, f.len, frame_type, discontinuity, &m_pCustomRTCTransceiver->m_tQueue);
		SetEvent(m_pCustomRTCTransceiver->m_hOutgoing);
	}
	return S_OK;
}

HRESULT CRTInPin::CheckMediaType(const CMediaType *pmt)
{
	if (pmt == 0)
		return E_INVALIDARG;

	const AM_MEDIA_TYPE& mt = *(static_cast<const AM_MEDIA_TYPE*>(pmt));

	if (mt.majortype != MEDIATYPE_Video)
		return S_FALSE;

	if (mt.subtype != WebmTypes::MEDIASUBTYPE_VP80)
		return S_FALSE;

	if (mt.formattype != FORMAT_VideoInfo)  // TODO: liberalize
		return S_FALSE;

	if (mt.pbFormat == 0)
		return S_FALSE;

	if (mt.cbFormat < sizeof(VIDEOINFOHEADER))
		return S_FALSE;

	const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
	const BITMAPINFOHEADER& bmih = vih.bmiHeader;

	if (bmih.biSize != sizeof(BITMAPINFOHEADER))  // TODO: liberalize
		return S_FALSE;

	if (bmih.biWidth <= 0)
		return S_FALSE;

	if (bmih.biHeight <= 0)
		return S_FALSE;

	if (bmih.biCompression != WebmTypes::MEDIASUBTYPE_VP80.Data1)  //"VP80"
		return S_FALSE;

	CopyMT(&m_mt, mt);
	while (m_pCustomRTCTransceiver->m_syncingTMT.test_and_set(std::memory_order_relaxed));  // acquire lock
	m_mtSynced = false;
	if (m_pCustomRTCTransceiver->m_socket.state & CONNECTED)
	{
		CreateThread(NULL, 0, syncTMT, m_pCustomRTCTransceiver, 0, NULL);
	}
	else
	{
		m_pCustomRTCTransceiver->m_syncingTMT.clear(std::memory_order_relaxed);
	}
	return S_OK;
}

HRESULT CRTInPin::CopyMT(AM_MEDIA_TYPE* tgt, const AM_MEDIA_TYPE& src)
{
	*tgt = src;

	if (src.cbFormat == 0)
	{
		tgt->pbFormat = 0;
		return S_OK;
	}

	tgt->pbFormat = (BYTE*)CoTaskMemAlloc(src.cbFormat);

	if (tgt->pbFormat == 0)
	{
		tgt->cbFormat = 0;
		return E_OUTOFMEMORY;
	}

	memcpy(tgt->pbFormat, src.pbFormat, src.cbFormat);

	return S_OK;
}


void CRTInPin::DestroyMT(AM_MEDIA_TYPE* tgt)
{
	CoTaskMemFree(tgt->pbFormat);

	tgt->pbFormat = 0;
	tgt->cbFormat = 0;
}