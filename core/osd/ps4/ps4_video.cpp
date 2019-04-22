/*
**	$OSD$/ps4/ps4_video.cpp
*/
#include "types.h"

#include "ps4.h"
#include "ps4_video.h"





void *VideoMemory::memAlign(int size, int alignment)
{
	uint64_t offset = roundup(m_stackPointer, alignment);

	if (offset + size > m_stackMax) return NULL;

	m_stackPointer = offset + size;

	return (void *)(offset);
}

void VideoMemory::reset(void)
{
	m_stackPointer = m_stackBase;
}


void VideoMemory::init(size_t size)
{
	int ret;
	off_t offsetOut;

	m_stackPointer = 0;
	m_stackSize = size;
	m_stackBase = 0;


	const uint32_t  shaderGpuAlignment = 2 * 1024 * 1024;

	ret = sceKernelAllocateDirectMemory(
		0,
		SCE_KERNEL_MAIN_DMEM_SIZE,
		m_stackSize,
		shaderGpuAlignment,
		SCE_KERNEL_WC_GARLIC,
		&offsetOut);
	if (ret) {
		assert((ret == 0));
	}

	m_baseOffset = offsetOut;

	void* ptr = NULL;
	ret = sceKernelMapDirectMemory(
		&ptr,
		m_stackSize,
		SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_READ | SCE_KERNEL_PROT_GPU_ALL,
		0, //flags
		offsetOut,
		shaderGpuAlignment);
	assert((ret >= 0));


	m_stackBase = (uintptr_t)ptr;
	m_stackMax = m_stackBase + m_stackSize;

	m_stackPointer = m_stackBase;

}

void VideoMemory::finish()
{
	int ret;

	ret = sceKernelReleaseDirectMemory(m_baseOffset, m_stackSize);
	assert((ret >= 0));

	return;
}










/*
 * utility functions for libSceVideoOut
 */
 // setup VideoOut and acquire handle
int32_t initializeVideoOut(SceKernelEqueue *eqFlip)
{
	int videoOutHandle;
	int ret;

	ret = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	assert((ret >= 0));

	videoOutHandle = ret;

	ret = sceKernelCreateEqueue(eqFlip, "eq to wait flip");
	assert((ret >= 0));

	ret = sceVideoOutAddFlipEvent(*eqFlip, videoOutHandle, NULL);
	assert((ret >= 0));


	return videoOutHandle;
}

// register display buffers
int initializeDisplayBuffer(int32_t videoOutHandle, RenderBuffer *renderBuffers, int num, int startIndex)
{

	assert(SCE_VIDEO_OUT_BUFFER_NUM_MAX >= num);
	void *addresses[SCE_VIDEO_OUT_BUFFER_NUM_MAX];
	for (int i = 0; i < num; i++) addresses[i] = renderBuffers[i].getAddress();


	SceVideoOutBufferAttribute attribute;
	renderBuffers[0].getRenderBufferAttributes()->setDisplayBufferAttributes(&attribute);


	// register the buffers to the slot: [startIndex..startIndex+num-1]
	int ret = sceVideoOutRegisterBuffers(videoOutHandle, startIndex, &addresses[0], num, &attribute);
	assert((ret >= 0));

	for (int i = 0; i < num; i++) {
		renderBuffers[i].setDisplayBufferIndex(startIndex + i); // set the buffer index to flip
	}


	return ret;
}

// allocate direct memory for display buffers
void allocateDisplayBuffers(VideoMemory *videoMem, RenderBuffer *renderBuffers, int bufferNum, RenderBufferAttributes *attr)
{
	int i;
	void *addr;

	for (i = 0; i < bufferNum; i++) {
		addr = videoMem->memAlign(attr->getSize(), attr->getAlign());
		assert((addr != NULL));
		renderBuffers[i].init(attr, addr);
	}
}

// wait until no flip is on pending
void waitFlipAllFinished(int videoOutHandle, SceKernelEqueue *eqFlip)
{

	while (sceVideoOutIsFlipPending(videoOutHandle) != 0) {
		int ret = 0;
		SceKernelEvent ev;
		int out;
		ret = sceKernelWaitEqueue(*eqFlip, &ev, 1, &out, 0);
		assert((ret >= 0));
	}

	return;
}

// wait until incremental flipArg (specified at SubmitFlip) reaches the value
void waitFlipArg(int videoOutHandle, SceKernelEqueue *eqFlip, int64_t flipArg)
{

	while (1) {
		int ret = 0;

		SceVideoOutFlipStatus status;
		ret = sceVideoOutGetFlipStatus(videoOutHandle, &status);
		assert((ret >= 0));

		if (status.flipArg >= flipArg) {
			return;
		}


		SceKernelEvent ev;
		int out;
		ret = sceKernelWaitEqueue(*eqFlip, &ev, 1, &out, 0);
		assert((ret >= 0));
	}


	return;
}

static void configureWithResolution(int handle, RenderBufferAttributes *attr)
{
	bool isTile = true;
	int width, height;

	if (sceKernelIsNeoMode()) {
		SceVideoOutResolutionStatus status;
		if (SCE_OK == sceVideoOutGetResolutionStatus(handle, &status) && status.fullHeight > 1080) {
			height = 2160;
		}
		else {
			height = 1080;
		}
	}
	else {
		height = 720;
	}
	width = height * 16 / 9;

	attr->init(width, height, isTile);
}



void ps4_video_init()
{
	int ret;

	RenderBufferAttributes attr;
	RenderBuffer renderBuffers[DISPLAY_BUFFER_NUM];
	int bufferIndex;
	SceKernelEqueue eqFlip;

	VideoMemory videoMem;
	// setup VideoOut and acquire handle
	int32_t videoOutHandle = initializeVideoOut(&eqFlip);

	configureWithResolution(videoOutHandle, &attr);

	// setup direct memory for display buffers
	videoMem.init(VIDEO_MEMORY_STACK_SIZE);

	// allocate display buffers
	allocateDisplayBuffers(&videoMem, renderBuffers, DISPLAY_BUFFER_NUM, &attr);

	// register display buffers
	int startIndex = 0;
	initializeDisplayBuffer(videoOutHandle, renderBuffers, DISPLAY_BUFFER_NUM, startIndex);



}





