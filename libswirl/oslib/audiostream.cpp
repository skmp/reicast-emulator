/*
	This file is part of libswirl
*/
#include "license/bsd"


#include <limits.h>
#include "hw/holly/sb.h" // for STATIC_FORWARD
#include "cfg/cfg.h"
#include "oslib/threading.h"
#include "oslib/oslib.h"
#include "audiostream.h"

struct SoundFrame { s16 l; s16 r; };

#ifdef LOG_SOUND
// TODO Only works on Windows!
WaveWriter rawout("d:\\aica_out.wav");
#endif

static unsigned int audiobackends_num_max = 1;
static unsigned int audiobackends_num_registered = 0;
static audiobackend_t **audiobackends = NULL;

static float InterpolateCatmull4pt3oX(float x0, float x1, float x2, float x3, float t) {
	return 0.45f* ((2 * x1) + t * ((-x0 + x2) + t * ((2 * x0 - 5 * x1 + 4 * x2 - x3) + t * (-x0 + 3 * x1 - 3 * x2 + x3))));
}

static s16 SaturateToS16(float v)
{
	if (v > 32767)
		return 32767;
	if (v < -32768)
		return -32768;
	return (s16)v;
}

class PullBuffer_t {
private:
	static const u32 SAMPLE_COUNT = 4096;
	static const u32 OVERRUN_SKIP = 128;

	SoundFrame RingBuffer[SAMPLE_COUNT];
	const u32 RingBufferByteSize = sizeof(RingBuffer);
	const u32 RingBufferSampleCount = SAMPLE_COUNT;

	volatile u32 WritePtr;  //last WRITEN sample
	volatile u32 ReadPtr;   //next sample to read

	u32 asRingUsedCount()
	{
		if (WritePtr > ReadPtr)
			return WritePtr - ReadPtr;
		else
			return RingBufferSampleCount - (ReadPtr - WritePtr);
		//s32 sz=(WritePtr+1)%RingBufferSampleCount-ReadPtr;
		//return sz<0?sz+RingBufferSampleCount:sz;
	}

	u32 asRingFreeCount()
	{
		return RingBufferSampleCount - asRingUsedCount();
	}

	SoundFrame Status[4] = { { 0 } };
	float current_partial_pos = 0;
public:
	u32 ReadAudioResampling(void* buffer, u32 buffer_size, u32 amt, u32 target_rate)
	{
		if (buffer == nullptr)
			return asRingUsedCount() * target_rate / 44100;

		SoundFrame* outbuf = (SoundFrame*)buffer;

		u32 read = min(min(amt, buffer_size), asRingUsedCount());

		float inc = 44100.0f / target_rate;

		for (int i = 0; i < read; i++)
		{
			SoundFrame res = {
				SaturateToS16(InterpolateCatmull4pt3oX(Status[0].l,Status[1].l,Status[2].l,Status[3].l,current_partial_pos)),
				SaturateToS16(InterpolateCatmull4pt3oX(Status[0].r,Status[1].r,Status[2].r,Status[3].r,current_partial_pos))
			};
			outbuf[i] = res;
			current_partial_pos += inc;
			while (current_partial_pos >= 1)
			{
				Status[0] = Status[1];
				Status[1] = Status[2];
				Status[2] = Status[3];
				Status[3] = RingBuffer[ReadPtr];
				ReadPtr = (ReadPtr + 1) % RingBufferSampleCount;
				current_partial_pos -= 1;
			}
		}

		return read;
	}

	u32 ReadAudio(void* buffer, u32 buffer_size, u32 amt)
	{
		if (buffer == nullptr)
			return asRingUsedCount();

		u32 read = min(min(amt, buffer_size), asRingUsedCount());
		u32 first = min(RingBufferSampleCount - ReadPtr, read);
		memcpy(buffer, RingBuffer + ReadPtr, first * sizeof(SoundFrame));
		if (read > first)
		{
			memcpy(((SoundFrame*)buffer) + first, RingBuffer,(read - first) * sizeof(SoundFrame));
		}
		ReadPtr = (ReadPtr + read) % RingBufferSampleCount;
		return read;
	}

	void WriteSample(s16 r, s16 l, bool wait)
	{
		const u32 ptr = (WritePtr + 1) % RingBufferSampleCount;
		RingBuffer[ptr].r = r;
		RingBuffer[ptr].l = l;
		WritePtr = ptr;

		if (asRingFreeCount() < 16)
		{
			if (wait)
			{
				// infinite?
				while (asRingFreeCount() < 512)
					SleepMs(1);
			}
			else
			{
				ReadPtr = (ReadPtr + OVERRUN_SKIP) % RingBufferSampleCount;
			}
		}
	}
};

class PushBuffer_t {
private:
	static const u32 SAMPLE_COUNT = 512;

	SoundFrame RingBuffer[SAMPLE_COUNT];
	const u32 RingBufferByteSize = sizeof(RingBuffer);
	const u32 RingBufferSampleCount = SAMPLE_COUNT;

	volatile u32 WritePtr;  //last WRITEN sample

public:
	// set by creator
	audiobackend_t* audiobackend_current;

	u32 PushAudio(u32 amt, bool wait)
	{
		if (audiobackend_current != NULL) {
			return audiobackend_current->push(RingBuffer, amt, wait);
		}
		return 0;
	}

	void WriteSample(s16 r, s16 l, bool wait)
	{
		const u32 ptr = (WritePtr + 1) % RingBufferSampleCount;
		RingBuffer[ptr].r = r;
		RingBuffer[ptr].l = l;
		WritePtr = ptr;

		if (WritePtr == (SAMPLE_COUNT - 1))
		{
			PushAudio(SAMPLE_COUNT, wait);
		}
	}
};

u32 GetAudioBackendCount()
{
	return audiobackends_num_registered;
}

audiobackend_t* GetAudioBackend(int num)
{
	return audiobackends[num];
}

bool RegisterAudioBackend(audiobackend_t *backend)
{
	/* This function announces the availability of an audio backend to reicast. */
	// Check if backend is valid
	if (backend == NULL)
	{
		printf("ERROR: Tried to register invalid audio backend (NULL pointer).\n");
		return false;
	}

	if (backend->slug == "auto" || backend->slug == "none") {
		printf("ERROR: Tried to register invalid audio backend (slug \"%s\" is a reserved keyword).\n", backend->slug.c_str());
		return false;
	}

	// First call to RegisterAudioBackend(), create the backend structure;
	if (audiobackends == NULL)
		audiobackends = static_cast<audiobackend_t**>(calloc(audiobackends_num_max, sizeof(audiobackend_t*)));

	// Check if we need to allocate addition memory for storing the pointers and allocate if neccessary
	if (audiobackends_num_registered == audiobackends_num_max)
	{
		// Check for integer overflows
		if (audiobackends_num_max == UINT_MAX)
		{
			printf("ERROR: Registering audio backend \"%s\" (%s) failed. Cannot register more than %u backends\n", backend->slug.c_str(), backend->name.c_str(), audiobackends_num_max);
			return false;
		}
		audiobackends_num_max++;
		audiobackend_t **new_ptr = static_cast<audiobackend_t**>(realloc(audiobackends, audiobackends_num_max * sizeof(audiobackend_t*)));
		// Make sure that allocation worked
		if (new_ptr == NULL)
		{
			printf("ERROR: Registering audio backend \"%s\" (%s) failed. Cannot allocate additional memory.\n", backend->slug.c_str(), backend->name.c_str());
			return false;
		}
		audiobackends = new_ptr;
	}

	audiobackends[audiobackends_num_registered] = backend;
	audiobackends_num_registered++;
	return true;
}

audiobackend_t* GetAudioBackend(std::string slug)
{
	if (slug == "none")
	{
		printf("WARNING: Audio backend set to \"none\"!\n");
	}
	else if (audiobackends_num_registered > 0)
	{
		if (slug == "auto")
		{
			/* FIXME: At some point, one might want to insert some intelligent
				 algorithm for autoselecting the approriate audio backend here.
				 I'm too lazy right now. */
			printf("Auto-selected audio backend \"%s\" (%s).\n", audiobackends[0]->slug.c_str(), audiobackends[0]->name.c_str());
			return audiobackends[0];
		}
		else
		{
			for (unsigned int i = 0; i < audiobackends_num_registered; i++)
			{
				if (audiobackends[i]->slug == slug)
				{
					return audiobackends[i];
				}
			}
			printf("WARNING: Audio backend \"%s\" not found!\n", slug.c_str());
		}
	}
	else
	{
		printf("WARNING: No audio backends available!\n");
	}
	return NULL;
}

static bool backends_sorted = false;
void SortAudioBackends()
{
	if (backends_sorted)
		return;

	// Sort backends by slug
	for (int n = audiobackends_num_registered; n > 0; n--)
	{
		for (int i = 0; i < n - 1; i++)
		{
			if (audiobackends[i]->slug > audiobackends[i + 1]->slug)
			{
				audiobackend_t* swap = audiobackends[i];
				audiobackends[i] = audiobackends[i + 1];
				audiobackends[i + 1] = swap;
			}
		}
	}
}


extern double mspdf;


struct AudioStream_impl : AudioStream {
	PullBuffer_t PullBuffer;
	PushBuffer_t PushBuffer;
	audiobackend_t* audiobackend_current = nullptr;

	u32 PullAudioCallback(void* buffer, u32 buffer_size, u32 amt, u32 target_rate)
	{
		if (target_rate != 0 && target_rate != 44100)
			return PullBuffer.ReadAudioResampling(buffer, buffer_size, amt, target_rate);
		else
			return PullBuffer.ReadAudio(buffer, buffer_size, amt);
	}


	bool IsPullMode()
	{
		if (audiobackend_current != NULL && audiobackend_current->prefer_pull != NULL) {
			return audiobackend_current->prefer_pull();
		}
		return false;
	}

	void WriteSample(s16 r, s16 l)
	{
		bool wait = settings.aica.LimitFPS;

		if (IsPullMode())
		{
			PullBuffer.WriteSample(r, l, wait);
		}
		else
		{
			PushBuffer.WriteSample(r, l, wait);
		}
	}


	void InitAudio()
	{
		if (cfgLoadInt("audio", "disable", 0)) {
			printf("WARNING: Audio disabled in config!\n");
			return;
		}

		cfgSaveInt("audio", "disable", 0);

		if (audiobackend_current != NULL) {
			printf("ERROR: The audio backend \"%s\" (%s) has already been initialized, you need to terminate it before you can call audio_init() again!\n", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
			return;
		}

		SortAudioBackends();

		string audiobackend_slug = settings.audio.backend;
		audiobackend_current = GetAudioBackend(audiobackend_slug);
		PushBuffer.audiobackend_current = audiobackend_current;

		if (audiobackend_current == NULL) {
			printf("WARNING: Running without audio!\n");
			return;
		}

		printf("Initializing audio backend \"%s\" (%s)...\n", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
		
		audiobackend_current->init([=](void* buffer, u32 buffer_size, u32 amt, u32 target_rate) {
			return this->PullAudioCallback(buffer, buffer_size, amt, target_rate); 
		});
	}

	void TermAudio()
	{
		if (audiobackend_current != NULL) {
			audiobackend_current->term();
			printf("Terminating audio backend \"%s\" (%s)...\n", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
			audiobackend_current = NULL;
		}
	}
};

AudioStream* AudioStream::Create() {
	return new AudioStream_impl();
}