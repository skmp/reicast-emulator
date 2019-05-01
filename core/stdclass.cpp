#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include "types.h"
#include "cfg/cfg.h"
#include "stdclass.h"


#if COMPILER_VC_OR_CLANG_WIN32
	#include <io.h>
	#include <direct.h>
	#define access _access
	#define R_OK   4
#else
	#include <unistd.h>
#endif

#include "hw/mem/_vmem.h"

string user_config_dir;
string user_data_dir;
std::vector<string> system_config_dirs;
std::vector<string> system_data_dirs;

bool file_exists(const string& filename)
{
	return (access(filename.c_str(), R_OK) == 0);
}

void set_user_config_dir(const string& dir)
{
	user_config_dir = dir;
}

void set_user_data_dir(const string& dir)
{
	user_data_dir = dir;
}

void add_system_config_dir(const string& dir)
{
	system_config_dirs.push_back(dir);
}

void add_system_data_dir(const string& dir)
{
	system_data_dirs.push_back(dir);
}

string get_writable_config_path(const string& filename)
{
	/* Only stuff in the user_config_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_config_dir + filename);
}

string get_readonly_config_path(const string& filename)
{
	string user_filepath = get_writable_config_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	string filepath;
	for (unsigned int i = 0; i < system_config_dirs.size(); i++) {
		filepath = system_config_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

string get_writable_data_path(const string& filename)
{
	/* Only stuff in the user_data_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_data_dir + filename);
}

string get_readonly_data_path(const string& filename)
{
	string user_filepath = get_writable_data_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	string filepath;
	for (unsigned int i = 0; i < system_data_dirs.size(); i++) {
		filepath = system_data_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

string get_game_save_prefix()
{
	string save_file = cfgLoadStr("config", "image", "");
	size_t lastindex = save_file.find_last_of("/");
#ifdef _WIN32
	size_t lastindex2 = save_file.find_last_of("\\");
	lastindex = max(lastindex, lastindex2);
#endif
	if (lastindex != -1)
		save_file = save_file.substr(lastindex + 1);
	return get_writable_data_path("/data/") + save_file;
}

string get_game_basename()
{
	string game_dir = cfgLoadStr("config", "image", "");
	size_t lastindex = game_dir.find_last_of(".");
	if (lastindex != -1)
		game_dir = game_dir.substr(0, lastindex);
	return game_dir;
}

string get_game_dir()
{
	string game_dir = cfgLoadStr("config", "image", "");
	size_t lastindex = game_dir.find_last_of("/");
#ifdef _WIN32
	size_t lastindex2 = game_dir.find_last_of("\\");
	lastindex = max(lastindex, lastindex2);
#endif
	if (lastindex != -1)
		game_dir = game_dir.substr(0, lastindex + 1);
	return game_dir;
}

bool make_directory(const string& path)
{
#if COMPILER_VC_OR_CLANG_WIN32
#define mkdir _mkdir
#endif

#ifdef _WIN32
	return mkdir(path.c_str()) == 0;
#else
	return mkdir(path.c_str(), 0755) == 0;
#endif
}

// Thread & related platform dependant code
#if !defined(HOST_NO_THREADS)

#if HOST_OS==OS_WINDOWS
void cThread::Start() {
	verify(hThread == NULL);
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entry, param, 0, NULL);
	ResumeThread(hThread);
}
void cThread::WaitToEnd() {
	WaitForSingleObject(hThread,INFINITE);
	CloseHandle(hThread);
	hThread = NULL;
}
#else
void cThread::Start() {
	verify(hThread == NULL);
	hThread = new pthread_t;
	if (pthread_create( hThread, NULL, entry, param))
		die("Thread creation failed");
}
void cThread::WaitToEnd() {
	if (hThread) {
		pthread_join(*hThread,0);
		delete hThread;
		hThread = NULL;
	}
}
#endif

#endif


#if HOST_OS==OS_WINDOWS
cResetEvent::cResetEvent() {
		hEvent = CreateEvent(
		NULL,             // default security attributes
		FALSE,            // auto-reset event?
		FALSE,            // initial state is State
		NULL			  // unnamed object
		);
}
cResetEvent::~cResetEvent()
{
	//Destroy the event object ?
	 CloseHandle(hEvent);
}
void cResetEvent::Set()//Signal
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	SetEvent(hEvent);
}
void cResetEvent::Reset()//reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	ResetEvent(hEvent);
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	return WaitForSingleObject(hEvent,msec) == WAIT_OBJECT_0;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	WaitForSingleObject(hEvent,(u32)-1);
}
#else
cResetEvent::cResetEvent() {
	pthread_mutex_init(&mutx, NULL);
	pthread_cond_init(&cond, NULL);
}
cResetEvent::~cResetEvent() {
}
void cResetEvent::Set()//Signal
{
	pthread_mutex_lock( &mutx );
	state=true;
    pthread_cond_signal( &cond);
	pthread_mutex_unlock( &mutx );
}
void cResetEvent::Reset()//reset
{
	pthread_mutex_lock( &mutx );
	state=false;
	pthread_mutex_unlock( &mutx );
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	pthread_mutex_lock( &mutx );
	if (!state)
	{
		struct timespec ts;
#if HOST_OS == OS_DARWIN
		// OSX doesn't have clock_gettime.
		clock_serv_t cclock;
		mach_timespec_t mts;

		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		ts.tv_sec = mts.tv_sec;
		ts.tv_nsec = mts.tv_nsec;
#else
		clock_gettime(CLOCK_REALTIME, &ts);
#endif
		ts.tv_sec += msec / 1000;
		ts.tv_nsec += (msec % 1000) * 1000000;
		while (ts.tv_nsec > 1000000000)
		{
			ts.tv_nsec -= 1000000000;
			ts.tv_sec++;
		}
		pthread_cond_timedwait( &cond, &mutx, &ts );
	}
	bool rc = state;
	state=false;
	pthread_mutex_unlock( &mutx );

	return rc;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	pthread_mutex_lock( &mutx );
	if (!state)
	{
		pthread_cond_wait( &cond, &mutx );
	}
	state=false;
	pthread_mutex_unlock( &mutx );
}
#endif



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/* This has mostly been used for PS4, and now for debug purposes */

#if defined(CUSTOM_ALLOCATOR) && !defined(TARGET_PS4)






static size_t maxheap=MB(512), heapsize=0;
static void	*heapbase=nullptr;



void initheap()
{

	heapbase = _aligned_malloc(maxheap, PAGE_SIZE);
	if(!heapbase) die("initheap() : malloc() failed! \n");
	
	printf("@@@@@@@@@@@@@@@ initheap w. address: %p ####################\n", heapbase);
}

char * sbrk(int incr)
{
    char *ret;
    
    if ((heapsize + incr) <= maxheap) {
		ret = (char *)heapbase + heapsize;
	//	bzero(ret, incr);
		memset(ret,0,incr);
		heapsize += incr;
		return(ret);
    }
    errno = ENOMEM;
    return((char *)-1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




struct janitor_block {
    size_t size;
    struct janitor_block *next;
    int flag;
};

typedef struct janitor_block janitor_t;

#define BLOCK_SIZE sizeof(janitor_t)



/* keep a head and tail block */
janitor_t *head, *tail;

/* implement mutex lock to prevent races */
cMutex global_malloc_lock;




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
	global_malloc_lock.Lock();	//   pthread_mutex_lock(&global_malloc_lock);

    /* first-fit: check if there already is a block size that meets our allocation size and immediately fill it and return */
    header = get_free_block(size);
    if (header) {
        header->flag = 0;
        global_malloc_lock.Unlock();  //pthread_mutex_unlock(&global_malloc_lock);
        return (void *) (header + 1);
    }

    /* if not found, continue by extending the size of the heap with sbrk, extending the break to meet our allocation */
    total_size = BLOCK_SIZE + size;
    block = sbrk(total_size);
    if (block == (void *) -1 ) {
        global_malloc_lock.Unlock();  //pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    /* set struct entries with allocation specification and mark as not free */
    header = (janitor_t*)block;
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
    global_malloc_lock.Unlock();  //pthread_mutex_unlock(&global_malloc_lock);

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


void
jfree(void *block)
{
    janitor_t *header, *tmp;
    void * programbreak;

    /* if the block is provided */
    if (!block)
        return (void) NULL;

    /* start critical section */
    global_malloc_lock.Lock();  //pthread_mutex_lock(&global_malloc_lock);

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
        global_malloc_lock.Unlock();  //pthread_mutex_unlock(&global_malloc_lock);

        /* returns nothing */
        return (void) NULL;
    }

    /* set flag to unmarked */
    header->flag = 1;

    /* unlock critical section */
    global_malloc_lock.Unlock();  //pthread_mutex_unlock(&global_malloc_lock);
}


void *
jrealloc(void *block, size_t size)
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








extern "C" void* zmalloc(unsigned long size)
{
	if (nullptr==heapbase) initheap();

	return jmalloc(size);
}

extern "C" void* z_calloc(size_t nelem, size_t size)	// libz has a zcalloc / global ns clash
{
	if (nullptr==heapbase) initheap();

	return jcalloc(nelem, size);
}


extern "C" void* zrealloc(void* ptr, unsigned long size)
{
	if (nullptr==heapbase) initheap();

	return jrealloc(ptr, size);
}

extern "C" int   zmemalign(void **ptr, unsigned long alignment, unsigned long size)
{
	if (nullptr==heapbase) initheap();



	// *FIXME*

	*ptr=nullptr;
	return 0; //jmemalign(msp, ptr, alignment, size);

}

extern "C" void  zfree(void* ptr)
{
	////////// *FIXME* !!!!!!!!!!!!!!!!
	/// this is a hack !

	if ( ((unat)ptr >= (unat)heapbase) && ((unat)ptr < ((unat)heapbase + maxheap)) )
		zpf(" >>> zfree not in heap !!! @ %p !! \n", ptr);


	//else free(ptr);	//// WHY can't this just work ?! - 
}


void* operator new(size_t size)
{
	return zmalloc(size);
}

void operator delete(void* ptr)
{
	if ( ((unat)ptr < (unat)heapbase) || ((unat)ptr >= ((unat)heapbase + maxheap)) )
		zpf(" >>> IS DELETE @ %p !! \n", ptr);

	zfree(ptr);
}













#endif // defined(CUSTOM_ALLOCATOR)