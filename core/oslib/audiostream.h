#pragma once
#include "types.h"

extern bool RegisterAudioBackend(void);
extern void InitAudio();
extern u32 PushAudio(void* frame, u32 amt, bool wait);
extern void TermAudio();
