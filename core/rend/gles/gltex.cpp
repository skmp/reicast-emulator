#include <math.h>
#include <map>

#include <memalign.h>

#include <algorithm>

#include <glsm/glsm.h>
#include <glsm/glsmsym.h>

#ifdef __SSE4_1__
#include <xmmintrin.h>
#endif

#include <libretro.h>

#include "gles.h"
#include "../rend.h"
#include "../TexCache.h"
#include "glcache.h"

#include "../../hw/pvr/pvr_mem.h"
#include "../../hw/mem/_vmem.h"

#ifndef GL_IMPLEMENTATION_COLOR_READ_TYPE
#define GL_IMPLEMENTATION_COLOR_READ_TYPE 0x8B9A
#endif

#ifndef GL_IMPLEMENTATION_COLOR_READ_FORMAT
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#endif

/*
Textures

Textures are converted to native OpenGL textures
The mapping is done with tcw:tsp -> GL texture. That includes stuff like
filtering/ texture repeat

To save space native formats are used for 1555/565/4444 (only bit shuffling is done)
YUV is converted to 8888
PALs are decoded to their unpaletted format (5551/565/4444/8888 depending on palette type)

Compression
	look into it, but afaik PVRC is not realtime doable
*/

extern u32 decoded_colors[3][65536];
extern "C" struct retro_hw_render_callback hw_render;

typedef void TexConvFP(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
typedef void TexConvFP32(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);

struct PvrTexInfo
{
	const char* name;
   int bpp;        //4/8 for pal. 16 for yuv, rgb, argb
	GLuint type;
   // Conversion to 16 bpp
	TexConvFP *PL;
	TexConvFP *TW;
	TexConvFP *VQ;
   // Conversion to 32 bpp
   TexConvFP32 *PL32;
	TexConvFP32 *TW32;
	TexConvFP32 *VQ32;
};

PvrTexInfo format[8]=
{
   // name     bpp GL format				   Planar		Twiddled	 VQ				Planar(32b)    Twiddled(32b)  VQ (32b)
   {"1555", 	16,	GL_UNSIGNED_SHORT_5_5_5_1, tex1555_PL,	tex1555_TW,  tex1555_VQ,	tex1555_PL32,  tex1555_TW32,  tex1555_VQ32 },	//1555
	{"565", 	16, GL_UNSIGNED_SHORT_5_6_5,   tex565_PL,	tex565_TW,   tex565_VQ, 	tex565_PL32,   tex565_TW32,   tex565_VQ32 },	//565
	{"4444", 	16, GL_UNSIGNED_SHORT_4_4_4_4, tex4444_PL,	tex4444_TW,  tex4444_VQ, 	tex4444_PL32,  tex4444_TW32,  tex4444_VQ32 },	//4444
	{"yuv", 	16, GL_UNSIGNED_INT_8_8_8_8,   NULL, 		NULL, 		 NULL,			texYUV422_PL,  texYUV422_TW,  texYUV422_VQ },	//yuv
	{"bumpmap", 16, GL_UNSIGNED_SHORT_4_4_4_4, texBMP_PL,	texBMP_TW,	 texBMP_VQ, 	NULL},											//bump map
	{"pal4", 	4,	0,						   0,			texPAL4_TW,  0, 			NULL, 		   texPAL4_TW32,  NULL },			//pal4
	{"pal8", 	8,	0,						   0,			texPAL8_TW,  0, 			NULL, 		   texPAL8_TW32,  NULL },			//pal8
	{"ns/1555", 0},
};

const u32 MipPoint[8] =
{
	0x00006, /*    8  x 8*/
	0x00016, /*   16  x 16*/
	0x00056, /*   32  x 32 */
	0x00156, /*   64  x 64 */
	0x00556, /*  128  x 128*/
	0x01556, /*  256  x 256*/
	0x05556, /*  512  x 512 */
	0x15556  /* 1024  x 1024 */
};

const GLuint PAL_TYPE[4]=
{GL_UNSIGNED_SHORT_5_5_5_1,GL_UNSIGNED_SHORT_5_6_5,GL_UNSIGNED_SHORT_4_4_4_4, GL_UNSIGNED_INT_8_8_8_8};



/* Texture Cache */
struct TextureCacheData
{
	TSP tsp;             /* PowerVR texture parameters */
	TCW tcw;

   GLuint texID;        /* GL texture ID */
	u16* pData;
	int tex_type;

	u32 Lookups;

	/* decoded texture info */
   u32 sa;              /* pixel data start address in VRAM (might be offset for mipmaps/etc) */
   u32 sa_tex;		      /* texture data start address in VRAM */
   u32 w,h;             /* Width & height of the texture */
   u32 size;            /* Size, in bytes, in VRAM */

	PvrTexInfo *tex;
	TexConvFP  *texconv;
   TexConvFP32*  texconv32;

	u32 dirty;
	vram_block* lock_block;

	u32 Updates;

	/* Used for palette updates */
	u32  pal_local_rev;         /* Local palette rev */
	u32* pal_table_rev;         /* Table palette rev pointer */
	u32  indirect_color_ptr;    /* Palette color table index for paletted texture */

	                            /* VQ quantizers table for VQ texture.
	                             * A texture can't be both VQ and PAL (paletted) at the same time */

	//Create GL texture from tsp/tcw
	void Create(bool isGL)
	{
      texID      = 0;

		if (isGL)
			texID = glcache.GenTexture();
		
		/* Reset state info */
		pData      = 0;
		tex_type   = 0;
		Lookups    = 0;
		Updates    = 0;
		dirty      = FrameCount;
		lock_block = 0;

		/* Decode info from TSP/TCW into the texture struct */
		tex        = &format[tcw.PixelFmt == PixelReserved ? Pixel1555 : tcw.PixelFmt];		/* texture format table entry */

		sa_tex     = (tcw.TexAddr<<3) & VRAM_MASK;               /* texture start address */
		sa         = sa_tex;						                     /* data texture start address (modified for MIPs, as needed) */
		w          = 8 << tsp.TexU;                              /* texture width */
		h          = 8 << tsp.TexV;                              /* texture height */

      pal_table_rev = 0;

		/* PAL texture */
      if (tex->bpp == 4)
      {
         pal_table_rev=&pal_rev_16[tcw.PalSelect];
         indirect_color_ptr=tcw.PalSelect<<4;
      }
      else if (tex->bpp == 8)
      {
         pal_table_rev=&pal_rev_256[tcw.PalSelect>>4];
         indirect_color_ptr=(tcw.PalSelect>>4)<<8;
      }
      else
      {
         pal_table_rev=0;
      }

		/* VQ table (if VQ texture) */
		if (tcw.VQ_Comp)
			indirect_color_ptr = sa;

      texconv = 0;

      /* Convert a PVR texture into OpenGL */
		switch (tcw.PixelFmt)
      {
         case Pixel1555:     /* ARGB1555  - value: 1 bit; RGB values: 5 bits each */
         case PixelReserved: /* RESERVED1 - Regarded as 1555 */
         case Pixel565:      /* RGB565    - R value: 5 bits; G value: 6 bits; B value: 5 bits */
         case Pixel4444:     /* ARGB4444  - value: 4 bits; RGB values: 4 bits each */
         case PixelYUV:   /* YUV422    - 32 bits per 2 pixels; YUYV values: 8 bits each */
         case PixelBumpMap:  /* BUMPMAP   - 16 bits/pixel; S value: 8 bits; R value: 8 bits */
         case PixelPal4:     /* 4BPP      - Palette texture with 4 bits/pixel */
         case PixelPal8:     /* 8BPP      - Palette texture with 8 bits/pixel */
            if (tcw.ScanOrder && (tex->PL || tex->PL32))
            {
               //Texture is stored 'planar' in memory, no deswizzle is needed
               //verify(tcw.VQ_Comp==0);
#ifndef NDEBUG
               if (tcw.VQ_Comp != 0)
                  printf("Warning: planar texture with VQ set (invalid)\n");
#endif

               /* Planar textures support stride selection,
                * mostly used for NPOT textures (videos). */
               int stride = w;
               if (tcw.StrideSel)
                  stride  = (TEXT_CONTROL&31)*32;

               /* Call the format specific conversion code */
               texconv    = tex->PL;
               texconv32  = tex->PL32;
               size       = stride * h * tex->bpp/8;
               /* Calculate the size, in bytes, for the locking. */
            }
            else
            {
               size = w * h;
               if (tcw.VQ_Comp)
               {
                  verify(tex->VQ != NULL || tex->VQ32 != NULL);
                  indirect_color_ptr = sa;
                  if (tcw.MipMapped)
                     sa             += MipPoint[tsp.TexU];
                  texconv            = tex->VQ;
                  texconv32          = tex->VQ32;
               }
               else
               {
                  verify(tex->TW != NULL || tex->TW32 != NULL);
                  if (tcw.MipMapped)
                     sa             += MipPoint[tsp.TexU]*tex->bpp/2;
                  texconv            = tex->TW;
                  texconv32          = tex->TW32;
                  size              *= tex->bpp;
               }
               size /= 8;
            }
            break;
         default:
            printf("Unhandled texture %d\n",tcw.PixelFmt);
            size=w*h*2;
            texconv = NULL;
            texconv32 = NULL;
            break;
      }
	}

	void Update(void)
   {
      GLuint textype;

      Updates++;                                   /* texture state tracking stuff */
      dirty              = 0;
      textype            = tex->type;

      bool has_alpha = false;
      if (pal_table_rev) 
      {
         textype         = PAL_TYPE[PAL_RAM_CTRL&3];
         pal_local_rev   = *pal_table_rev;             /* make sure to update the local rev, 
                                                      so it won't have to redo the texture */
         if (textype == GL_UNSIGNED_INT_8_8_8_8)
				has_alpha = true;
      }

      palette_index      = indirect_color_ptr;              /* might be used if paletted texture */
      vq_codebook        = (u8*)&vram.data[indirect_color_ptr];  /* might be used if VQ texture */

      //texture conversion work

      u32 stride         = w;

      if (tcw.StrideSel && tcw.ScanOrder && (tex->PL || tex->PL32))
         stride = (TEXT_CONTROL&31)*32; //I think this needs +1 ?

      //PrintTextureName();
      if (sa_tex > VRAM_SIZE || size == 0 || sa + size > VRAM_SIZE)
		{
         printf("Warning: invalid texture. Address %08X %08X size %d\n", sa_tex, sa, size);
			return;
		}

      void *temp_tex_buffer = NULL;
		u32 upscaled_w = w;
		u32 upscaled_h = h;

		PixelBuffer<u16> pb16;
		PixelBuffer<u32> pb32;

		// Figure out if we really need to use a 32-bit pixel buffer
      bool need_32bit_buffer = true;
		if ((settings.rend.TextureUpscale <= 1
				|| w * h > settings.rend.MaxFilteredTextureSize
					* settings.rend.MaxFilteredTextureSize		// Don't process textures that are too big
				|| tcw.PixelFmt == PixelYUV)					// Don't process YUV textures
			&& (pal_table_rev == NULL || textype != GL_UNSIGNED_INT_8_8_8_8)
			&& texconv != NULL)
         need_32bit_buffer = false;
		// TODO avoid upscaling/depost. textures that change too often

      if (texconv32 != NULL && need_32bit_buffer)
      {
         // Force the texture type since that's the only 32-bit one we know
			textype = GL_UNSIGNED_INT_8_8_8_8;

			pb32.init(w, h);

			texconv32(&pb32, (u8*)&vram[sa], stride, h);

#ifdef DEPOSTERIZE
			{
				// Deposterization
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(w, h);

				DePosterize(pb32.data(), tmp_buf.data(), w, h);
				pb32.steal_data(tmp_buf);
			}
#endif

#ifdef HAVE_TEXUPSCALE
			// xBRZ scaling
			if (settings.rend.TextureUpscale > 1)
			{
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(w * settings.rend.TextureUpscale, h * settings.rend.TextureUpscale);

				if (tcw.PixelFmt == Pixel1555 || tcw.PixelFmt == Pixel4444)
					// Alpha channel formats. Palettes with alpha are already handled
					has_alpha = true;
				UpscalexBRZ(settings.rend.TextureUpscale, pb32.data(), tmp_buf.data(), w, h, has_alpha);
				pb32.steal_data(tmp_buf);
				upscaled_w *= settings.rend.TextureUpscale;
				upscaled_h *= settings.rend.TextureUpscale;
			}
#endif
			temp_tex_buffer = pb32.data();
      }
      else if (texconv != NULL)
      {
         pb16.init(w, h);

         texconv(&pb16,(u8*)&vram[sa],stride,h);
			temp_tex_buffer = pb16.data();
      }
      else
      {
         /* fill it in with a temporary color. */
         printf("UNHANDLED TEXTURE\n");
         pb16.init(w, h);
			memset(pb16.data(), 0x80, w * h * 2);
			temp_tex_buffer = pb16.data();
      }

      /* lock the texture to detect changes in it. */
      lock_block = libCore_vramlock_Lock(sa_tex,sa+size-1,this);

      if (texID)
      {
         glcache.BindTexture(GL_TEXTURE_2D, texID);
         GLuint comps=textype==GL_UNSIGNED_SHORT_5_6_5?GL_RGB:GL_RGBA;
#ifdef GLES
			GLuint actual_textype = textype == GL_UNSIGNED_INT_8_8_8_8 ? GL_UNSIGNED_BYTE : textype;
         glTexImage2D(GL_TEXTURE_2D, 0,comps , upscaled_w, upscaled_h, 0, comps, actual_textype, temp_tex_buffer);
#else
         glTexImage2D(GL_TEXTURE_2D, 0,comps , upscaled_w, upscaled_h, 0, comps, textype, temp_tex_buffer);
#endif
         if (tcw.MipMapped && settings.rend.UseMipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);
         //dumpTexture(texID, w, h, textype, temp_tex_buffer);
      }
      else
      {
#if FEAT_HAS_SOFTREND
         if (textype == GL_UNSIGNED_SHORT_5_6_5)
            tex_type = 0;
         else if (textype == GL_UNSIGNED_SHORT_5_5_5_1)
            tex_type = 1;
         else if (textype == GL_UNSIGNED_SHORT_4_4_4_4)
            tex_type = 2;

         u16 *tex_data = (u16 *)temp_tex_buffer;
         if (pData)
         {
#ifdef __SSE4_1__
            _mm_free(pData);
#else
            memalign_free(pData);
#endif
         }

#ifdef __SSE4_1__
         pData = (u16*)_mm_malloc(w * h * 16, 16);
#else
         pData = (u16*)memalign_alloc(16, w * h * 16);
#endif
         for (int y = 0; y < h; y++)
         {
            for (int x = 0; x < w; x++)
            {
               u32* data = (u32*)&pData[(x + y*w) * 8];

               data[0]   = decoded_colors[tex_type][tex_data[(x + 1) % w + (y + 1) % h * w]];
               data[1]   = decoded_colors[tex_type][tex_data[(x + 0) % w + (y + 1) % h * w]];
               data[2]   = decoded_colors[tex_type][tex_data[(x + 1) % w + (y + 0) % h * w]];
               data[3]   = decoded_colors[tex_type][tex_data[(x + 0) % w + (y + 0) % h * w]];
            }
         }
#endif
      }
   }

	/* true if : dirty or paletted texture and revs don't match */
	bool NeedsUpdate()
   { 
      return (dirty) || (pal_table_rev!=0 && *pal_table_rev!=pal_local_rev);
   }
	
	void Delete()
	{
#if FEAT_HAS_SOFTREND
      if (pData)
#ifdef __SSE4_1__
         _mm_free(pData);
#else
         memalign_free(pData);
#endif
      pData = 0;
#endif

		if (texID)
         glcache.DeleteTextures(1, &texID);
      texID = 0;
		if (lock_block)
			libCore_vramlock_Unlock_block(lock_block);
		lock_block=0;
	}
};

#include <map>
map<u64,TextureCacheData> TexCache;
typedef map<u64,TextureCacheData>::iterator TexCacheIter;

TextureCacheData *getTextureCacheData(TSP tsp, TCW tcw);


FBT fb_rtt;

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt)
{
	FBT& rv=fb_rtt;

	if (rv.fbo)
      glDeleteFramebuffers(1,&rv.fbo);
	if (rv.tex)
      glcache.DeleteTextures(1,&rv.tex);
	if (rv.depthb)
      glDeleteRenderbuffers(1,&rv.depthb);

	rv.TexAddr=addy>>3;

	/* Find the largest square POT texture that fits into the viewport */
   int fbh2 = 2;
   while (fbh2 < fbh)
      fbh2 *= 2;
   int fbw2 = 2;
   while (fbw2 < fbw)
      fbw2 *= 2;

   if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
	{
		fbw *= settings.rend.RenderToTextureUpscale;
		fbh *= settings.rend.RenderToTextureUpscale;
		fbw2 *= settings.rend.RenderToTextureUpscale;
		fbh2 *= settings.rend.RenderToTextureUpscale;
	}

	/* Get the currently bound frame buffer object. On most platforms this just gives 0. */

	/* Generate and bind a render buffer which will become a depth buffer shared between our two FBOs */
	glGenRenderbuffers(1, &rv.depthb);
	glBindRenderbuffer(RARCH_GL_RENDERBUFFER, rv.depthb);

	/*
		Currently it is unknown to GL that we want our new render buffer to be a depth buffer.
		glRenderbufferStorage will fix this and in this case will allocate a depth buffer
		m_i32TexSize by m_i32TexSize.
	*/

	glRenderbufferStorage(RARCH_GL_RENDERBUFFER, RARCH_GL_DEPTH24_STENCIL8, fbw2, fbh2);

	/* Create a texture for rendering to */
	rv.tex = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, rv.tex);

	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, fmt, 0);

	/* Create the object that will allow us to render to the aforementioned texture */
	glGenFramebuffers(1, &rv.fbo);
	glBindFramebuffer(RARCH_GL_FRAMEBUFFER, rv.fbo);

	/* Attach the texture to the FBO */
	glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rv.tex, 0);

	// Attach the depth buffer we created earlier to our FBO.
#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES1) || defined(OSX_PPC)
	glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER, RARCH_GL_DEPTH_ATTACHMENT,
         RARCH_GL_RENDERBUFFER, rv.depthb);
   glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER, RARCH_GL_STENCIL_ATTACHMENT,
         RARCH_GL_RENDERBUFFER, rv.depthb);
#else
   glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
         RARCH_GL_RENDERBUFFER, rv.depthb);
#endif

	/* Check that our FBO creation was successful */
	GLuint uStatus = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);

	verify(uStatus == RARCH_GL_FRAMEBUFFER_COMPLETE);

   glViewport(0, 0, fbw, fbh);		// TODO CLIP_X/Y min?
}

void ReadRTTBuffer() {
	u32 w = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
	u32 h = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;

	u32 stride = FB_W_LINESTRIDE.stride * 8;
	if (stride == 0)
		stride = w * 2;
	else if (w * 2 > stride) {
    	// Happens for Virtua Tennis
    	w = stride / 2;
    }
	u32 size = w * h * 2;

	const u8 fb_packmode = FB_W_CTRL.fb_packmode;

	if (settings.rend.RenderToTextureBuffer)
	{
		u32 tex_addr = fb_rtt.TexAddr << 3;

		// Manually mark textures as dirty and remove all vram locks before calling glReadPixels
		// (deadlock on rpi)
		for (TexCacheIter i = TexCache.begin(); i != TexCache.end(); i++)
		{
			if (i->second.sa_tex <= tex_addr + size - 1 && i->second.sa + i->second.size - 1 >= tex_addr) {
				i->second.dirty = FrameCount;
				if (i->second.lock_block != NULL) {
					libCore_vramlock_Unlock_block(i->second.lock_block);
					i->second.lock_block = NULL;
				}
			}
		}
		vram.UnLockRegion(0, 2 * vram.size);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		u16 *dst = (u16 *)&vram[tex_addr];

		GLint color_fmt, color_type;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &color_fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &color_type);

		if (fb_packmode == 1 && stride == w * 2 && color_fmt == GL_RGB && color_type == GL_UNSIGNED_SHORT_5_6_5) {
			// Can be read directly into vram
			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dst);
		}
		else
		{
			PixelBuffer<u32> tmp_buf;
			tmp_buf.init(w, h);

			const u16 kval_bit = (FB_W_CTRL.fb_kval & 0x80) << 8;
			const u8 fb_alpha_threshold = FB_W_CTRL.fb_alpha_threshold;

			u8 *p = (u8 *)tmp_buf.data();
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);

			for (u32 l = 0; l < h; l++) {
				switch(fb_packmode)
				{
				case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 3) & 0x1F) << 10) | (((p[1] >> 3) & 0x1F) << 5) | ((p[2] >> 3) & 0x1F) | kval_bit;
						p += 4;
					}
					break;
				case 1: //0x1   565 RGB 16 bit
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 3) & 0x1F) << 11) | (((p[1] >> 2) & 0x3F) << 5) | ((p[2] >> 3) & 0x1F);
						p += 4;
					}
					break;
				case 2: //0x2   4444 ARGB 16 bit
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 4) & 0xF) << 8) | (((p[1] >> 4) & 0xF) << 4) | ((p[2] >> 4) & 0xF) | (((p[3] >> 4) & 0xF) << 12);
						p += 4;
					}
					break;
				case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
					for (u32 c = 0; c < w; c++) {
						*dst++ = (((p[0] >> 3) & 0x1F) << 10) | (((p[1] >> 3) & 0x1F) << 5) | ((p[2] >> 3) & 0x1F) | (p[3] >= fb_alpha_threshold ? 0x8000 : 0);
						p += 4;
					}
					break;
				}
				dst += (stride - w * 2) / 2;
			}
		}

		// Restore VRAM locks
		for (TexCacheIter i = TexCache.begin(); i != TexCache.end(); i++)
		{
				if (i->second.lock_block != NULL) {
						vram.LockRegion(i->second.sa_tex, i->second.sa + i->second.size - i->second.sa_tex);

						//TODO: Fix this for 32M wrap as well
						if (_nvmem_enabled() && VRAM_SIZE == 0x800000) {
								vram.LockRegion(i->second.sa_tex + VRAM_SIZE, i->second.sa + i->second.size - i->second.sa_tex);
						}
				}
		}
	}
	else
	{
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);
	}

    //dumpRtTexture(fb_rtt.TexAddr, w, h);
    
    if (w > 1024 || h > 1024) {
    	glcache.DeleteTextures(1, &fb_rtt.tex);
    }
    else
    {
    	TCW tcw = { { TexAddr : fb_rtt.TexAddr, Reserved : 0, StrideSel : 0, ScanOrder : 1 } };
    	switch (fb_packmode) {
    	case 0:
    	case 3:
    		tcw.PixelFmt = Pixel1555;
    		break;
    	case 1:
    		tcw.PixelFmt = Pixel565;
    		break;
    	case 2:
    		tcw.PixelFmt = Pixel4444;
    		break;
    	}
    	TSP tsp = { 0 };
    	for (tsp.TexU = 0; tsp.TexU <= 7 && (8 << tsp.TexU) < w; tsp.TexU++);
    	for (tsp.TexV = 0; tsp.TexV <= 7 && (8 << tsp.TexV) < h; tsp.TexV++);

    	TextureCacheData *texture_data = getTextureCacheData(tsp, tcw);
    	if (texture_data->texID != 0)
    		glcache.DeleteTextures(1, &texture_data->texID);
    	else {
    		texture_data->Create(false);
    		texture_data->lock_block = libCore_vramlock_Lock(texture_data->sa_tex, texture_data->sa + texture_data->size - 1, texture_data);
    	}
    	texture_data->texID = fb_rtt.tex;
    	texture_data->dirty = 0;
    }
    fb_rtt.tex = 0;

	if (fb_rtt.fbo) { glDeleteFramebuffers(1,&fb_rtt.fbo); fb_rtt.fbo = 0; }
	if (fb_rtt.depthb) { glDeleteRenderbuffers(1,&fb_rtt.depthb); fb_rtt.depthb = 0; }
	if (fb_rtt.stencilb) { glDeleteRenderbuffers(1,&fb_rtt.stencilb); fb_rtt.stencilb = 0; }

   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
}

static int TexCacheLookups;
static int TexCacheHits;
static float LastTexCacheStats;

// Only use TexU and TexV from TSP in the cache key
const TSP TSPTextureCacheMask = { { TexV : 7, TexU : 7 } };
const TCW TCWTextureCacheMask = { { TexAddr : 0x1FFFFF, Reserved : 0, StrideSel : 0, ScanOrder : 0, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1 } };

TextureCacheData *getTextureCacheData(TSP tsp, TCW tcw) {
   u64 key = tsp.full & TSPTextureCacheMask.full;

   if (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
		// Paletted textures have a palette selection that must be part of the key
		key |= (u64)tcw.full << 32;
	else
		key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

	TexCacheIter tx = TexCache.find(key);

	TextureCacheData* tf;
	if (tx != TexCache.end())
	{
		tf = &tx->second;
      // Needed if the texture is updated
		tf->tcw.StrideSel = tcw.StrideSel;
		tf->tcw.ScanOrder = tcw.ScanOrder;
	}
	else //create if not existing
	{
		TextureCacheData tfc={0};
		TexCache[key] = tfc;

		tx=TexCache.find(key);
		tf=&tx->second;

		tf->tsp = tsp;
		tf->tcw = tcw;
	}

	return tf;
}

GLuint gl_GetTexture(TSP tsp, TCW tcw)
{
   TexCacheLookups++;

	/* Lookup texture */
   TextureCacheData* tf = getTextureCacheData(tsp, tcw);

   if (tf->texID == 0)
		tf->Create(true);

	/* Update if needed */
	if (tf->NeedsUpdate())
		tf->Update();
   else
      TexCacheHits++;

	/* Update state for opts/stuff */
	tf->Lookups++;

	/* Return gl texture */
	return tf->texID;
}

text_info raw_GetTexture(TSP tsp, TCW tcw)
{
	text_info rv = { 0 };

	//lookup texture
	TextureCacheData* tf;
	u64 key = ((u64)(tcw.full & TCWTextureCacheMask.full) << 32) | (tsp.full & TSPTextureCacheMask.full);

	TexCacheIter tx = TexCache.find(key);

	if (tx != TexCache.end())
	{
		tf = &tx->second;
	}
	else //create if not existing
	{
		TextureCacheData tfc = { 0 };
		TexCache[key] = tfc;

		tx = TexCache.find(key);
		tf = &tx->second;

		tf->tsp = tsp;
		tf->tcw = tcw;
		tf->Create(false);
	}

	//update if needed
	if (tf->NeedsUpdate())
		tf->Update();

	//update state for opts/stuff
	tf->Lookups++;

	//return gl texture
	rv.height = tf->h;
	rv.width = tf->w;
	rv.pdata = tf->pData;
	rv.textype = tf->tex_type;
	
	
	return rv;
}

void CollectCleanup(void)
{
   vector<u64> list;

   u32 TargetFrame = max((u32)120,FrameCount) - 120;

   for (TexCacheIter i=TexCache.begin();i!=TexCache.end();i++)
   {
      if ( i->second.dirty &&  i->second.dirty < TargetFrame)
         list.push_back(i->first);

      if (list.size() > 5)
         break;
   }

   for (size_t i=0; i<list.size(); i++)
   {
      TexCache[list[i]].Delete();
      TexCache.erase(list[i]);
   }
}

void killtex(void)
{
	for (TexCacheIter i=TexCache.begin();i!=TexCache.end();i++)
		i->second.Delete();

	TexCache.clear();
}

void rend_text_invl(vram_block* bl)
{
	TextureCacheData* tcd = (TextureCacheData*)bl->userdata;
	tcd->dirty=FrameCount;
	tcd->lock_block=0;

	libCore_vramlock_Unlock_block_wb(bl);
}
