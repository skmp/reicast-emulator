/*
	This file is part of libswirl
*/
#include "license/bsd"


/*
    Simple Core Audio backend for osx (and maybe ios?)
    Based off various audio core samples and dolphin's code

    This is part of the Reicast project, please consult the
    LICENSE file for licensing & related information

    This could do with some locking logic to avoid
    race conditions, and some variable length buffer
    logic to support chunk sizes other than 512 bytes

    It does work on my macmini though
 */

#include "oslib/audiostream.h"
#include "oslib/threading.h"

#if HOST_OS == OS_DARWIN
#include <atomic>

#include <AudioUnit/AudioUnit.h>

static AudioUnit audioUnit;

// ~ 93 ms buffer
#define SAMPLES_BUFSIZE (4 * 1024 * 4)
static u8 samples_buffer[SAMPLES_BUFSIZE];
static std::atomic<int> samples_wptr;
static std::atomic<int> samples_rptr;
static cResetEvent bufferEmpty;

static OSStatus coreaudio_callback(void* ctx, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts, UInt32 bus, UInt32 frames, AudioBufferList* abl) {
    verify(frames <= 1024);

    for (int i = 0; i < abl->mNumberBuffers; i++) {
        u32 out_size = abl->mBuffers[i].mDataByteSize;
        u8* out_buffer = reinterpret_cast<u8*>(abl->mBuffers[i].mData);
        if ((samples_wptr - samples_rptr + SAMPLES_BUFSIZE) % SAMPLES_BUFSIZE < out_size) {
            printf("Core Audio: buffer underrun\n");
            memset(out_buffer, '\0', out_size);
        } else {
            if (samples_rptr + out_size > SAMPLES_BUFSIZE) {
                // The data wraps around the buffer, so we need to do 2 copies
                int size1 = SAMPLES_BUFSIZE - samples_rptr;
                int size2 = out_size - size1;
                memcpy(out_buffer, samples_buffer + samples_rptr, size1);
                memcpy(out_buffer + size1, samples_buffer, size2);
            } else {
                // The data doesn't wrap, so just do one copy
                memcpy(out_buffer, samples_buffer + samples_rptr, out_size);
            }
            
            // Increment the read pointer
            samples_rptr = (samples_rptr + out_size) % SAMPLES_BUFSIZE;
            
            // Set the mutex to allow writing
            bufferEmpty.Set();
        }
    }

    return noErr;
}

// We're making these functions static - there's no need to pollute the global namespace
static void coreaudio_init(audio_backend_pull_callback_t pull_callback) {
    OSStatus err;
    AURenderCallbackStruct callback_struct;
    AudioStreamBasicDescription format;
    AudioComponentDescription desc;
    AudioComponent component;
    
    // Setup the AudioComponent
    desc.componentType = kAudioUnitType_Output;
#if !defined(TARGET_IPHONE)
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#else
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    component = AudioComponentFindNext(nullptr, &desc);
    verify(component != nullptr);
    err = AudioComponentInstanceNew(component, &audioUnit);
    verify(err == noErr);
    
    // Setup the AudioUnit format (44.1KHz sample rate, 2 channels, 16 bit samples)
    FillOutASBDForLPCM(format, 44100, 2, 16, 16, false, false, false);
    err = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(AudioStreamBasicDescription));
    verify(err == noErr);
    
    // Setup the callback function
    callback_struct.inputProc = coreaudio_callback;
    callback_struct.inputProcRefCon = 0;
    err = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback_struct, sizeof callback_struct);
    verify(err == noErr);
    
    // Initialize and start the AudioUnit
    err = AudioUnitInitialize(audioUnit);
    verify(err == noErr);
    err = AudioOutputUnitStart(audioUnit);
    verify(err == noErr);
    
    // Set the mutex to allow writing
    bufferEmpty.Set();
}

static u32 coreaudio_push(void* frame, u32 samples, bool wait) {
    int byte_size = samples * 4;
    while (true) {
        // If the write pointer is outpacing the read pointer, wait until there is space in the sample buffer
        int space = (samples_rptr - samples_wptr + SAMPLES_BUFSIZE) % SAMPLES_BUFSIZE;
        if (space != 0 && byte_size > space - 1) {
            if (!wait)
                break;
            // Wait for a call to bufferEmpty.Set() before continuing
            bufferEmpty.Wait();
            continue;
        }
        
        // Copy the data to the sample buffer and increment the write pointer
        memcpy(&samples_buffer[samples_wptr], frame, byte_size);
        samples_wptr = (samples_wptr + byte_size) % SAMPLES_BUFSIZE;
        break;
    }
    
    return 1;
}

static void coreaudio_term() {
    OSStatus err;

    // Tear down the AudioUnit
    err = AudioOutputUnitStop(audioUnit);
    verify(err == noErr);
    err = AudioUnitUninitialize(audioUnit);
    verify(err == noErr);
    err = AudioComponentInstanceDispose(audioUnit);
    verify(err == noErr);
    
    // Reset the write mutex
    bufferEmpty.Set();
}

audiobackend_t audiobackend_coreaudio = {
    "coreaudio", // Slug
    "Core Audio", // Name
    &coreaudio_init,
    &coreaudio_push,
    &coreaudio_term,
	NULL
};

static bool core = RegisterAudioBackend(&audiobackend_coreaudio);

#endif
