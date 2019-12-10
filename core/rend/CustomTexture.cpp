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

#include "deps/libpng/png.h"
#include "reios/reios.h"
#include "file/file_path.h"

extern const char *retro_get_system_directory();

void CustomTexture::LoaderThread()
{
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

std::string CustomTexture::GetGameId()
{
   std::string game_id(ip_meta.product_number, sizeof(ip_meta.product_number));
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
	}
}

u8* CustomTexture::LoadCustomTexture(u32 hash, int& width, int& height)
{
	if (unknown_hashes.find(hash) != unknown_hashes.end())
		return NULL;
	std::stringstream path;
	path << textures_path << std::hex << hash << ".png";

	u8 *image_data = LoadPNG(path.str(), width, height);
	if (image_data == NULL)
		unknown_hashes.insert(hash);

	return image_data;
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

static FILE* pngfile;

static void png_cstd_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	fread(data,1, length,pngfile);
}

u8* CustomTexture::LoadPNG(const std::string& fname, int &width, int &height)
{
	const char* filename = fname.c_str();
	FILE* file = fopen(filename, "rb");
	pngfile = file;

	if (!file)
	{
		WARN_LOG(RENDERER, "Error opening %s", filename);
		return NULL;
	}

	//header for testing if it is a png
	png_byte header[8];

	//read the header
	fread(header,1,8,file);

	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png)
	{
		fclose(file);
		WARN_LOG(RENDERER, "Not a PNG file : %s", filename);
		return NULL;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
		NULL, NULL);
	if (!png_ptr)
	{
		fclose(file);
		WARN_LOG(RENDERER, "Unable to create PNG struct : %s", filename);
		return (NULL);
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		WARN_LOG(RENDERER, "Unable to create PNG info : %s", filename);
		fclose(file);
		return (NULL);
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		WARN_LOG(RENDERER, "Unable to create PNG end info : %s", filename);
		fclose(file);
		return (NULL);
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fclose(file);
		WARN_LOG(RENDERER, "Error during setjmp : %s", filename);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return (NULL);
	}

	//init png reading
	//png_init_io(png_ptr, fp);
	png_set_read_fn(png_ptr, NULL, png_cstd_read);

	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);

	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);

	//variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 twidth, theight;

	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type,
		NULL, NULL, NULL);

	//update width and height based on png info
	width = twidth;
	height = theight;

	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);

	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// Allocate the image_data as a big block, to be given to opengl
	png_byte *image_data = new png_byte[rowbytes * height];
	if (!image_data)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		WARN_LOG(RENDERER, "Unable to allocate image_data while loading %s", filename);
		fclose(file);
		return NULL;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = new png_bytep[height];
	if (!row_pointers)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		delete[] image_data;
		WARN_LOG(RENDERER, "Unable to allocate row_pointer while loading %s", filename);
		fclose(file);
		return NULL;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	for (int i = 0; i < height; ++i)
		row_pointers[height - i - 1] = image_data + i * rowbytes;

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	delete[] row_pointers;
	fclose(file);

	return image_data;
}

void CustomTexture::DumpTexture(u32 hash, int w, int h, TextureType textype, void *temp_tex_buffer)
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
	FILE *fp = fopen(path.str().c_str(), "wb");
	if (fp == NULL)
	{
		WARN_LOG(RENDERER, "Failed to open %s for writing", path.str().c_str());
		return;
	}

	u16 *src = (u16 *)temp_tex_buffer;

	png_bytepp rows = (png_bytepp)malloc(h * sizeof(png_bytep));
	for (int y = 0; y < h; y++)
	{
		rows[h - y - 1] = (png_bytep)malloc(w * 4);	// 32-bit per pixel
		u8 *dst = (u8 *)rows[h - y - 1];
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
			fclose(fp);
			free(rows[0]);
			free(rows);
			return;
		}
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);

	png_init_io(png_ptr, fp);


	// write header
	png_set_IHDR(png_ptr, info_ptr, w, h,
			 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
			 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);


	// write bytes
	png_write_image(png_ptr, rows);

	// end write
	png_write_end(png_ptr, NULL);
	fclose(fp);

	for (int y = 0; y < h; y++)
		free(rows[y]);
	free(rows);
}
