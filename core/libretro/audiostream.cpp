#include "types.h"

#include <libretro.h>

extern retro_audio_sample_batch_t audio_batch_cb;


#if !defined(TARGET_NO_THREADS)
SoundFrame RingBuffer[SAMPLE_COUNT];
SoundFrame RingBufferStored[SAMPLE_COUNT*10];
u32 ring_buffer_size = 0 ;
bool flush_audio_buf = false ;
cMutex mtx_audioLock ;
#endif

void WriteSample(s16 r, s16 l)
{
   static const u32 RingBufferByteSize = sizeof(RingBuffer);
   static const u32 RingBufferSampleCount = SAMPLE_COUNT;
   static volatile u32 WritePtr;  //last written sample
   static volatile u32 ReadPtr;   //next sample to read
	const u32 ptr = (WritePtr+1)%RingBufferSampleCount;
	RingBuffer[ptr].r=r;
	RingBuffer[ptr].l=l;
	WritePtr=ptr;

	if (WritePtr==(SAMPLE_COUNT-1))
	{
#if !defined(TARGET_NO_THREADS)
		if ( settings.rend.ThreadedRendering  )
		{
			mtx_audioLock.Lock() ;
			memcpy(((u8*)RingBufferStored)+ring_buffer_size,RingBuffer,sizeof(RingBuffer)) ;
			ring_buffer_size += sizeof(RingBuffer) ;
			if ( ring_buffer_size  >= sizeof(RingBufferStored) )
			{
				ring_buffer_size = 0 ;
			}
			flush_audio_buf = true ;
			mtx_audioLock.Unlock() ;
		}
		else
#endif
		audio_batch_cb((const int16_t*)RingBuffer, SAMPLE_COUNT);
	}
}
