/*
	This file is part of libswirl
*/
#include "license/bsd"



#ifndef CORE_REND_GLES_CUSTOMTEXTURE_H_
#define CORE_REND_GLES_CUSTOMTEXTURE_H_

#include <string>
#include <set>
#include "gles.h"

class CustomTexture {
public:
	CustomTexture()
		:
		loader_thread(loader_thread_func, this)
		{}
	~CustomTexture() { Terminate(); }
	u8* LoadCustomTexture(u32 hash, int& width, int& height);
	void LoadCustomTextureAsync(TextureCacheData *texture_data);
	void DumpTexture(u32 hash, int w, int h, GLuint textype, void *temp_tex_buffer);

private:
	bool Init();
	void Terminate();
	void LoaderThread();
	std::string GetGameId();
	
	static void *loader_thread_func(void *param) { ((CustomTexture *)param)->LoaderThread(); return NULL; }
	
	bool initialized;
	bool custom_textures_available;
	std::string textures_path;
	std::set<u32> unknown_hashes;
	cThread loader_thread;
	cResetEvent wakeup_thread;
	std::vector<TextureCacheData *> work_queue;
	cMutex work_queue_mutex;
};

#endif /* CORE_REND_GLES_CUSTOMTEXTURE_H_ */
