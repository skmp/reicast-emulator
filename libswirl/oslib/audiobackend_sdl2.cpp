/*
	This file is part of libswirl
*/
#include "license/bsd"


#if defined(USE_SDL_AUDIO)

#include <SDL2/SDL.h>
#include "oslib/audiostream.h"
#include "stdclass.h"

static audio_backend_pull_callback_t pull_callback;

static SDL_AudioDeviceID audiodev;
static bool needs_resampling;

// To easily access samples.
union Sample { int16_t s[2]; uint32_t l; };

static void sdl2_audiocb(void* userdata, Uint8* stream, int len) {
	unsigned oslen = len / sizeof(uint32_t);

	if (!needs_resampling) {
		if (pull_callback(nullptr, 0, 0, 0) >= oslen)
		{
			pull_callback(stream, oslen, oslen, 0);
		}
		else {
			memset(stream, 0, oslen);
		}
	}
	else
	{
		if (pull_callback(nullptr, 0, 0, 48000) >= oslen)
		{
			pull_callback(stream, oslen, oslen, 48000);
		}
		else {
			memset(stream, 0, oslen);
		}
	}
}

static void sdl2_audio_init(audio_backend_pull_callback_t pull_callback)
{
	::pull_callback = pull_callback;

	if (!SDL_WasInit(SDL_INIT_AUDIO))
		SDL_InitSubSystem(SDL_INIT_AUDIO);

	// Support 44.1KHz (native) but also upsampling to 48KHz
	SDL_AudioSpec wav_spec, out_spec;
	memset(&wav_spec, 0, sizeof(wav_spec));
	wav_spec.freq = 44100;
	wav_spec.format = AUDIO_S16;
	wav_spec.channels = 2;
	wav_spec.samples = 1024;  // Must be power of two
	wav_spec.callback = sdl2_audiocb;
	
	// Try 44.1KHz which should be faster since it's native.
	audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
	if (!audiodev) {
		needs_resampling = true;
		wav_spec.freq = 48000;
		audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
		verify(audiodev);
	}

	if (SDL_GetAudioDeviceStatus(audiodev) != SDL_AUDIO_PLAYING)
		SDL_PauseAudioDevice(audiodev, 0);
}

static u32 sdl2_audio_push(void* frame, u32 samples, bool wait) {
	return 0;
}

static void sdl2_audio_term() {
    // Stop audio playback.
	SDL_PauseAudioDevice(audiodev, 1);
}

static bool sdl2_pull_preferred() {
	return true;
}

audiobackend_t audiobackend_sdl2audio = {
		"sdl2", // Slug
		"Simple DirectMedia Layer 2 Audio", // Name
		&sdl2_audio_init,
		&sdl2_audio_push,
		&sdl2_audio_term,
		NULL,
		&sdl2_pull_preferred
};

static bool sdl2audiobe = RegisterAudioBackend(&audiobackend_sdl2audio);

#endif

