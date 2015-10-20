#include <limits.h>
#include "cfg/cfg.h"
#include "oslib/oslib.h"
#include "audiostream.h"

struct SoundFrame { s16 l;s16 r; };
#define SAMPLE_COUNT 512

SoundFrame RingBuffer[SAMPLE_COUNT];
const u32 RingBufferByteSize = sizeof(RingBuffer);
const u32 RingBufferSampleCount = SAMPLE_COUNT;

volatile u32 WritePtr;  //last WRITEN sample
volatile u32 ReadPtr;   //next sample to read

u32 gen_samples=0;

double time_diff = 128/44100.0;
double time_last;

bool RegisterAudioBackend(audiobackend_t *backend)
{
	return true;
}

void RegisterAllAudioBackends() {
}

u32 PushAudio(void* frame, u32 amt, bool wait)
{
   /* audio_batch_cb goes here - return amount of samples */
	return 0;
}

u32 asRingUsedCount()
{
	if (WritePtr>ReadPtr)
		return WritePtr-ReadPtr;
   return RingBufferSampleCount-(ReadPtr-WritePtr);
}
u32 asRingFreeCount()
{
	return RingBufferSampleCount-asRingUsedCount();
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
	if (cfgLoadInt("audio", "disable", 0)) {
		printf("WARNING: Audio disabled in config!\n");
		return;
	}

	cfgSaveInt("audio","disable",0);

	printf("Initializing audio backend...\n");
}

void TermAudio()
{
}
