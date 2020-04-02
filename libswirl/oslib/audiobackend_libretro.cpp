/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "oslib/audiostream.h"
#include "libretro-common/include/libretro.h"

#ifdef BUILD_RETROARCH_CORE
#include <chrono>
#include "perf_tools/moving_average.hpp"
extern retro_audio_sample_batch_t audio_batch_cb;

static void libretro_init(audio_backend_pull_callback_t pull_callback)
{
	 
}
 

static int libretro_getusedSamples()
{
	return 0;
	 // (bufferSize-xaudio_getfreesz())/4;
}
 
static u32 libretro_push(void* frame, u32 samples, bool wait) {
	if (audio_batch_cb == nullptr)
		return 1;

/*
   struct retro_audio_callback audio_cb = { audio_callback, audio_set_state };
   use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_cb);
   
*/
	audio_batch_cb((const int16_t*)frame,samples);
 
	return 1;
}

static void libretro_term()
{
	printf("Libretro_audio : terminated!\n");
	audio_batch_cb = nullptr;
}

audiobackend_t audiobackend_libretro = {
    "libretro", // Slug
    "libretro audio subsystem", // Name
    &libretro_init,
    &libretro_push,
    &libretro_term,
	NULL
};


static bool ds = RegisterAudioBackend(&audiobackend_libretro);
#endif