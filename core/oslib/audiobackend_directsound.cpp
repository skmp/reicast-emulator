#include "oslib/audiostream.h"
#if HOST_OS==OS_WINDOWS
#include "oslib.h"
#include <initguid.h>
#include <dsound.h>

#define SOUND_BUFFER_SIZE 8192

IDirectSound8* dsound;
IDirectSoundBuffer8* buffer;

static HANDLE *buffer_events;
static int num_buffer_events = 0;
static bool dsound_running = false;
static HANDLE thread;
static DWORD tid;
static int chunk_size;
static u32 writeCursor;
static u32 bufferSize;

static audio_backend_pull_callback_t pull_callback;

DWORD CALLBACK dsound_thread(PVOID param)
{
	while (dsound_running) {
		u32 rv = WaitForMultipleObjects(num_buffer_events, buffer_events, FALSE, 20);
		if (rv == WAIT_OBJECT_0)
			return 0;

		void *p1, *p2;
		DWORD s1, s2;

		DWORD read, usedBuffer, freeBuffer;

		verifyc(buffer->GetCurrentPosition(&read, NULL));
		if (writeCursor > read)
			usedBuffer = writeCursor - read;
		else
			usedBuffer = bufferSize - (read - writeCursor);

		freeBuffer = bufferSize - usedBuffer;

		while (freeBuffer >= chunk_size)
		{
			verifyc(buffer->Lock(writeCursor, chunk_size, &p1, &s1, &p2, &s2, 0));
			{
				if (pull_callback(nullptr, 0, 0, 0) >= 512)
				{
					pull_callback(p1, s1 / 4, s1 / 4, 0);
					if (p2 != nullptr)
						pull_callback(p2, s2 / 4, s2 / 4, 0);
				}
				else {
					memset(p1, 0, s1);
					if (p2 != nullptr)
						memset(p2, 0, s2);
				}
			}
			buffer->Unlock(p1, s1, p2, s2);
			writeCursor = (writeCursor + chunk_size) % bufferSize;
			freeBuffer -= chunk_size;
		}
	}
	return 0;
}

static void directsound_init(audio_backend_pull_callback_t pull_callback)
{
	::pull_callback = pull_callback;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	verifyc(DirectSoundCreate8(NULL, &dsound, NULL));

	verifyc(dsound->SetCooperativeLevel((HWND)libPvr_GetRenderTarget(), DSSCL_PRIORITY));
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

	bufferSize = SOUND_BUFFER_SIZE * wfx.nBlockAlign;

	memset(&desc, 0, sizeof(DSBUFFERDESC));
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;// _CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;

	desc.dwBufferBytes = bufferSize;
	desc.lpwfxFormat = &wfx;

	if (settings.aica.HW_mixing == 0)
	{
		desc.dwFlags |= DSBCAPS_LOCSOFTWARE;
	}
	else if (settings.aica.HW_mixing == 1)
	{
		desc.dwFlags |= DSBCAPS_LOCHARDWARE;
	}
	else if (settings.aica.HW_mixing == 2)
	{
		//auto
	}
	else
	{
		die("settings.HW_mixing: Invalid value");
	}

	if (settings.aica.GlobalFocus)
		desc.dwFlags |= DSBCAPS_GLOBALFOCUS;

	verifyc(dsound->CreateSoundBuffer(&desc, &buffer_, 0));
	verifyc(buffer_->QueryInterface(IID_IDirectSoundBuffer8, (void**)& buffer));
	buffer_->Release();

	int num_buffers = (bufferSize / wfx.nBlockAlign) / 512;
	chunk_size = 512 * wfx.nBlockAlign;
	num_buffer_events = num_buffers + 1;
	buffer_events = new HANDLE[num_buffer_events];
	DSBPOSITIONNOTIFY* notfy = new DSBPOSITIONNOTIFY[num_buffers];
	for (int i = 0; i < num_buffers; i++)
	{
		notfy[i].dwOffset = chunk_size * i;
		notfy[i].hEventNotify = buffer_events[i+1] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	buffer_events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);

	IDirectSoundNotify* buffer_notify;
	verifyc(buffer->QueryInterface(IID_IDirectSoundNotify, (void**)& buffer_notify));
	buffer_notify->SetNotificationPositions(num_buffers, notfy);
	buffer_notify->Release();

	delete[] notfy;

	//Play the buffer !
	verifyc(buffer->Play(0, 0, DSBPLAY_LOOPING));

	dsound_running = true;
	if (thread = ::CreateThread(nullptr, 0, dsound_thread, nullptr, 0, &tid))
	{
		::SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
	}
}

static u32 directsound_push(void* frame, u32 samples, bool wait)
{
	return 0;
}

static void directsound_term()
{
	SetEvent(buffer_events[0]);
	dsound_running = false;
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);

	buffer->Stop();

	buffer->Release();
	dsound->Release();

	for (int i = 0; i < num_buffer_events; i++)
		CloseHandle(buffer_events[i]);
	delete[] buffer_events;
}

static bool directsound_preferpull()
{
	return true;
}

audiobackend_t audiobackend_directsound = {
    "directsound", // Slug
    "Microsoft DirectSound", // Name
    &directsound_init,
    &directsound_push,
    &directsound_term,
	NULL,
	&directsound_preferpull
};

static bool ds = RegisterAudioBackend(&audiobackend_directsound);
#endif
