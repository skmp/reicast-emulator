/*
**	reicast: ps4_main.cpp
*/
#include "types.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_cfg.h"

#if 1 // defined(TARGET_PS4) || BUILD_OS==OS_PS4_BSD


#include <kernel_ex.h>
#include <sysmodule_ex.h>
#include <system_service_ex.h>
#include <shellcore_util.h>
#include <piglet.h>
#include <pad.h>
#include <audioout.h>

#include <kernel.h>
#include <video_out.h>
#include <user_service.h>
#include <system_service.h>
#include <gnm.h>


using namespace sce;


#include <sys/cdefs.h>
#include <sys/dmem.h>





SceWindow render_window = { 0, 1920,1080 }; // width, height };
void* libPvr_GetRenderTarget() { return &render_window; }
void* libPvr_GetRenderSurface() { return (void*)EGL_DEFAULT_DISPLAY; }



int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }



void common_linux_setup();
int reicast_init(int argc, char* argv[]);
void *rend_thread(void *);

void dc_term();
void dc_stop();


void ps4_module_init();


int main(int argc, char* argv[])
{
	printf("-- REICAST BETA -- \n");
	//atexit(&cleanup);

	ps4_module_init();


	printf("common_setup()\n");

	/* Set directories */
	set_user_data_dir(PS4_DIR_DATA);
	set_user_config_dir(PS4_DIR_CFG);

	printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
	printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());
	printf("RO Data dir is:   %s\n", get_readonly_data_path("/").c_str());

	common_linux_setup();



	settings.profile.run_counts = 0;

	printf("dc_init();\n");

	if (reicast_init(argc, argv) != 0)
		die("Reicast initialization failed");

	
	printf("rend_thread();\n");

	rend_thread(NULL);

	printf("dc_term();\n");
	dc_term();




err:
	return 0;
}


extern "C" void catchReturnFromMain(int exit_code)
{
	printf("ReturnFromMain(%d)\n",exit_code);
	for(;1;) sceKernelSleep(1);
}



void os_CreateWindow() { }

void os_DoEvents()
{
	int32_t rS = SCE_OK;
	SceSystemServiceEvent sse; // = { 0 };
	SceSystemServiceStatus sss = { 0 };


	memset(&sss, 0, sizeof(SceSystemServiceStatus));
	rS = sceSystemServiceGetStatus(&sss);
	if (SCE_OK != rS)
		printf("os_DoEvents(): sceSystemServiceGetStatus() Failed!");


	/*
	**	Polling is really the only method of finding out we're supposed to stop?
	**		Fucked up when the emulator destroys the OS if you don't stop...
	*/
	if (sss.isInBackgroundExecution)
	{
		printf("os_DoEvents(): ! STOP \n");
		dc_stop();
		printf("os_DoEvents(): STOP ! \n");
	}

	do {
		/*
			eventNum					Number of events that can be received with sceSystemServiceReceiveEvent()
			isSystemUiOverlaid			Flag indicating whether the system software UI is being overlaid or not
			isInBackgroundExecution		Flag indicating whether the application is running in the background or not
			isCpuMode7CpuNormal			Flag indicating whether the CPU mode is 7CPU(NORMAL) mode or not
			isGameLiveStreamingOnAir	Flag indicating whether live streaming is currently being performed or not
		*/


		memset(&sse, 0, sizeof(SceSystemServiceEvent));
		rS = sceSystemServiceReceiveEvent(&sse);
		if (SCE_OK == rS) {

			if (SCE_SYSTEM_SERVICE_EVENT_ON_RESUME == sse.eventType)
				printf("SCE EVENT - RESUME!\n"); // dc_run()
			else
				printf("SCE EVENT - Type: %08X \n", sse.eventType);
		}

		if (sss.eventNum-- < 0) {
			printf("SCE EVENT - OVERFLOW? \n"); break;
		}
	} while (SCE_OK == rS);



}







extern SceUserServiceUserId userId;

int32_t pad1_H = 0;



u16 kcode[4];
u32 vks[4];
s8 joyx[4], joyy[4];
u8 rt[4], lt[4];

#define Btn_C            (1 << 0)
#define Btn_B            (1 << 1)
#define Btn_A            (1 << 2)
#define Btn_START        (1 << 3)
#define Btn_DPAD_UP      (1 << 4)
#define Btn_DPAD_DOWN    (1 << 5)
#define Btn_DPAD_LEFT    (1 << 6)
#define Btn_DPAD_RIGHT   (1 << 7)
#define Btn_Z            (1 << 8)
#define Btn_Y            (1 << 9)
#define Btn_X            (1 << 10)
#define Btn_D            (1 << 11)
#define Btn_DPAD2_UP     (1 << 12)
#define Btn_DPAD2_DOWN   (1 << 13)
#define Btn_DPAD2_LEFT   (1 << 14)
#define Btn_DPAD2_RIGHT  (1 << 15)


void os_SetupInput() {

	mcfg_CreateDevices();

	verify(SCE_OK == scePadInit());
	pad1_H = scePadOpen(userId, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);

	printf(" --UserId: %X\n", userId);
	printf(" --pad1_H: %X\n", pad1_H);


#if 1 // FUCK_YOU_SONY
	if (pad1_H >= 0) printf("pad1_H: %X\n", pad1_H); return;  // Winner Winner

	SceUserServiceLoginUserIdList userIdList;
	verify(SCE_OK == sceUserServiceGetLoginUserIdList(&userIdList));

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++) {
		if (userIdList.userId[i] != SCE_USER_SERVICE_USER_ID_INVALID) {
			pad1_H = scePadOpen(userIdList.userId[i], SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
			if (pad1_H >= 0) printf("Fixed pad1_H: %X\n", pad1_H); return;  // Winner Winner
		}
	}

	printf("Error, No DS4 or input pad found!\n");
#endif
}





void UpdateInputState(u32 port)
{
	ScePadData data;
	
	int ret = scePadReadState(pad1_H, &data);
	if (ret != SCE_OK || !data.connected) {
		printf("Warning, Controller is not connected or failed to read data! \n");
		return;
	}

	if (port != 0) return;	// *FIXME* multiplayer

	data.buttons;
	kcode[port] = 0xFFFF;
	joyx[port] = joyy[port] = 0;
	lt[port] = rt[port] = 0;


	if (data.buttons&SCE_PAD_BUTTON_CROSS)		kcode[port] &= ~Btn_A;
	if (data.buttons&SCE_PAD_BUTTON_CIRCLE)		kcode[port] &= ~Btn_B;
	if (data.buttons&SCE_PAD_BUTTON_SQUARE)		kcode[port] &= ~Btn_X;
	if (data.buttons&SCE_PAD_BUTTON_TRIANGLE)	kcode[port] &= ~Btn_Y;
	if (data.buttons&SCE_PAD_BUTTON_OPTIONS)	kcode[port] &= ~Btn_START;

	if (data.buttons&SCE_PAD_BUTTON_UP)			kcode[port] &= ~Btn_DPAD_UP;
	if (data.buttons&SCE_PAD_BUTTON_DOWN)		kcode[port] &= ~Btn_DPAD_DOWN;
	if (data.buttons&SCE_PAD_BUTTON_LEFT)		kcode[port] &= ~Btn_DPAD_LEFT;
	if (data.buttons&SCE_PAD_BUTTON_RIGHT)		kcode[port] &= ~Btn_DPAD_RIGHT;


	lt[port] = (data.buttons&SCE_PAD_BUTTON_L1) ? 255 : data.analogButtons.l2;
	rt[port] = (data.buttons&SCE_PAD_BUTTON_R1) ? 255 : data.analogButtons.r2;


	// Sticks are u8 !
	s8 lsX = data.leftStick.x - 128; lsX = (lsX < 0) ? std::min(lsX, (s8)-8) : std::max(lsX, (s8)7);
	s8 lsY = data.leftStick.y - 128; lsY = (lsY < 0) ? std::min(lsY, (s8)-8) : std::max(lsY, (s8)7);

	joyx[port] = lsX;
	joyy[port] = lsY;

	s8 rsX = data.rightStick.x - 128; rsX = (rsX < 0) ? std::min(rsX, (s8)-8) : std::max(rsX, (s8)7);
	s8 rsY = data.rightStick.y - 128; rsY = (rsY < 0) ? std::min(rsY, (s8)-8) : std::max(rsY, (s8)7);

	if (rsY < -120)		kcode[port] &= ~Btn_DPAD2_UP;
	if (rsY > 119)		kcode[port] &= ~Btn_DPAD2_DOWN;
	if (rsX < -120)		kcode[port] &= ~Btn_DPAD2_LEFT;
	if (rsX > 119)		kcode[port] &= ~Btn_DPAD2_RIGHT;

}

void UpdateVibration(u32 port, u32 value)
{
	ScePadVibrationParam vibParam;
	vibParam.largeMotor = (uint8_t)value;
	vibParam.smallMotor = 0;

	scePadSetVibration(pad1_H, &vibParam);
}






#define USE_CUSTOM_ALLOCATOR 1
#if USE_CUSTOM_ALLOCATOR //defined(TARGET_PS4)		

// *TODO* see if we can use MSpace to get the 512mb heap while using libcInternal
//		-- this code is meant for testing purposes ONLY, it never free's data and will overflow !


#if 0	// Evidently LibcInternal doesn't handle these either ... fml

extern "C" {

	int user_malloc_init(void) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	int user_malloc_finalize(void) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_malloc(size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void user_free(void *ptr) { printf("VM @@@@ %s", __FUNCTION__); }

	void *user_calloc(size_t nelem, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_realloc(void *ptr, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_memalign(size_t boundary, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	void *user_reallocalign(void *ptr, size_t size, size_t boundary) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	int user_posix_memalign(void **ptr, size_t boundary, size_t size) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	int user_malloc_stats(SceLibcMallocManagedSize *mmsize) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }
	int user_malloc_stats_fast(SceLibcMallocManagedSize *mmsize) { printf("VM @@@@ %s", __FUNCTION__);  return 0; }

	size_t user_malloc_usable_size(void *ptr)
	{
		printf("VM @@@@ %s", __FUNCTION__);
		return 1 << 28;
	}


};

#endif







	/// Actual Implementation //



/* defines a block of memory for allocator. */
struct janitor_block {
    size_t size;
    struct janitor_block *next;
    int flag;
};

typedef struct janitor_block janitor_t;

#define BLOCK_SIZE sizeof(janitor_t)





off_t phyAddr = 0;



static size_t	maxheap=MB(512), heapsize = 0;
static void	*heapbase=nullptr;

void setheap(void *base, void *top)
{
    /* Align start address to 16 bytes for the malloc code. Sigh. */
    heapbase = (void *)(((uintptr_t)base + 15) & ~15);
    maxheap = (char *)top - (char *)heapbase;
}

void initheap()
{
	int32_t res = SCE_OK;
	if (SCE_OK != (res = sceKernelAllocateMainDirectMemory(MB(512), KB(64), SCE_KERNEL_WB_ONION, &phyAddr))) {
		die("sceKernelAllocateMainDirectMemory() Failed"); // with 0x%08X \n", (unat)res);
		return;
	}

	if (SCE_OK != (res = sceKernelMapDirectMemory(&heapbase,MB(512),SCE_KERNEL_PROT_CPU_RW,0,phyAddr,KB(64)))) {
		die("sceKernelMapDirectMemory() Failed"); // with 0x%08X \n", (unat)res);
		return;
	}
	
	printf("@@@@@@@@@@@@@@@ Z HEAP INIT - %p ####################\n", heapbase);
	//setheap() not needed
}

char * sbrk(int incr)
{
	zpf("<< sbrk(%d) >>\n", incr);
    char	*ret;
    
	if(!heapbase)
		initheap();

    if ((heapsize + incr) <= maxheap) {
		ret = (char *)heapbase + heapsize;
		bzero(ret, incr);
		heapsize += incr;
		return(ret);
    }
    errno = ENOMEM;
    return((char *)-1);
}




/* keep a head and tail block */
janitor_t *head, *tail;

/* implement mutex lock to prevent races */
pthread_mutex_t global_malloc_lock;


static janitor_t * get_free_block(size_t size)
{
    /* set head block as current block */
    janitor_t *current = head;
    while (current) {

        /* check if block is marked free and if ample space is provided for allocation */
        if (current->flag && current->size >= size)
            return current;

        /* check next block */
        current = current->next;
    }
    return NULL;
}


void * jmalloc(size_t size)
{
    size_t total_size;
    void *block;
    janitor_t *header;

    /* error-check size */
    if (!size)
        return NULL;

    /* critical section start */
    pthread_mutex_lock(&global_malloc_lock);

    /* first-fit: check if there already is a block size that meets our allocation size and immediately fill it and return */
    header = get_free_block(size);
    if (header) {
        header->flag = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *) (header + 1);
    }

    /* if not found, continue by extending the size of the heap with sbrk, extending the break to meet our allocation */
    total_size = BLOCK_SIZE + size;
    block = sbrk(total_size);
    if (block == (void *) -1 ) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    /* set struct entries with allocation specification and mark as not free */
    header = (janitor_t *)block;
    header->size = size;
    header->flag = 0;
    header->next = NULL;

    /* switch context to next free block
     * - if there is no head block for the list, set header as head
     * - if a tail block is present, set the next element to point to header, now the new tail
     */
    if (!head)
        head = header;
    if (tail)
        tail->next = header;

    tail = header;

    /* unlock critical section */
    pthread_mutex_unlock(&global_malloc_lock);

    /* returned memory after break */
    return (void *)(header + 1);
}


void * jcalloc(size_t num, size_t size)
{
    size_t total_size;
    void *block;

    /* check if parameters were provided */
    if (!num || !size)
        return (void *) NULL;

    /* check if size_t bounds adhere to multiplicative inverse properties */
    total_size = num * size;
    if (size != total_size / num)
        return (void *) NULL;

    /* perform conventional malloc with total_size */
    block = jmalloc(total_size);
    if (!block)
        return (void *) NULL;

    /* zero out our newly heap allocated block */
    memset(block, 0, size);
    return block;
}


void jfree(void *block)
{
    janitor_t *header, *tmp;
    void * programbreak;

    /* if the block is provided */
    if (!block)
        return (void) NULL;

    /* start critical section */
    pthread_mutex_lock(&global_malloc_lock);

    /* set header as previous block */
    header = (janitor_t *) block - 1;

    /* start programbreak at byte 0 */
    programbreak = sbrk(0);

   /* start to NULL out block until break point of heap
    * NOTE: header (previous block) size + target block should meet
    */
    if (( char *) block + header->size == programbreak){

        /* check if block is only allocated block (since it is both head and tail), and NULL */
        if (head == tail)
            head = tail = NULL;
        else {

            /* copy head into tmp block, NULL each block from tail back to head */
            tmp = head;
            while (tmp) {
                if (tmp->next == tail){
                    tmp->next = NULL;
                    tail = tmp;
                }
                tmp = tmp->next;
            }
        }

        /* move break back to memory address after deallocation */
        sbrk(0 - BLOCK_SIZE - header->size);

        /* unlock critical section*/
        pthread_mutex_unlock(&global_malloc_lock);

        /* returns nothing */
        return (void) NULL;
    }

    /* set flag to unmarked */
    header->flag = 1;

    /* unlock critical section */
    pthread_mutex_unlock(&global_malloc_lock);
}


void * jrealloc(void *block, size_t size)
{
    janitor_t *header;
    void *ret;

    /* create a new block if parameters not set */
    if (!block)
        return jmalloc(size);

    /* set header to be block's previous bit */
    header = (janitor_t *) block - 1;

    /* check if headers size is greater than specified paramater */
    if (header->size >= size)
        return block;

    /* create a new block allocation */
    ret = jmalloc(size);

    /* add content from previous block to newly allocated block */
    if (ret) {
        memcpy(ret, block, header->size);
        jfree(block);
    }
    return ret;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////



extern "C" void* zmalloc(unsigned long size)
{
#if 1
	void *res = jmalloc(size);

	zpf("zmalloc(%d) got %p\n", size, res);
	return res;

#else
	if(nullptr==pHeap)
	{
		int32_t res = SCE_OK;
		if (SCE_OK != (res = sceKernelAllocateMainDirectMemory(MB(512), KB(64), SCE_KERNEL_WB_ONION, &phyAddr))) {
			printf("sceKernelAllocateMainDirectMemory() Failed with 0x%08X \n", (unat)res);
			return nullptr;
		}

		if (SCE_OK != (res = sceKernelMapDirectMemory(&pHeap,MB(512),SCE_KERNEL_PROT_CPU_RW,0,phyAddr,KB(64)))) {
			printf("sceKernelMapDirectMemory() Failed with 0x%08X \n", (unat)res);
			return nullptr;
		}

		printf("@@@@@@@@@@@@@@@ Z HEAP INIT - %p ####################\n",pHeap);
	}


	void* res = (void*)((unat)pHeap+sbrk);

	sbrk+=size;

	if (sbrk>MB(500)) die("BS Allocator ran out of real estate!");

//	zpf("zmalloc(%d) @ %p :: total %d\n", size, res, sbrk);

	return res;
#endif
}

extern "C" void* zrealloc(void* ptr, unsigned long size)
{
#if 1
	return jrealloc(ptr,size);
#else
	void *pre = zmalloc(size);
	memcpy(ptr,pre,size);		// this is fucked but should work ok unless we hit the end....
#endif
}

extern "C" int zmemalign(void **ptr, unsigned long alignment, unsigned long size)
{
	if(nullptr==ptr || alignment<sizeof(void*))
		return EINVAL;

	// *FIXME* 

	unat _addr = (unat)sbrk(0);
	unat diff = AlignUp(_addr, alignment) - _addr;
	_addr = (unat)jmalloc(size+diff);
	*ptr = (void*)AlignUp((unat)_addr, alignment);

	
	zpf("zmemalign(%d,%d) w. diff: %d , got %p \n", alignment, size, diff, *ptr);

	return 0;


//	heapsize=AlignUp(heapsize,alignment);
//	*ptr = jmalloc(size);
//	if(*ptr == nullptr)
//		return ENOMEM;
//
//	return 0;
}

extern "C" void  zfree(void* ptr)
{
	zpf("zfree(%p)\n", ptr);
	jfree(ptr);
	zpf("zfree -finished\n");
}

extern "C" void  zfree2(void* ptr, unsigned long size)
{
	zpf("zfree2(%p, %d)\n", ptr, size);
#if 1
	jfree(ptr);	// *FIXME* ??
#else
	const uint32_t psMask = (0x4000 - 1);
	int32_t mem_size = (size + psMask) & ~psMask;
	sceKernelMunmap(ptr, mem_size);	// who cares atm, fucking choke on it

	zmtotal -= mem_size;
#endif
}




void* operator new(size_t size)
{
	return zmalloc(size);
}

void operator delete(void* p)
{
	zfree(p);
}




#if 0
zmemalign(16384,135,266,304)	w. diff: 11720 , got 2`0000`c000
zmemalign(16384,16,777,216)		w. diff: 16360 , got 2`0811`0000
zmemalign(16384,8,388,608)		w. diff: 16360 , got 2`0911`4000
zmemalign(16384,2,097,152)		w. diff: 16360 , got 2`0991`8000

zmalloc(112) got 2`09b1`8030
zmalloc(112) got 2`09b1`80b8
zmalloc(112) got 2`09b1`8140

zfree(2`09b1`8140)
#endif

















#endif // USE_CUSTOM_ALLOCATOR




#endif // TARGET_PS4
