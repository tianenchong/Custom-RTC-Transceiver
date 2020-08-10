#pragma once

#include "Filter.h"
#include "cstreams.h"
#include "common.h"
#include "dvdmedia.h"
#include "webmtypes.h"
#include "graphutil.h"
#include "cmediatypes.h"
//#include "cmediasample.h"
#include "rtp.h"
#include "assert.h"


DWORD WINAPI skip_manager_thread(LPVOID lpParam);
DWORD WINAPI consume(LPVOID lpParam);
class CustomRTCTransceiver;

class CRTOutPin : public CBaseOutputPin, public Depacketizer
{
public:
	CustomRTCTransceiver * m_pCustomRTCTransceiver;	//Main filter object

	// Constructor and destructor
	CRTOutPin(TCHAR *pName, CustomRTCTransceiver *pCustomRTCTransceiver, HRESULT *phr, LPCWSTR pPinName, int pinNum);
	~CRTOutPin();

	// VP8 Filter methods
	HRESULT PostConnect(IPin* p);
	HRESULT PostConnectStats(IPin* p);
	HRESULT PostConnectVideo(IPin* p);
	HRESULT CRTOutPin::InitAllocator(IMemInputPin* pInputPin, IMemAllocator* pAllocator);
	VP8PassMode CRTOutPin::GetPassMode() const;
	const BITMAPINFOHEADER& GetBMIH() const;

	// VP8 Filter structure
	struct Config
	{
		typedef __int32 int32_t;

		int32_t encoder_kind;
		int32_t deadline;
		int32_t threads;
		int32_t error_resilient;
		int32_t lag_in_frames;
		int32_t dropframe_thresh;
		int32_t resize_allowed;
		int32_t resize_up_thresh;
		int32_t resize_down_thresh;
		int32_t end_usage;
		int32_t target_bitrate;
		int32_t min_quantizer;
		int32_t max_quantizer;
		int32_t undershoot_pct;
		int32_t overshoot_pct;
		int32_t decoder_buffer_size;
		int32_t decoder_buffer_initial_size;
		int32_t decoder_buffer_optimal_size;
		int32_t keyframe_mode;
		int32_t keyframe_min_interval;
		int32_t keyframe_max_interval;
		int32_t token_partitions;
		int32_t pass_mode;
		const BYTE* two_pass_stats_buf;
		LONGLONG two_pass_stats_buflen;
		int32_t two_pass_vbr_bias_pct;
		int32_t two_pass_vbr_minsection_pct;
		int32_t two_pass_vbr_maxsection_pct;
		int32_t auto_alt_ref;
		int32_t arnr_max_frames;
		int32_t arnr_strength;
		int32_t arnr_type;
		int32_t cpu_used;
		int32_t static_threshold;

		void Init();
	};

	// CBaseOutputPin Methods
	HRESULT CheckMediaType(const CMediaType *pmt);
	HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * ppropInputRequest);

	// Override CBasePin Methods
	HRESULT Connect(IPin * pReceivePin, const AM_MEDIA_TYPE *pmt);	//pmt - optional media type
	HRESULT Active(void);
	HRESULT Inactive(void);

	HRESULT SetPreferredMTV(AM_MEDIA_TYPE &mt);
	AM_MEDIA_TYPE& GetPreferredMTV(void) { return m_preferred_mtv[0];};

	// proprietary property
	bool ReceivePacket(PACKET_UNION *p);
	int GetPinNum() { return m_pNum; };

	// public property
	Config m_cfg;
	bool m_mtSynced;
	HANDLE m_sshandle;
	ConcurrentQueue<PACKET_UNION*> m_queue;	// queue up by packets for consumption
	HANDLE m_consume;
	HANDLE m_packetReady;

	// Depacketizer pure virtual function implementation
	void skip_manager();
	HANDLE m_hSkipManager;
	int ReadPacket(PACKET * x, bool * frame_ready);
	int ReadXOR(XORPACKET * x, bool * frame_ready);
private:
	int m_pNum;	//Pin Number
	CMediaTypes m_connection_mtv;
	CMediaTypes m_preferred_mtv;
	IStreamPtr m_pStream;
};