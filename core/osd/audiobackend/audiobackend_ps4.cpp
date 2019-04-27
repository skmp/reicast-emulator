/*
**	reicast: audiobackend_ps4.cpp
*/
#include "types.h"
#include "audiostream.h"

#if defined(TARGET_PS4) || BUILD_OS == OS_PS4_BSD	// defined(PS4_EXT_AUDIO) && 


/* Note: This is a basic audio template, it is NOT finished code *FIXME* 
*/

#include <kernel.h>
#include <user_service.h>
#include <system_service.h>
#include <audioout.h>

int32_t aOut_H = 0;

#define PARAMS16 (SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO << SCE_AUDIO_OUT_PARAM_FORMAT_SHIFT)

static void ps4aout_init()
{
	verify(SCE_OK == sceAudioOutInit());

	SceUserServiceUserId userId = SCE_USER_SERVICE_USER_ID_SYSTEM;
	aOut_H = sceAudioOutOpen(userId, SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, PARAMS16);

	printf(" --aOut_H: %X\n", aOut_H);
}

static void ps4aout_term() {
	sceAudioOutClose(aOut_H);
}

static u32 ps4aout_push(void* frame, u32 samples, bool wait)	// ** This is not right, doesn't wait etc check outputTime
{
	/* Output audio */
	if (sceAudioOutOutput(aOut_H, frame) < 0) {
		/* Error handling */
	}

	/* Get output process time */
	uint64_t outputTime;
	if (sceAudioOutGetLastOutputTime(aOut_H, &outputTime) < 0) {
		/* Error handling */
	}

	/* Get port state */
	SceAudioOutPortState portState;
	if (sceAudioOutGetPortState(aOut_H, &portState) < 0) {
		/* Error handling */
	}

	/* Get system state */
	SceAudioOutSystemState aoss;
	if (sceAudioOutGetSystemState(&aoss) < 0) {
		/* Error handling */
	}

	return 1;
}

audiobackend_t audiobackend_ps4 = {
		"ps4aout", // Slug
		"PS4 AudioOut", // Name
		&ps4aout_init,
		&ps4aout_push,
		&ps4aout_term
};


	// This is done in newer builds instead of using headers and global reg. *FIXME* check is this screw us somehow in newer code ??
//static bool ps4_aout = RegisterAudioBackend(&audiobackend_ps4);


#endif
