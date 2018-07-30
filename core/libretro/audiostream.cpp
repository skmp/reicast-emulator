#include "types.h"

#include <libretro.h>

#define SAMPLE_COUNT 512

extern retro_audio_sample_batch_t audio_batch_cb;

struct SoundFrame
{
   s16 l;
   s16 r;
};

void WriteSample(s16 r, s16 l)
{
   static SoundFrame RingBuffer[SAMPLE_COUNT];
   static const u32 RingBufferByteSize = sizeof(RingBuffer);
   static const u32 RingBufferSampleCount = SAMPLE_COUNT;
   static volatile u32 WritePtr;  //last written sample
   static volatile u32 ReadPtr;   //next sample to read
	const u32 ptr = (WritePtr+1)%RingBufferSampleCount;
	RingBuffer[ptr].r=r;
	RingBuffer[ptr].l=l;
	WritePtr=ptr;

	if (WritePtr==(SAMPLE_COUNT-1))
      audio_batch_cb((const int16_t*)RingBuffer, SAMPLE_COUNT);
}
