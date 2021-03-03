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
#include <sys/stat.h>
#include "retro_dirent.h"
#include "string/stdstring.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "reios/reios.h"
#include "file/file_path.h"
#include "hw/naomi/naomi_cart.h"

extern const char *retro_get_system_directory();

CustomTexture custom_texture;

void CustomTexture::LoaderThread()
{
	LoadMap();
	while (initialized)
	{
		BaseTextureCacheData *texture;
		
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
					free(texture->custom_image_data);
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

std::string CustomTexture::GetGameId()
{
   std::string game_id;
	if (settings.System == DC_PLATFORM_DREAMCAST)
	   game_id = std::string(ip_meta.product_number, sizeof(ip_meta.product_number));
	else
		game_id = naomi_game_id;
   const size_t str_end = game_id.find_last_not_of(' ');
   if (str_end == std::string::npos)
	   return "";
   game_id = game_id.substr(0, str_end + 1);
   std::replace(game_id.begin(), game_id.end(), ' ', '_');

   return game_id;
}

bool CustomTexture::Init()
{
	if (!initialized)
	{
		initialized = true;
		std::string game_id = GetGameId();
		if (game_id.length() > 0)
		{
			textures_path = std::string(retro_get_system_directory()) + "/dc/textures/"
					+ game_id + "/";

			if (path_is_directory(textures_path.c_str()))
			{
				INFO_LOG(RENDERER, "Found custom textures directory: %s", textures_path.c_str());
				custom_textures_available = true;
				loader_thread.Start();
			}
		}
	}
	return custom_textures_available;
}

void CustomTexture::Terminate()
{
	if (initialized)
	{
		initialized = false;
		work_queue_mutex.Lock();
		work_queue.clear();
		work_queue_mutex.Unlock();
		wakeup_thread.Set();
		loader_thread.WaitToEnd();
		texture_map.clear();
	}
}

u8* CustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
	auto it = texture_map.find(hash);
	if (it == texture_map.end())
		return nullptr;

	int n;
	stbi_set_flip_vertically_on_load(1);
	return stbi_load(it->second.c_str(), &width, &height, &n, STBI_rgb_alpha);
}

void CustomTexture::LoadCustomTextureAsync(BaseTextureCacheData *texture_data)
{
	if (!Init())
		return;

	texture_data->custom_load_in_progress++;
	work_queue_mutex.Lock();
	work_queue.insert(work_queue.begin(), texture_data);
	work_queue_mutex.Unlock();
	wakeup_thread.Set();
}

void CustomTexture::DumpTexture(u32 hash, int w, int h, TextureType textype, void *src_buffer)
{
	std::string base_dump_dir = get_writable_data_path("/texdump/");
	if (!path_is_valid(base_dump_dir.c_str()))
		path_mkdir(base_dump_dir.c_str());
	std::string game_id = GetGameId();
	if (game_id.length() == 0)
	   return;

	base_dump_dir += game_id + "/";
	if (!path_is_valid(base_dump_dir.c_str()))
		path_mkdir(base_dump_dir.c_str());

	std::stringstream path;
	path << base_dump_dir << std::hex << hash << ".png";

	u16 *src = (u16 *)src_buffer;
	u8 *dst_buffer = (u8 *)malloc(w * h * 4);	// 32-bit per pixel
	u8 *dst = dst_buffer;

	for (int y = 0; y < h; y++)
	{
		switch (textype)
		{
		case TextureType::_4444:
			for (int x = 0; x < w; x++)
			{
				*dst++ = ((*src >> 12) & 0xF) << 4;
				*dst++ = ((*src >> 8) & 0xF) << 4;
				*dst++ = ((*src >> 4) & 0xF) << 4;
				*dst++ = (*src & 0xF) << 4;
				src++;
			}
			break;
		case TextureType::_565:
			for (int x = 0; x < w; x++)
			{
				*dst++ = ((*src >> 11) & 0x1F) << 3;
				*dst++ = ((*src >> 5) & 0x3F) << 2;
				*dst++ = (*src & 0x1F) << 3;
				*dst++ = 255;
				src++;
			}
			break;
		case TextureType::_5551:
			for (int x = 0; x < w; x++)
			{
				*dst++ = ((*src >> 11) & 0x1F) << 3;
				*dst++ = ((*src >> 6) & 0x1F) << 3;
				*dst++ = ((*src >> 1) & 0x1F) << 3;
				*dst++ = (*src & 1) ? 255 : 0;
				src++;
			}
			break;
		case TextureType::_8888:
			for (int x = 0; x < w; x++)
			{
				*(u32 *)dst = *(u32 *)src;
				dst += 4;
				src += 2;
			}
			break;
		default:
			WARN_LOG(RENDERER, "dumpTexture: unsupported picture format %x", (u32)textype);
			free(dst_buffer);
			return;
		}
	}

	stbi_flip_vertically_on_write(1);
	stbi_write_png(path.str().c_str(), w, h, STBI_rgb_alpha, dst_buffer, 0);

	free(dst_buffer);
}

void CustomTexture::LoadMap()
{
	texture_map.clear();
	RDIR *dir = retro_opendir(textures_path.c_str());
	if (dir == nullptr)
		return;
	while (true)
	{
		if (!retro_readdir(dir))
			break;
		if (retro_dirent_is_dir(dir, nullptr))
				continue;

		std::string name = retro_dirent_get_name(dir);
		std::string child_path = textures_path + name;

		const char *extension_ro = path_get_extension(name.c_str());
		char extension[5];
		strncpy(extension, extension_ro, 4);
		extension[4] = '\0';
		string_to_lower(extension);
		if (strcmp(extension, "jpg") && strcmp(extension, "jpeg") && strcmp(extension, "png"))
			continue;

		std::string::size_type dotpos = name.find_last_of('.');
		std::string basename = name.substr(0, dotpos);
		char *endptr;
		u32 hash = (u32)strtoll(basename.c_str(), &endptr, 16);
		if (endptr - basename.c_str() < (ptrdiff_t)basename.length())
		{
			INFO_LOG(RENDERER, "Invalid hash %s", basename.c_str());
			continue;
		}
		texture_map[hash] = child_path;
	}
	retro_closedir(dir);
	custom_textures_available = !texture_map.empty();
}
