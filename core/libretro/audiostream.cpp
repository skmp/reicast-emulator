#include "types.h"
#include "emulator.h"

#include <libretro.h>

#define SAMPLE_COUNT 512

extern retro_audio_sample_batch_t audio_batch_cb;

void WriteSample(s16 r, s16 l)
{
   static SoundFrame Buffer[SAMPLE_COUNT];
   static u32 writePtr; // next sample index
   Buffer[writePtr].r = r;
   Buffer[writePtr].l = l;

   if (++writePtr == SAMPLE_COUNT)
   {
      if ( dc_is_running() && (!settings.rend.ThreadedRendering || settings.aica.LimitFPS) )
         audio_batch_cb((const int16_t*)Buffer, SAMPLE_COUNT);
      writePtr = 0;
   }
}
