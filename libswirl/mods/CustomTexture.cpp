/*
	 Copyright 2018 flyinghead
 
	 This file is part of reicast.
 
	 reicast is free software: you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation, either version 2 of the License, or
	 (at your option) any later version.
 
	 reicast is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.
 
	 You should have received a copy of the GNU General Public License
	 along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "CustomTexture.h"

#include <algorithm>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include "dirent/dirent.h"
#else
#include <dirent.h>
#endif

#include "deps/libpng/png.h"
#include "reios/reios.h"

#include "hw/pvr/Renderer_if.h"

#include "mods/mods.h"

void CustomTexture::LoaderThread()
{
	while (initialized)
	{
		TextureCacheData *texture;
		
		do {
			texture = NULL;

			work_queue_mutex.Lock();
			if (!work_queue.empty())
			{
				texture = work_queue.back();
				work_queue.pop_back();
			}
			work_queue_mutex.Unlock();
			
			if (texture != NULL)
			{
				texture->ComputeHash();
				if (texture->custom_image_data != NULL)
				{
					delete [] texture->custom_image_data;
					texture->custom_image_data = NULL;
				}
				if (!texture->dirty)
				{
					int width, height;
					u8 *image_data = LoadCustomTexture(texture->texture_hash, width, height);
					if (image_data == NULL)
					{
						image_data = LoadCustomTexture(texture->old_texture_hash, width, height);
					}
					if (image_data != NULL)
					{
						texture->custom_width = width;
						texture->custom_height = height;
						texture->custom_image_data = image_data;
					}
				}
				texture->custom_load_in_progress--;
			}

		} while (texture != NULL);
		
		wakeup_thread.Wait();
	}
}

bool CustomTexture::Init(std::string path)
{
	if (!initialized)
	{
		initialized = true;
		custom_textures_available = false;
#ifndef TARGET_NO_THREADS
		textures_path = path + "/textures";

		DIR* dir = opendir(textures_path.c_str());
		if (dir != NULL)
		{
			printf("Found custom textures directory: %s\n", textures_path.c_str());
			custom_textures_available = true;
			closedir(dir);
			loader_thread.Start();
		}
#endif
	}
	return custom_textures_available;
}

void CustomTexture::Terminate()
{
	if (initialized)
	{
		initialized = false;
		custom_textures_available = false;
		work_queue_mutex.Lock();
		work_queue.clear();
		work_queue_mutex.Unlock();
		wakeup_thread.Set();
#ifndef TARGET_NO_THREADS
		loader_thread.WaitToEnd();
#endif
	}
}

u8* CustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
	if (unknown_hashes.find(hash) != unknown_hashes.end())
		return NULL;
	std::stringstream path;
	path << textures_path << std::hex << hash << ".png";

	u8 *image_data = loadPNGData(path.str(), width, height);
	if (image_data == NULL)
		unknown_hashes.insert(hash);

	return image_data;
}

void CustomTexture::LoadCustomTextureAsync(TextureCacheData *texture_data)
{
	if (!custom_textures_available)
		return;

	texture_data->custom_load_in_progress++;
	work_queue_mutex.Lock();
	work_queue.insert(work_queue.begin(), texture_data);
	work_queue_mutex.Unlock();
	wakeup_thread.Set();
}

CustomTexture custom_texture;

static bool customtexture_detect(std::string path) {
	return custom_texture.Init(path);
}

static void customtexture_close() {
	custom_texture.Terminate();
}

static mod_handlers customtexturemod = {
	customtexture_detect,
	customtexture_close,
	nullptr
};
static bool registered = mod_handler_register(customtexturemod);
