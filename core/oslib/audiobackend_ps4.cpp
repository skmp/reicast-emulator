/*
**	reicast: audiobackend_ps4.cpp
*/
#include "types.h"
#include "oslib/audiostream.h"

#if defined(TARGET_PS4) || BUILD_OS == OS_PS4_BSD


/* Note: This is a basic audio template, it is NOT finished code *FIXME* 
*/

#include <kernel.h>
#include <user_service.h>
#include <system_service.h>
#include <audioout.h>

int32_t aOut_H = 0;

constexpr u32 nSamples = 512 ;  // 256, 512, 768, 1024, 1280, 1536, 1792, or 2048

#define PARAMS16 (SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO << SCE_AUDIO_OUT_PARAM_FORMAT_SHIFT)

static void ps4aout_init()
{
	verify(SCE_OK == sceAudioOutInit());

	SceUserServiceUserId userId = SCE_USER_SERVICE_USER_ID_SYSTEM;
	aOut_H = sceAudioOutOpen(userId, SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, nSamples, 48000, PARAMS16);

	printf(" --aOut_H: %X\n", aOut_H);
}

static void ps4aout_term() {
	sceAudioOutClose(aOut_H);
}

static u32 ps4aout_push(void* frame, u32 samples, bool wait)	// ** This is not right, doesn't wait etc check outputTime
{
	int32_t res = 0;

	u32 rm = (samples%nSamples);
	if (rm)
		printf("@@ Audio Losing %d samples (%d / %d)... \n", rm, nSamples, samples);


	for (u32 s=0; s<(samples/nSamples); s++)	// *FIXME* better be multiples damnit
	{
		u16 * data = &((u16*)frame)[s * nSamples*2];		// 2bytes(s16) * nSamples (buff size) * Index
		
		if ((res=sceAudioOutOutput(aOut_H, data)) < 0) {
			printf("sceAudioOutOutput() returned: 0x%08X \n", (u32)res);
		}
		
		//res=sceAudioOutOutput(aOut_H, NULL);
	}
	res=sceAudioOutOutput(aOut_H, NULL);

	return 1;
}

audiobackend_t audiobackend_ps4 = {
		"ps4aout", // Slug
		"PS4 AudioOut", // Name
		&ps4aout_init,
		&ps4aout_push,
		&ps4aout_term
};


static bool ps4_aout = RegisterAudioBackend(&audiobackend_ps4);


#endif
