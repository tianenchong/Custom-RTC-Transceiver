#include "vp8outpin.h"
#include "cvp8sample.h"
#include "cmediasample.h"


CRTOutPin::CRTOutPin(TCHAR *pName, CustomRTCTransceiver *pCustomRTCTransceiver, HRESULT *phr, LPCWSTR pPinName, int pinNum) :
	CBaseOutputPin(pName, pCustomRTCTransceiver, pCustomRTCTransceiver, phr, pPinName),
	m_mtSynced(false),
	m_hSkipManager(NULL),
	m_consume(NULL)
{
	ASSERT(phr);
	m_pNum = pinNum;
	m_pCustomRTCTransceiver = pCustomRTCTransceiver;
	m_sshandle = CreateEvent(NULL, NULL, NULL, NULL);
	m_packetReady = CreateEvent(NULL, NULL, NULL, NULL);
	CreateDepacketizer(pinNum);
}

CRTOutPin::~CRTOutPin()
{
	HANDLE handles[] = { m_hSkipManager, m_consume};
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

	m_preferred_mtv.Clear();
	m_connection_mtv.Clear();
}

HRESULT CRTOutPin::CheckMediaType(const CMediaType *pmt)
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

	return S_OK;
}

HRESULT CRTOutPin::Active(void)
{
	if (bool(m_pAllocator))
	{
		assert(bool(m_pInputPin));

		const HRESULT hr = m_pAllocator->Commit();
		assert(SUCCEEDED(hr));
	}
	return S_OK;
}

HRESULT CRTOutPin::Inactive(void)
{
	if (bool(m_pAllocator))
	{
		assert(bool(m_pInputPin));

		HRESULT hr = m_pAllocator->Decommit();
		assert(SUCCEEDED(hr));
	}
	return S_OK;
}

HRESULT CRTOutPin::DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * ppropInputRequest)
{
	UNREFERENCED_PARAMETER(pAlloc);
	UNREFERENCED_PARAMETER(ppropInputRequest);
	return S_OK;
}

HRESULT CRTOutPin::Connect(
	IPin* pin,
	const AM_MEDIA_TYPE* pmt)
{
	HRESULT hr = S_OK;

	if (pin == 0)
		return E_POINTER;

	CAutoLock cObjectLock(m_pLock);

	if (!m_pFilter->IsStopped())
		return VFW_E_NOT_STOPPED;

	if (IsConnected())
		return VFW_E_ALREADY_CONNECTED;

	//Fail here if type hasn't decided through internet type negotiation

	m_connection_mtv.Clear();

	if (pmt)
	{
		hr = QueryAccept(pmt);

		if (hr != S_OK)
			return VFW_E_TYPE_NOT_ACCEPTED;

		hr = pin->ReceiveConnection(this, pmt);

		if (FAILED(hr))
			return hr;

		const AM_MEDIA_TYPE& mt = *pmt;

		m_connection_mtv.Add(mt);
	}
	else
	{
		ULONG i = 0;
		const ULONG j = m_preferred_mtv.Size();

		while (i < j)
		{
			const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

			hr = pin->ReceiveConnection(this, &mt);

			if (SUCCEEDED(hr))
				break;

			++i;
		}

		if (i >= j)
			return VFW_E_NO_ACCEPTABLE_TYPES;

		const AM_MEDIA_TYPE& mt = m_preferred_mtv[i];

		m_connection_mtv.Add(mt);
	}

	hr = PostConnect(pin);

	if (SUCCEEDED(hr))
		m_Connected = pin;

	return hr;
}

HRESULT CRTOutPin::PostConnect(IPin* p)
{
	const VP8PassMode m = GetPassMode();

	if (m == kPassModeFirstPass)  //stream output
		return PostConnectStats(p);
	else
		return PostConnectVideo(p);
}

HRESULT CRTOutPin::PostConnectStats(IPin* p)
{
	IStreamPtr pStream;

	HRESULT hr = p->QueryInterface(&pStream);

	if (FAILED(hr))
		return hr;

	const GraphUtil::IMemInputPinPtr pMemInput(p);

	if (bool(pMemInput))
	{
		GraphUtil::IMemAllocatorPtr pAllocator;

		hr = pMemInput->GetAllocator(&pAllocator);

		if (FAILED(hr))
		{
			hr = CMediaSample::CreateAllocator(&pAllocator);

			if (FAILED(hr))
				return VFW_E_NO_ALLOCATOR;
		}

		assert(bool(pAllocator));

		ALLOCATOR_PROPERTIES props;

		props.cBuffers = -1;    //number of buffers
		props.cbBuffer = -1;    //size of each buffer, excluding prefix
		props.cbAlign = -1;     //applies to prefix, too
		props.cbPrefix = -1;    //imediasample::getbuf does NOT include prefix

		hr = pMemInput->GetAllocatorRequirements(&props);

		if (props.cBuffers < 0)
			props.cBuffers = 0;

		if (props.cbBuffer < 0)
			props.cbBuffer = 0;

		if (props.cbAlign <= 0)
			props.cbAlign = 1;

		if (props.cbPrefix < 0)
			props.cbPrefix = 0;

		ALLOCATOR_PROPERTIES actual;

		hr = pAllocator->SetProperties(&props, &actual);

		if (FAILED(hr))
			return hr;

		hr = pMemInput->NotifyAllocator(pAllocator, 0);  //allow writes

		if (FAILED(hr) && (hr != E_NOTIMPL))
			return hr;

		m_pAllocator = pAllocator;
		m_pInputPin = pMemInput;
	}

	m_pStream = pStream;

	return S_OK;
}

HRESULT CRTOutPin::PostConnectVideo(IPin* p)
{
	IMemInputPin* pInputPin;

	HRESULT hr = p->QueryInterface(&pInputPin);

	if (FAILED(hr))
		return hr;

	IMemAllocator* pAllocator;

	hr = CVP8Sample::CreateAllocator(&pAllocator);

	if (FAILED(hr))
		return VFW_E_NO_ALLOCATOR;

	return InitAllocator(pInputPin, pAllocator);
}

HRESULT CRTOutPin::InitAllocator(
	IMemInputPin* pInputPin,
	IMemAllocator* pAllocator)
{
	assert(pInputPin);
	assert(pAllocator);

	ALLOCATOR_PROPERTIES props, actual;

	props.cBuffers = -1;    //number of buffers
	props.cbBuffer = -1;    //size of each buffer, excluding prefix
	props.cbAlign = -1;     //applies to prefix, too
	props.cbPrefix = -1;    //imediasample::getbuffer does NOT include prefix

	HRESULT hr = pInputPin->GetAllocatorRequirements(&props);

	if (props.cBuffers <= 0)
		props.cBuffers = 1;

	const BITMAPINFOHEADER& bmih = GetBMIH();

	LONG w = bmih.biWidth;
	assert(w > 0);

	LONG h = labs(bmih.biHeight);
	assert(h > 0);

	if (w % 16)
		w = 16 * ((w + 15) / 16);

	if (h % 16)
		h = 16 * ((h + 15) / 16);

	const long cbBuffer = w*h + 2 * (w / 2)*(h / 2);

	if (props.cbBuffer < cbBuffer)
		props.cbBuffer = cbBuffer;

	if (props.cbAlign <= 0)
		props.cbAlign = 1;

	if (props.cbPrefix < 0)
		props.cbPrefix = 0;

	hr = pAllocator->SetProperties(&props, &actual);

	if (FAILED(hr))
		return hr;

	hr = pInputPin->NotifyAllocator(pAllocator, 0);  //allow writes

	if (FAILED(hr) && (hr != E_NOTIMPL))
		return hr;

	m_pInputPin = pInputPin;
	m_pAllocator = pAllocator;

	return S_OK;  //success
}

VP8PassMode CRTOutPin::GetPassMode() const
{
	const Config::int32_t m = m_cfg.pass_mode;

	if (m < 0)
		return kPassModeOnePass;  //default

	return static_cast<VP8PassMode>(m);
}

void CRTOutPin::Config::Init()
{
	encoder_kind = 0;  //TODO(matthewjheaney): resolve whether we need -1 here
	deadline = 1;
	threads = -1;
	error_resilient = -1;
	lag_in_frames = -1;
	target_bitrate = -1;
	min_quantizer = -1;
	max_quantizer = -1;
	undershoot_pct = -1;
	overshoot_pct = -1;
	decoder_buffer_size = -1;
	decoder_buffer_initial_size = -1;
	decoder_buffer_optimal_size = -1;
	keyframe_mode = -1;
	keyframe_min_interval = -1;
	keyframe_max_interval = -1;
	dropframe_thresh = -1;
	resize_allowed = -1;
	resize_up_thresh = -1;
	resize_down_thresh = -1;
	end_usage = -1;
	token_partitions = -1;
	pass_mode = -1;
	two_pass_stats_buf = 0;
	two_pass_stats_buflen = -1;
	two_pass_vbr_bias_pct = -1;
	two_pass_vbr_minsection_pct = -1;
	two_pass_vbr_maxsection_pct = -1;
	auto_alt_ref = -1;
	arnr_max_frames = -1;
	arnr_strength = -1;
	arnr_type = -1;
	cpu_used = -17;
	static_threshold = -1;
}

const BITMAPINFOHEADER& CRTOutPin::GetBMIH() const
{
	assert(m_connection_mtv.Size() == 1);

	const AM_MEDIA_TYPE& mt = m_connection_mtv[0];
	assert(mt.pbFormat);

	if (mt.formattype == FORMAT_VideoInfo)
	{
		assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));

		const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
		const BITMAPINFOHEADER& bmih = vih.bmiHeader;

		return bmih;
	}

	assert(mt.formattype == FORMAT_VideoInfo2);
	assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER2));

	const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);
	const BITMAPINFOHEADER& bmih = vih.bmiHeader;

	return bmih;
}

HRESULT CRTOutPin::SetPreferredMTV(AM_MEDIA_TYPE& mt)
{
	HRESULT hr;
	do
	{
		hr = m_preferred_mtv.Clear();
		BREAK_ON_FAIL(hr);

		hr = m_preferred_mtv.Add(mt);
		BREAK_ON_FAIL(hr);

	} while (false);
	return hr;
}

bool CRTOutPin::ReceivePacket(PACKET_UNION *p)
{
#ifdef _DEBUG
	wodbgstream os;
	os << "START START START START START START START START START START START START START" << std::endl;
	os << "Reading Packet ssrc: " << p->p.ssrc << "; target: pin " << GetPinNum() << "; seq: " << p->c.seq << "; type: " << ((p->c.type == DATA_PACKET) ? "data" : "xor") << std::endl;
#endif
	bool frame_ready = false;

	//check for valid packet
	/*1 - invalid packets, wrong source*/
	//wrong ssrc, not the same as depacketizer ssrc exit
	if (d.ssrc != p->c.ssrc)
	{
		assert(!(d.ssrc != p->c.ssrc));
		return 0;
	}

	//xor/data packet has the previous timestamp, if previous frame is consumed, there is no reason to exist, so we skip it on purpose to save unnecessary performance wastage
	if ((p->c.timestamp + 1) <= ((d.last_consumed_timestamp + 1) & 0xffffffff) || (d.p[p->c.seq & PSM].c.timestamp == p->c.timestamp && d.p[p->c.seq & PSM].c.size == p->c.size))	// 1st condition is for used packet, 2nd condition is for existing but still not used packet
	{
#ifdef _DEBUG
		os << ((p->c.type == DATA_PACKET) ? "DATA" : "XOR") << " packet seq: " << p->c.seq << " is useless" << std::endl;
#endif
		if (p->c.type == DATA_PACKET && (!((p->c.timestamp + 1) <= ((d.last_consumed_timestamp + 1) & 0xffffffff))))
		{

			//assert((x->timestamp + 1) <= ((d.last_consumed_timestamp + 1) & 0xffffffff));
			//assert(!RemoveSkip(x->seq));
			//make sure still not in skipped store
			//while (d.m_ssinUse.test_and_set(std::memory_order_relaxed));  // acquire lock
			d.m_ssinUse.test_and_set(std::memory_order_acquire);
			int i;
			bool skip_exist = false;

			//remove packet from skip store if its there it came out of order...
			for (i = 0; i < SS; i++) {
				if (p->c.seq == d.s[i].seq && !d.s[i].acquired) {
					//d.s[i].seq = PS + 1;	//reset
					skip_exist = true;
					break;
				}
			}
			d.m_ssinUse.clear(std::memory_order_release);
			if (skip_exist)
				os << "Data packet seq: " << p->c.seq << " skips exists but is useless" << std::endl;
			assert(skip_exist == false);

		}
		return 0;
	}

	


	if (p->r.type == XOR_PACKET)
		ReadXOR((XORPACKET*)p, &frame_ready);
	else
		ReadPacket((PACKET*)p, &frame_ready);
		
	//return;	// for testing purpose
	int i;
	if (FrameReady(&i))
	{
#ifdef _DEBUG
		os << "new Frame(s)?" << i << std::endl;
#endif
		BYTE* data;
		GraphUtil::IMediaSamplePtr pOutSample;
		unsigned int timestamp;
		bool isDiscontinuity;

			
		do
		{

			HRESULT hr = m_pAllocator->GetBuffer(&pOutSample, 0, 0, 0);
			//BREAK_ON_FAIL(hr);
			assert(hr == S_OK);
#ifdef _DEBUG
			//os << "m_pAllocator returned hr: " << hr << std::endl;
#endif

			_COM_SMARTPTR_TYPEDEF(IVP8Sample, __uuidof(IVP8Sample));
			const IVP8SamplePtr pSample(pOutSample);
			assert(bool(pSample));

			IVP8Sample::Frame& f = pSample->GetFrame();
			assert(f.buf == 0);

			hr = CVP8Sample::GetFrame((CMemAllocator*)m_pAllocator, f);
			//BREAK_ON_FAIL(hr);
			assert(SUCCEEDED(hr));

			//f.off is definitely zero because it depends on downstream filter, not encoder

			if (GetFrame(f.buf, &f.len, &timestamp, &isDiscontinuity))
			{
				f.start = timestamp * 10000;	//convert to 100 nanosecond unit
				f.stop = -1;

				f.key = isDiscontinuity;

				hr = pOutSample->SetPreroll(false);
				//BREAK_ON_FAIL(hr);
				assert(SUCCEEDED(hr));

				hr = pOutSample->SetDiscontinuity(isDiscontinuity);
				//BREAK_ON_FAIL(hr);
				assert(SUCCEEDED(hr));

				hr = m_pInputPin->Receive(pOutSample);
				//BREAK_ON_FAIL(hr);
				assert(SUCCEEDED(hr));
			}
			i--;
		} while (i > 0);
	}
	else
	{
#ifdef _DEBUG
		os << "Finished Reading Packet................ new Frame?0" << std::endl;
#endif
	}
#ifdef _DEBUG
	os << "END END END END END END END END END END END END END END END END END END " << std::endl;
#endif
}

DWORD WINAPI skip_manager_thread(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CRTOutPin *pin = (CRTOutPin*)lpParam;
	CustomRTCTransceiver *filter = (CustomRTCTransceiver*)pin->m_pCustomRTCTransceiver;

	HANDLE handles[] = { pin->m_sshandle, filter->m_hQuit};
	// loop and send resend request every return trip time until no more skips
	// send
	// back to waiting for skip event

	while (true)
	{
		// wait for skip event and quit event (any one of them will trigger)
		DWORD dwEvent = WaitForMultipleObjects(2, handles, false, INFINITE);
		if (dwEvent == WAIT_OBJECT_0 + 1)	// quit event
		{
			return 0;
		}
		else
		{
			while (true)
			{
				//while (pin->d.m_ssinUse.test_and_set(std::memory_order_relaxed));  // acquire lock
				pin->d.m_ssinUse.test_and_set(std::memory_order_acquire);
				if (pin->d.skip_count > 0)	// if has skipped packet
				{
					// compile skip list
					memset(pin->d.resend_req.list, 0, sizeof(pin->d.resend_req.list));
					int j = 0, count = 0;
					_timespec64 now;
					_timespec64_get(&now, 1);
					for (unsigned int i = 0; i < SS; i++)
					{
						if (pin->d.s[i].acquired == false)
						{
							unsigned long long diff = Depacketizer::_timespec64_diff(&now, &pin->d.s[i].spawn_time);	//in ms
							if (diff == 0) diff = 1; //just in case
							if (diff > pin->d.s[i].req_marker)
							{
								pin->d.s[i].req_marker = diff;
								pin->d.resend_req.list[j] = pin->d.s[i].seq;
								pin->d.s[i].req_marker++;
								j++;
#if _DEBUG
								wodbgstream os;
								os << "requesting resend for skipped seq " << pin->d.s[i].seq << std::endl;
#endif
							}
							count++;
							if (count == pin->d.skip_count)	//covered all skipped
								break;
						}
					}
					pin->d.resend_req.count = j;
#if _DEBUG
					wodbgstream os;
					os << "Current skipped count " << pin->d.skip_count << " total seq currently requested " << j << std::endl;
#endif
					pin->d.m_ssinUse.clear(std::memory_order_release);	//release lock as soon as we are finished using

					// send resend request
					int bytes_sent;
					net_sendto(&(filter->m_socket), (c8 *)&pin->d.resend_req, sizeof(PACKET_RESEND), &bytes_sent, filter->m_socket.remote.in_addr[HOST]);
				}
				else {	// no skipped packet
					pin->d.m_ssinUse.clear(std::memory_order_release);	//release lock
					break; // break out of this loop and back to infinite waiting
				}


				DWORD dwSingleEvent = WaitForSingleObject(filter->m_hQuit, filter->rtt);	//either rtt times out for another resend request(if any) or we quit
				if (dwSingleEvent == WAIT_OBJECT_0)	// quit event
				{
					return 0;
				}
			}
		}
	}
	return 0;
}

void CRTOutPin::skip_manager()
{
	m_hSkipManager = CreateThread(NULL, 0, skip_manager_thread, this, 0, NULL);
}

int CRTOutPin::ReadPacket(PACKET *x, bool* frame_ready)
{
#ifdef _DEBUG
	wodbgstream os;
#endif
	//x->seq = R2(x->seq);
	//x->timestamp = R4(x->timestamp);
	bool skip_fill = false;

	//types of packets
	//1 - invalid packets, wrong source - useless
	//2 - valid packets
	//2.1 - packets that are too old/redundant(already received) or when we already have it via recovery - useless
	//2.2 - on time packets	 - useful
	//2.2.1 - normal order
	//2.2.2 - out of order/nth packet of first that indicates presence of skipped packets
	//2.3 - late but not too late / initially skipped packets(skips filler) - useful

	/*Useful packets from this point onwards*/
#ifdef _DEBUG
	os << "ReadPacket << Data packet seq: "<< x->seq <<" is useful" << std::endl;
#endif
	//for all useful packets
	d.p[x->seq & PSM].p = *x;	//save packets to depacketizer

	//this takes care of the late arrival of skipped packets
	skip_fill = RemoveSkip(x->seq & PSM);	//check if got receive a xth frame packet that is already in the skip store

	if (skip_fill)	//late packets
	{
		AdjSkipsTimestamp(x->seq & PSM);

		//can improve on recovery once it is received
		//if (IS_DATA_PACKET(x->seq)) // must be DATA_PACKET
		//{
			bool xor_present = false;
			int xor_location = (x->seq - ((x->seq + 1) % (FEC_INTERVAL + 1)) + (FEC_INTERVAL + 1)) & PSM;	//refer to truth table and logic excerpt
			XORPACKET* r = NULL;
			//get xor packet location and check if xor is there
			if (d.p[xor_location].c.size != 0 && d.p[xor_location].c.timestamp >= x->timestamp)	//xor not empty and xor is same or newer than the late packet
			{
				r = &d.p[xor_location].r;
				xor_present = true;
			}
			/*
			for (int j = 0; j < FEC_INTERVAL; j++)
			{
			if (IS_XOR_PACKET((x->seq + j) & PSM) && d.p[(x->seq + j) & PSM].c.size != 0)
			{
			xor_present = true;
			r = &d.p[((x->seq + j) & PSM)].r;
			break;
			}
			}*/


			//if (xor_present && r != NULL)
			
			if (xor_present)	//definitely has content
			{
				//check for previous packets skips and see if recovery is possible
				char build_target_offset = 0;
				if (rebuildPossible(r, &build_target_offset))
				{
					rebuildSeq(r, build_target_offset);
#if _DEBUG
					os << "ReadPacket << rebuilt seq " << (r->seq - build_target_offset) << std::endl;
#endif
				}
			}
		//}
	}
	else
	{
		//on time packet
		if (x->seq != ((d.last_seq + 1) & PSM))	//out of order/nth packet, indicating skips in between, late arrival of skipped packets is already taken care of above
		{
#ifdef _DEBUG
			//os << "Ssrc " << x->ssrc << " skipped packet: x->seq: " << x->seq << "; d.last_seq: " << d.last_seq << "; ((d.last_seq + 1) & PSM): " << ((d.last_seq + 1) & PSM) << "; condition met:" << (x->seq != ((d.last_seq + 1) & PSM)) << std::endl;
#endif
			//add to skip store for each skipped packets
			for (unsigned short sn = ((d.last_seq + 1) & PSM); sn != x->seq; sn = (++sn & PSM))
			{
				SetSkipTimestamp(sn);

				//don't bother skipping about recovery packet
				if (IS_XOR_PACKET(sn))	//e.g 0 1 2 (3) 4 5 6 (7) 8 9 10 (11) 12 13 14 (15) 16 17 18 (19) 20
				{
#ifdef _DEBUG
					os << "ReadPacket << Ssrc " << x->ssrc << " skipped XOR packet: x-seq: " << sn << std::endl;
#endif
					//d.last_seq = sn;	//treat missing recovery as present, just last_seq, shouldn't be last_seq because this xor_packet is in between the oldest(not yet consumed) and latest(which will have the last_seq)
					d.p[sn].c.seq = sn;
					//d.p[sn].c.type = XOR_PACKET;
					d.p[sn].c.size = 0; //only size tell us if it is missing
					//AddXSkip(sn);
				}
				else
				{
#ifdef _DEBUG
					os << "ReadPacket << Ssrc " << x->ssrc << " skipped DATA packet: x-seq: " << sn << std::endl;
#endif
					AddSkip(sn);
					SetEvent(m_sshandle);
				}
			}
		}
		//for all on time packets including normal order
		d.last_seq = x->seq;
	}
	return 0;
}

int CRTOutPin::ReadXOR(XORPACKET * x, bool * frame_ready)	//refer to ReadPacket for comments for same logic
{
#ifdef _DEBUG
	wodbgstream os;
#endif
	//x->seq = R2(x->seq);
	//x->timestamp = R4(x->timestamp);

	/*Useful packets from this point onwards*/
#ifdef _DEBUG
	os << "ReadXOR << XOR packet seq: " << x->seq << " is useful" << std::endl;
#endif
	//for all useful packets
	d.p[x->seq & PSM].r = *x;

	//don't bother about RemoveSkip(), because xor never gets into skips even if it is absent/skipped

	//Skipped data packets handling before current xor sequence like ReadPacket()
	if (x->seq != ((d.last_seq + 1) & PSM))
	{
#ifdef _DEBUG
		//os << "Potential skipped packet: x->seq: " << x->seq << "; d.last_seq: " << d.last_seq << "; ((d.last_seq + 1) & PSM): " << ((d.last_seq + 1) & PSM) << "; condition met:" << (x->seq != ((d.last_seq + 1) & PSM)) << std::endl;
#endif
		for (unsigned short sn = ((d.last_seq + 1) & PSM); sn != x->seq; sn = (++sn & PSM))
		{
			SetSkipTimestamp(sn);

			if (IS_XOR_PACKET(sn))
			{
#ifdef _DEBUG
				os << "ReadXOR << Ssrc " << x->ssrc << " skipped XOR packet: x-seq: " << sn << std::endl;
#endif
				//d.last_seq = sn;	//just like in readPacket();
				d.p[sn].c.seq = sn;
				//d.p[sn].c.type = XOR_PACKET;
				d.p[sn].c.size = 0;
			}
			else
			{
#ifdef _DEBUG
				os << "ReadXOR << Ssrc " << x->ssrc << " skipped DATA packet: x-seq: " << sn << std::endl;
#endif
				AddSkip(sn);
				SetEvent(m_sshandle);
			}
		}
	}

	/*
	//if (((x->seq - d.p[d.oldest_seq].c.seq) & PSM) >= ((x->seq - d.p[d.last_seq].c.seq) & PSM)) // make sure xor is not a late packet. take into account last == oldest, this xor packet can never be the oldest because when the one before oldest is consumed, xor has no reason to exist.
	//if ((((x->seq - d.p[d.oldest_seq].c.seq) & PSM) > ((x->seq - d.p[d.last_seq].c.seq) & PSM)) || (d.p[d.oldest_seq].c.seq == ((d.p[d.last_seq].c.seq + 1) & PSM) && ((x->seq - d.p[d.oldest_seq].c.seq) & PSM) < ((x->seq - d.p[d.last_seq].c.seq) & PSM)))
	{

		if (!(((x->seq - d.p[d.oldest_seq].c.seq) & PSM) > ((x->seq - d.p[d.last_seq].c.seq) & PSM)))
		{
#ifdef _DEBUG
			os << "ReadXOR << special condition to set for last_seq for XOR-packet: x-seq: " << x->seq << std::endl;
#endif
		}
		d.last_seq = x->seq;
	}*/

	if ((((x->seq - d.p[d.oldest_seq].c.seq) & PSM) >= ((x->seq - d.p[d.last_seq].c.seq) & PSM)) || (d.p[d.oldest_seq].c.seq == ((d.p[d.last_seq].c.seq + 1) & PSM)))
	{
		d.last_seq = x->seq;
	}
	else
	{
		AdjSkipsTimestamp(x->seq & PSM);
	}

	//check for previous packets skips and see if recovery is possible
	
	char build_target_offset = 0;
	if (rebuildPossible(x, &build_target_offset))
	{
		rebuildSeq(x, build_target_offset);
#if _DEBUG
		wodbgstream os;
		os << "ReadXOR << rebuilt seq " << (x->seq - build_target_offset) << std::endl;
#endif
	}
	return 0;
}

DWORD WINAPI consume(LPVOID lpParam)
{
	HRESULT hr = S_OK;
	CRTOutPin *rtpin = (CRTOutPin*)lpParam;
	CustomRTCTransceiver *filter = rtpin->m_pCustomRTCTransceiver;
	while (true)
	{
		HANDLE handles[] = { rtpin->m_packetReady, filter->m_hQuit };
		DWORD dwEvent = WaitForMultipleObjects(2, handles, false, INFINITE);
		if (dwEvent == WAIT_OBJECT_0 + 1)	// quit event
		{
			return 0;
		}
		else
		{
			int size = rtpin->m_queue.size();
			if (size > 0 && !rtpin->m_queue.end_of_queue())
			{
				for (int i = 0; i < size; i++)
				{
					if (rtpin->m_queue.front_ready())
					{
						PACKET_UNION* pkt;
						ULONG len;
						rtpin->m_queue.front(&pkt, &len);
						//consume
						rtpin->ReceivePacket(pkt);
						//pop
						rtpin->m_queue.pop();
					}
				}
			}
		}
	}
	return 0;
}