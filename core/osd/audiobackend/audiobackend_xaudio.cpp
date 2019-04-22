#include "audiobackend_xaudio.h"
#include "oslib.h"
#include <initguid.h>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

static const int RING_BUFFER_SIZE = 512 * 4;
static const int MAX_BUFFER_COUNT = 16;

// (/CX uses WRL::ComPtr, safe/shared_ptr should be good, do i care? no...)
IXAudio2* pXAudio2 = nullptr;
IXAudio2SourceVoice* pSourceVoice = nullptr;
IXAudio2MasteringVoice* pMasteringVoice = nullptr;
byte audioBuffers[MAX_BUFFER_COUNT][RING_BUFFER_SIZE];
u32 currentBuffer = 0;

static void xaudio_init()
{
	ZeroMemory(audioBuffers, sizeof(audioBuffers));
	verifyc(XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR));

#if defined(_DEBUG)
	XAUDIO2_DEBUG_CONFIGURATION debugConfig = { 0 };
	debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
	debugConfig.TraceMask = XAUDIO2_LOG_ERRORS;
	pXAudio2->SetDebugConfiguration(&debugConfig);
#endif

	verifyc(pXAudio2->CreateMasteringVoice(&pMasteringVoice));

	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = 44100;
	wfx.nBlockAlign = 4;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.wBitsPerSample = 16;

	verifyc(pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx));

	verifyc(pMasteringVoice->SetVolume(0.4f));

	verifyc(pSourceVoice->Start(0));
}

static int xaudio_getfreesz()
{
	return 0;
#if 0
	DWORD pc,wch;

	buffer->GetCurrentPosition(&pc,&wch);

	int fsz=0;
	if (wc>=pc)
		fsz=ds_ring_size-wc+pc;
	else
		fsz=pc-wc;

	fsz-=32;
	return fsz;
#endif
}

static int xaudio_getusedSamples()
{
	return 0;
	 // (ds_ring_size-xaudio_getfreesz())/4;
}

static u32 xaudio_push_nw(void* frame, u32 samplesb)
{
	XAUDIO2_VOICE_STATE state;
	u32 bytes = samplesb * 4;

	pSourceVoice->GetState(&state);

	if (state.BuffersQueued <= MAX_BUFFER_COUNT - 1)
	{
		// copy frame data to current audioBuffer
		u8* data = (u8*)frame;
		memcpy(audioBuffers[currentBuffer], data, bytes);

		// Create xaudio2 buffer to send to sourcevoice
		XAUDIO2_BUFFER buffer = { 0 };
		buffer.AudioBytes = bytes;
		buffer.pAudioData = audioBuffers[currentBuffer];
		buffer.Flags = XAUDIO2_END_OF_STREAM; // This option only supresses debug warnings for buffer queue starvation
		buffer.pContext = 0; // No callback needed atm

		verifyc(pSourceVoice->SubmitSourceBuffer(&buffer));

		currentBuffer++;
		currentBuffer %= MAX_BUFFER_COUNT;

		return 1;
	}

	return 0;
}

static u32 xaudio_push(void* frame, u32 samples, bool wait)
{
	u16* f=(u16*)frame;

	bool w=false;

	for (u32 i = 0; i < samples*2; i++)
	{
		if (f[i])
		{
			w = true;
			break;
		}
	}

	wait &= w;

	while (!xaudio_push_nw(frame, samples) && wait)
		0 && wprintf(L"FAILED waiting on audio FAILED %d\n", xaudio_getusedSamples());

	return 1;
}

static void xaudio_term()
{
	if (pSourceVoice != nullptr)
	{
		pSourceVoice->DestroyVoice();
	}
	if (pMasteringVoice != nullptr)
	{
		pMasteringVoice->DestroyVoice();
	}
	if (pXAudio2 != nullptr)
	{
		pXAudio2->Release();
	}

	pSourceVoice = nullptr;
	pMasteringVoice = nullptr;
	pXAudio2 = nullptr;
}

audiobackend_t audiobackend_xaudio = {
    L"XAudio2", // Slug
    L"Microsoft XAudio2", // Name
    &xaudio_init,
    &xaudio_push,
    &xaudio_term
};
