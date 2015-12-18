#include <limits.h>
#include "oslib/oslib.h"
#include "audiostream.h"
#include "../libretro/libretro.h"

struct SoundFrame { s16 l;s16 r; };
#define SAMPLE_COUNT 512

SoundFrame RingBuffer[SAMPLE_COUNT];
const u32 RingBufferByteSize = sizeof(RingBuffer);
const u32 RingBufferSampleCount = SAMPLE_COUNT;

volatile u32 WritePtr;  //last WRITEN sample
volatile u32 ReadPtr;   //next sample to read

extern retro_audio_sample_batch_t audio_batch_cb;

bool RegisterAudioBackend(audiobackend_t *backend)
{
	return true;
}

void RegisterAllAudioBackends() {
}

u32 PushAudio(void* frame, u32 amt, bool wait)
{
   audio_batch_cb((const int16_t*)frame, amt);
	return amt;
}

void WriteSample(s16 r, s16 l)
{
	const u32 ptr=(WritePtr+1)%RingBufferSampleCount;
	RingBuffer[ptr].r=r;
	RingBuffer[ptr].l=l;
	WritePtr=ptr;

	if (WritePtr==(SAMPLE_COUNT-1))
		PushAudio(RingBuffer,SAMPLE_COUNT,settings.aica.LimitFPS);
}

void InitAudio()
{
	printf("Initializing audio backend...\n");
}

void TermAudio()
{
}
