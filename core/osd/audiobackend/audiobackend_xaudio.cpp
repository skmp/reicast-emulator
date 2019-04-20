#include "audiobackend_xaudio.h"
#if HOST_OS==OS_WINDOWS
#include "oslib.h"
#include <initguid.h>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

void* SoundThread(void* param);
#define V2_BUFFERSZ (16*1024)

// (/CX uses WRL::ComPtr, safe/shared_ptr should be good, do i care? no...)
IXAudio2* pXAudio2 = nullptr;
IXAudio2SourceVoice* pSourceVoice = nullptr;
IXAudio2MasteringVoice* pMasterVoice = nullptr;

XAUDIO2_BUFFER buffer = {0};

u32 ds_ring_size;

static void xaudio_init()
{
	verifyc(XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR));
	verifyc(pXAudio2->CreateMasteringVoice(&pMasterVoice));

	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
	wfx.wFormatTag = WAVE_FORMAT_PCM; 
	wfx.nChannels = 2; 
	wfx.nSamplesPerSec = 44100; 
	wfx.nBlockAlign = 4; 
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; 
	wfx.wBitsPerSample = 16; 

	verifyc(pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx));

	ds_ring_size=8192*wfx.nBlockAlign;


	buffer.AudioBytes = ds_ring_size;  //buffer containing audio data
	buffer.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer


#if 0
	verifyc(DirectSoundCreate8(NULL,&dsound,NULL));

	verifyc(dsound->SetCooperativeLevel((HWND)libPvr_GetRenderTarget(),DSSCL_PRIORITY));
	IDirectSoundBuffer* buffer_;

	WAVEFORMATEX wfx; 
	DSBUFFERDESC desc; 

	// Set up WAV format structure. 

	memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
	wfx.wFormatTag = WAVE_FORMAT_PCM; 
	wfx.nChannels = 2; 
	wfx.nSamplesPerSec = 44100; 
	wfx.nBlockAlign = 4; 
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; 
	wfx.wBitsPerSample = 16; 

	// Set up DSBUFFERDESC structure. 

	ds_ring_size=8192*wfx.nBlockAlign;

	memset(&desc, 0, sizeof(DSBUFFERDESC)); 
	desc.dwSize = sizeof(DSBUFFERDESC); 
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;// _CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY; 
	
	desc.dwBufferBytes = ds_ring_size; 
	desc.lpwfxFormat = &wfx; 

	

	if (settings.aica.HW_mixing==0)
	{
		desc.dwFlags |=DSBCAPS_LOCSOFTWARE;
	}
	else if (settings.aica.HW_mixing==1)
	{
		desc.dwFlags |=DSBCAPS_LOCHARDWARE;
	}
	else if (settings.aica.HW_mixing==2)
	{
		//auto
	}
	else
	{
		die(L"settings.HW_mixing: Invalid value");
	}

	if (settings.aica.GlobalFocus)
		desc.dwFlags|=DSBCAPS_GLOBALFOCUS;

	verifyc(dsound->CreateSoundBuffer(&desc,&buffer_,0));
	verifyc(buffer_->QueryInterface(IID_IDirectSoundBuffer8,(void**)&buffer));
	buffer_->Release();

	//Play the buffer !
	verifyc(buffer->Play(0,0,DSBPLAY_LOOPING));
#endif
}


DWORD wc=0;

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
	return (ds_ring_size-xaudio_getfreesz())/4;
}

static u32 xaudio_push_nw(void* frame, u32 samplesb)
{
	DWORD pc,wch;

	u32 bytes=samplesb*4;

	
	//buffer.pAudioData = pDataBuffer;  //size of the audio buffer in bytes

	//pSourceVoice->SubmitSourceBuffer( &buffer );


#if 0
	buffer->GetCurrentPosition(&pc,&wch);

	int fsz=0;
	if (wc>=pc)
		fsz=ds_ring_size-wc+pc;
	else
		fsz=pc-wc;

	fsz-=32;

	//wprintf(L"%d: r:%d w:%d (f:%d wh:%d)\n",fsz>bytes,pc,wc,fsz,wch);

	if (fsz>bytes)
	{
		void* ptr1,* ptr2;
		DWORD ptr1sz,ptr2sz;

		u8* data=(u8*)frame;

		buffer->Lock(wc,bytes,&ptr1,&ptr1sz,&ptr2,&ptr2sz,0);
		memcpy(ptr1,data,ptr1sz);
		if (ptr2sz)
		{
			data+=ptr1sz;
			memcpy(ptr2,data,ptr2sz);
		}

		buffer->Unlock(ptr1,ptr1sz,ptr2,ptr2sz);
		wc=(wc+bytes)%ds_ring_size;
		return 1;
	}
#endif
	return 0;
	//ds_ring_size
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

	int ffs=1;
	
	/*
	while (xaudio_IsAudioBufferedLots() && wait)
		if (ffs == 0)
			ffs = wprintf(L"AUD WAIT %d\n", xaudio_getusedSamples());
	*/

	while (!xaudio_push_nw(frame, samples) && wait)
		0 && wprintf(L"FAILED waiting on audio FAILED %d\n", xaudio_getusedSamples());


	return 1;
}

static void xaudio_term()
{
#if 0
	buffer->Stop();
	buffer->Release();
#endif
	pXAudio2->Release();
}

audiobackend_t xaudio_directsound = {
    L"XAudio2", // Slug
    L"Microsoft XAudio2", // Name
    &xaudio_init,
    &xaudio_push,
    &xaudio_term
};
#endif
