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

#include "oslib/audiobackend_coreaudio.h"

#if HOST_OS == OS_DARWIN

// Ring buffer quick hack
TPCircularBuffer buffer;
static uint16_t cb_write(const void *inBuffer, uint16_t maxLength);
static uint16_t cb_read(void *outBuffer, uint16_t len);

//#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

AudioUnit audioUnit;

u8 samples_temp[1024 * 4];

volatile int samples_ptr = 0;

OSStatus coreaudio_callback(void* ctx, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                            UInt32 bus, UInt32 frames, AudioBufferList* abl)
{
    verify(frames <= 1024);

    u8* src = samples_temp;

    for (int i = 0; i < abl->mNumberBuffers; i++) {
		cb_read(abl->mBuffers[i].mData, abl->mBuffers[i].mDataByteSize);
//
//        memcpy(abl->mBuffers[i].mData, src, abl->mBuffers[i].mDataByteSize);
//        src += abl->mBuffers[i].mDataByteSize;
    }

//    samples_ptr -= frames * 2 * 2;

//    if (samples_ptr < 0)
//        samples_ptr = 0;
//
    return noErr;
}

// We're making these functions static - there's no need to pollute the global namespace
static void coreaudio_init()
{
    OSStatus err;
    AURenderCallbackStruct callback_struct;
    AudioStreamBasicDescription format;
    AudioComponentDescription desc;
    AudioComponent component;

	TPCircularBufferInit(&buffer, 2048 * 16); // I dunno, 16 frames?
    
    desc.componentType = kAudioUnitType_Output;
#if !defined(TARGET_IPHONE) && !defined(TARGET_IPHONE_SIMULATOR)
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#else
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
    //desc.componentSubType = kAudioUnitSubType_GenericOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    component = AudioComponentFindNext(nullptr, &desc);
    
    verify(component != nullptr);
    
    err = AudioComponentInstanceNew(component, &audioUnit);
    verify(err == noErr);
    
    FillOutASBDForLPCM(format, 44100,
                       2, 16, 16, false, false, false);
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &format,
                               sizeof(AudioStreamBasicDescription));
    verify(err == noErr);
    
    callback_struct.inputProc = coreaudio_callback;
    callback_struct.inputProcRefCon = 0;
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &callback_struct,
                               sizeof callback_struct);
    verify(err == noErr);
    
    /*
    err = AudioUnitSetParameter(audioUnit,
                                kHALOutputParam_Volume,
                                kAudioUnitParameterFlag_Output, 0,
                                1, 0);
    verify(err == noErr);
    
    */
    
    err = AudioUnitInitialize(audioUnit);
    
    verify(err == noErr);
    
    err = AudioOutputUnitStart(audioUnit);

    verify(err == noErr);
}

static u32 coreaudio_push(void* frame, u32 samples, bool wait)
{
	while (samples_ptr != 0 && wait)
		;

		cb_write(frame, samples * sizeof(s32));
    /* Yeah, right */
//    while (samples_ptr != 0 && wait) ;
//
//    if (samples_ptr == 0) {
//        memcpy(&samples_temp[samples_ptr], frame, samples * 4);
//        samples_ptr += samples * 4;
//    }
//
    return 1;
}

static void coreaudio_term()
{
    OSStatus err;
    
    err = AudioOutputUnitStop(audioUnit);
    verify(err == noErr);
    
    err = AudioUnitUninitialize(audioUnit);
    verify(err == noErr);
    
    err = AudioComponentInstanceDispose(audioUnit);
    verify(err == noErr);

	TPCircularBufferCleanup(&buffer);
}

audiobackend_t audiobackend_coreaudio = {
    "coreaudio", // Slug
    "Core Audio", // Name
    &coreaudio_init,
    &coreaudio_push,
    &coreaudio_term
};
#endif

static uint16_t cb_write(const void *inBuffer, uint16_t maxLength)
{
	return TPCircularBufferProduceBytes(&buffer, inBuffer, maxLength);
}

#define MIN(a,b) (((a)<(b))?(a):(b))

static uint16_t cb_read(void *outBuffer, uint16_t len)
{
	int availableBytes = 0;
	void *head = TPCircularBufferTail(&buffer, &availableBytes);
	availableBytes = MIN(availableBytes, len);
	memcpy(outBuffer, head, availableBytes);
	TPCircularBufferConsume(&buffer, availableBytes);
	return availableBytes;
}

static uint16_t cb_availableBytes()
{
	int availableBytes = 0;
	TPCircularBufferHead(&buffer, &availableBytes);
	return availableBytes;
}

static uint16_t cb_usedBytes()
{
	return buffer.length - buffer.fillCount;
}


//
//  TPCircularBuffer.c
//  Circular/Ring buffer implementation
//
//  Created by Michael Tyson on 10/12/2011.
//  Copyright 2011-2012 A Tasty Pixel. All rights reserved.


#include <mach/mach.h>
#include <stdio.h>

#define reportResult(result,operation) (_reportResult((result),(operation),strrchr(__FILE__, '/')+1,__LINE__))
static inline bool _reportResult(kern_return_t result, const char *operation, const char* file, int line) {
	if ( result != ERR_SUCCESS ) {
		printf("%s:%d: %s: %s\n", file, line, operation, mach_error_string(result));
		return false;
	}
	return true;
}

bool TPCircularBufferInit(TPCircularBuffer *buffer, int length) {

	// Keep trying until we get our buffer, needed to handle race conditions
	int retries = 3;
	while ( true ) {

		buffer->length = round_page(length);    // We need whole page sizes

		// Temporarily allocate twice the length, so we have the contiguous address space to
		// support a second instance of the buffer directly after
		vm_address_t bufferAddress;
		kern_return_t result = vm_allocate(mach_task_self(),
										   &bufferAddress,
										   buffer->length * 2,
										   VM_FLAGS_ANYWHERE); // allocate anywhere it'll fit
		if ( result != ERR_SUCCESS ) {
			if ( retries-- == 0 ) {
				reportResult(result, "Buffer allocation");
				return false;
			}
			// Try again if we fail
			continue;
		}

		// Now replace the second half of the allocation with a virtual copy of the first half. Deallocate the second half...
		result = vm_deallocate(mach_task_self(),
							   bufferAddress + buffer->length,
							   buffer->length);
		if ( result != ERR_SUCCESS ) {
			if ( retries-- == 0 ) {
				reportResult(result, "Buffer deallocation");
				return false;
			}
			// If this fails somehow, deallocate the whole region and try again
			vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
			continue;
		}

		// Re-map the buffer to the address space immediately after the buffer
		vm_address_t virtualAddress = bufferAddress + buffer->length;
		vm_prot_t cur_prot, max_prot;
		result = vm_remap(mach_task_self(),
						  &virtualAddress,   // mirror target
						  buffer->length,    // size of mirror
						  0,                 // auto alignment
						  0,                 // force remapping to virtualAddress
						  mach_task_self(),  // same task
						  bufferAddress,     // mirror source
						  0,                 // MAP READ-WRITE, NOT COPY
						  &cur_prot,         // unused protection struct
						  &max_prot,         // unused protection struct
						  VM_INHERIT_DEFAULT);
		if ( result != ERR_SUCCESS ) {
			if ( retries-- == 0 ) {
				reportResult(result, "Remap buffer memory");
				return false;
			}
			// If this remap failed, we hit a race condition, so deallocate and try again
			vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
			continue;
		}

		if ( virtualAddress != bufferAddress+buffer->length ) {
			// If the memory is not contiguous, clean up both allocated buffers and try again
			if ( retries-- == 0 ) {
				printf("Couldn't map buffer memory to end of buffer\n");
				return false;
			}

			vm_deallocate(mach_task_self(), virtualAddress, buffer->length);
			vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
			continue;
		}

		buffer->buffer = (void*)bufferAddress;
		buffer->fillCount = 0;
		buffer->head = buffer->tail = 0;

		return true;
	}
	return false;
}

void TPCircularBufferCleanup(TPCircularBuffer *buffer) {
	vm_deallocate(mach_task_self(), (vm_address_t)buffer->buffer, buffer->length * 2);
	memset(buffer, 0, sizeof(TPCircularBuffer));
}

void TPCircularBufferClear(TPCircularBuffer *buffer) {
	int32_t fillCount;
	if ( TPCircularBufferTail(buffer, &fillCount) ) {
		TPCircularBufferConsume(buffer, fillCount);
	}
}

