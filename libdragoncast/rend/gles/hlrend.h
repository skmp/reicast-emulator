#include <math.h>
#include "glcache.h"
#include "rend/TexCache.h"
#include "cfg/cfg.h"
#include "rend/gui.h"

#include "oslib/oslib.h"
#include "rend/rend.h"
#include "input/gamepad.h"

float fb_scale_x, fb_scale_y;
float scale_x, scale_y;

bool hl_init(void* hwnd, void* hdc)
{
	return false;
}

void hl_swap()
{

}

void hl_term()
{
}


void hl_delete_shaders()
{
}

struct ShaderUniforms_t ShaderUniforms;

PipelineShader* GetProgram(u32 cp_AlphaTest, u32 pp_ClipTestMode,
	u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
	u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear)
{
	if (settings.rend.Rotate90 != gl.rotate90)
	{
		gl_delete_shaders();
		gl.rotate90 = settings.rend.Rotate90;
	}
	u32 rv = 0;

	rv |= pp_ClipTestMode;
	rv <<= 1; rv |= cp_AlphaTest;
	rv <<= 1; rv |= pp_Texture;
	rv <<= 1; rv |= pp_UseAlpha;
	rv <<= 1; rv |= pp_IgnoreTexA;
	rv <<= 2; rv |= pp_ShadInstr;
	rv <<= 1; rv |= pp_Offset;
	rv <<= 2; rv |= pp_FogCtrl;
	rv <<= 1; rv |= pp_Gouraud;
	rv <<= 1; rv |= pp_BumpMap;
	rv <<= 1; rv |= fog_clamping;
	rv <<= 1; rv |= trilinear;

	PipelineShader* shader = &gl.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_ClipTestMode = pp_ClipTestMode - 1;
		shader->pp_Texture = pp_Texture;
		shader->pp_UseAlpha = pp_UseAlpha;
		shader->pp_IgnoreTexA = pp_IgnoreTexA;
		shader->pp_ShadInstr = pp_ShadInstr;
		shader->pp_Offset = pp_Offset;
		shader->pp_FogCtrl = pp_FogCtrl;
		shader->pp_Gouraud = pp_Gouraud;
		shader->pp_BumpMap = pp_BumpMap;
		shader->fog_clamping = fog_clamping;
		shader->trilinear = trilinear;
		CompilePipelineShader(shader);
	}

	return shader;
}

bool CompilePipelineShader(PipelineShader* s)
{

	return false;
}

GLuint osd_tex;

void hl_load_osd_resources()
{

}

void hl_free_osd_resources()
{

}

static void hl_create_modvol_shader()
{

}

bool hl_create_resources()
{
	hl_create_modvol_shader();

	hl_load_osd_resources();

	gui_init();

	return true;
}

//swap buffers
void gl_swap();

GLuint gl_CompileShader(const char* shader, GLuint type);

bool gl_create_resources();

//setup


bool gles_init()
{
	if (!gl_init((void*)libPvr_GetRenderTarget(),
		(void*)libPvr_GetRenderSurface()))
		return false;

	glcache.EnableCache();

	if (!gl_create_resources())
		return false;

#ifdef USE_EGL
#ifdef TARGET_PANDORA
	fbdev = open("/dev/fb0", O_RDONLY);
#else
	eglSwapInterval(gl.setup.display, 1);
#endif
#endif

	//    glEnable(GL_DEBUG_OUTPUT);
	//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	//    glDebugMessageCallback(gl_DebugOutput, NULL);
	//    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

	//clean up the buffer
	glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	gl_swap();

#ifdef GL_GENERATE_MIPMAP_HINT
	if (gl.is_gles)
		glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
#endif

	if (settings.rend.TextureUpscale > 1)
	{
		// Trick to preload the tables used by xBRZ
		u32 src[]{ 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
		u32 dst[16];
		UpscalexBRZ(2, src, dst, 2, 2, false);
	}
	fog_needs_update = true;

	return true;
}


void UpdateFogTexture(u8* fog_table, GLenum texture_slot, GLint fog_image_format)
{
	glActiveTexture(texture_slot);
	if (fogTextureId == 0)
	{
		fogTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);

	u8 temp_tex_buffer[256];
	for (int i = 0; i < 128; i++)
	{
		temp_tex_buffer[i] = fog_table[i * 4];
		temp_tex_buffer[i + 128] = fog_table[i * 4 + 1];
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, fog_image_format, 128, 2, 0, fog_image_format, GL_UNSIGNED_BYTE, temp_tex_buffer);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}



static void upload_vertex_indices()
{
	if (gl.index_type == GL_UNSIGNED_SHORT)
	{
		static bool overrun;
		static List<u16> short_idx;
		if (short_idx.daty != NULL)
			short_idx.Free();
		short_idx.Init(pvrrc.idx.used(), &overrun, NULL);
		for (u32* p = pvrrc.idx.head(); p < pvrrc.idx.LastPtr(0); p++)
			* (short_idx.Append()) = *p;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, short_idx.bytes(), short_idx.head(), GL_STREAM_DRAW);
	}
	else
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, pvrrc.idx.bytes(), pvrrc.idx.head(), GL_STREAM_DRAW);
	glCheck();
}



void rend_set_fb_scale(float x, float y)
{
	fb_scale_x = x;
	fb_scale_y = y;
}

struct glesrend : Renderer
{
	bool Init() 
	{
		return gles_init();
	}

	void Resize(int w, int h)
	{
		screen_width = w;
		screen_height = h;
	}

	void Term()
	{
		if (KillTex)
			killtex();
		gles_term();
	}

	bool Process(TA_context* ctx)
	{
		ctx->rend_inuse.Lock();

		if (KillTex)
			killtex();

		if (ctx->rend.isRenderFramebuffer)
		{
			RenderFramebuffer();
			ctx->rend_inuse.Unlock();
		}
		else
		{
			if (!ta_parse_vdrc(ctx))
				return false;
		}
		CollectCleanup();

		if (ctx->rend.Overrun)
			printf("ERROR: TA context overrun\n");

		return !ctx->rend.Overrun;

	}

	bool Render()
	{
#if 0
		bool is_rtt = pvrrc.isRTT;

		//if (FrameCount&7) return;

		//Setup the matrix

		//TODO: Make this dynamic
		float vtx_min_fZ = 0.f;	//pvrrc.fZ_min;
		float vtx_max_fZ = pvrrc.fZ_max;

		//sanitise the values, now with NaN detection (for omap)
		//0x49800000 is 1024*1024. Using integer math to avoid issues w/ infs and nans
		if ((s32&)vtx_max_fZ < 0 || (u32&)vtx_max_fZ>0x49800000)
			vtx_max_fZ = 10 * 1024;


		//add some extra range to avoid clipping border cases
		vtx_min_fZ *= 0.98f;
		vtx_max_fZ *= 1.001f;

		//calculate a projection so that it matches the pvr x,y setup, and
		//a) Z is linearly scaled between 0 ... 1
		//b) W is passed though for proper perspective calculations

		/*
		PowerVR coords:
		fx, fy (pixel coordinates)
		fz=1/w

		(as a note, fx=x*fz;fy=y*fz)

		Clip space
		-Wc .. Wc, xyz
		x: left-right, y: bottom-top
		NDC space
		-1 .. 1, xyz
		Window space:
		translated NDC (viewport, glDepth)

		Attributes:
		//this needs to be cleared up, been some time since I wrote my rasteriser and i'm starting
		//to forget/mixup stuff
		vaX         -> VS output
		iaX=vaX*W   -> value to be interpolated
		iaX',W'     -> interpolated values
		paX=iaX'/W' -> Per pixel interpolated value for attribute


		Proper mappings:
		Output from shader:
		W=1/fz
		x=fx*W -> maps to fx after perspective divide
		y=fy*W ->         fy   -//-
		z=-W for min, W for max. Needs to be linear.



		umodified W, perfect mapping:
		Z mapping:
		pz=z/W
		pz=z/(1/fz)
		pz=z*fz
		z=zt_s+zt_o
		pz=(zt_s+zt_o)*fz
		pz=zt_s*fz+zt_o*fz
		zt_s=scale
		zt_s=2/(max_fz-min_fz)
		zt_o*fz=-min_fz-1
		zt_o=(-min_fz-1)/fz == (-min_fz-1)*W


		x=fx/(fx_range/2)-1		//0 to max -> -1 to 1
		y=fy/(-fy_range/2)+1	//0 to max -> 1 to -1
		z=-min_fz*W + (zt_s-1)  //0 to +inf -> -1 to 1

		o=a*z+c
		1=a*z_max+c
		-1=a*z_min+c

		c=-a*z_min-1
		1=a*z_max-a*z_min-1
		2=a*(z_max-z_min)
		a=2/(z_max-z_min)
		*/

		//float B=2/(min_invW-max_invW);
		//float A=-B*max_invW+vnear;

		//these should be adjusted based on the current PVR scaling etc params
		float dc_width = 640;
		float dc_height = 480;

		if (!is_rtt)
		{
			gcflip = 0;
		}
		else
		{
			gcflip = 1;

			//For some reason this produces wrong results
			//so for now its hacked based like on the d3d code
			/*
			u32 pvr_stride=(FB_W_LINESTRIDE.stride)*8;
			*/
			dc_width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
			dc_height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
		}

		scale_x = 1;
		scale_y = 1;

		float scissoring_scale_x = 1;

		if (!is_rtt && !pvrrc.isRenderFramebuffer)
		{
			scale_x = fb_scale_x;
			scale_y = fb_scale_y;
			if (SCALER_CTL.interlace == 0 && SCALER_CTL.vscalefactor >= 0x400)
				scale_y *= SCALER_CTL.vscalefactor / 0x400;

			//work out scaling parameters !
			//Pixel doubling is on VO, so it does not affect any pixel operations
			//A second scaling is used here for scissoring
			if (VO_CONTROL.pixel_double)
			{
				scissoring_scale_x = 0.5f;
				scale_x *= 0.5f;
			}

			if (SCALER_CTL.hscale)
			{
				scissoring_scale_x /= 2;
				scale_x *= 2;
			}
		}

		dc_width *= scale_x;
		dc_height *= scale_y;

		/*

		float vnear=0;
		float vfar =1;

		float max_invW=1/vtx_min_fZ;
		float min_invW=1/vtx_max_fZ;

		float B=vfar/(min_invW-max_invW);
		float A=-B*max_invW+vnear;


		GLfloat dmatrix[16] =
		{
			(2.f/dc_width)  ,0                ,-(640/dc_width)              ,0  ,
			0               ,-(2.f/dc_height) ,(480/dc_height)              ,0  ,
			0               ,0                ,A                            ,B  ,
			0               ,0                ,1                            ,0
		};

		glUniformMatrix4fv(gl.matrix, 1, GL_FALSE, dmatrix);

		*/

		/*
			Handle Dc to screen scaling
		*/
		float screen_stretching = settings.rend.ScreenStretching / 100.f;
		float screen_scaling = settings.rend.ScreenScaling / 100.f;

		float dc2s_scale_h;
		float ds2s_offs_x;

		if (is_rtt)
		{
			ShaderUniforms.scale_coefs[0] = 2.0f / dc_width;
			ShaderUniforms.scale_coefs[1] = 2.0f / dc_height;	// FIXME CT2 needs 480 here instead of dc_height=512
			ShaderUniforms.scale_coefs[2] = 1;
			ShaderUniforms.scale_coefs[3] = 1;
		}
		else
		{
			if (settings.rend.Rotate90)
			{
				dc2s_scale_h = screen_height / 640.0f;
				ds2s_offs_x = (screen_width - dc2s_scale_h * 480.0f * screen_stretching) / 2;
				ShaderUniforms.scale_coefs[0] = 2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
				ShaderUniforms.scale_coefs[1] = -2.0f / dc_width;
				ShaderUniforms.scale_coefs[2] = 1 - 2 * ds2s_offs_x / screen_width;
				ShaderUniforms.scale_coefs[3] = 1;
			}
			else
			{
				dc2s_scale_h = screen_height / 480.0f;
				ds2s_offs_x = (screen_width - dc2s_scale_h * 640.0f * screen_stretching) / 2;
				ShaderUniforms.scale_coefs[0] = 2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
				ShaderUniforms.scale_coefs[1] = -2.0f / dc_height;
				ShaderUniforms.scale_coefs[2] = 1 - 2 * ds2s_offs_x / screen_width;
				ShaderUniforms.scale_coefs[3] = -1;
			}
			//-1 -> too much to left
		}

		ShaderUniforms.depth_coefs[0] = 2 / (vtx_max_fZ - vtx_min_fZ);
		ShaderUniforms.depth_coefs[1] = -vtx_min_fZ - 1;
		ShaderUniforms.depth_coefs[2] = 0;
		ShaderUniforms.depth_coefs[3] = 0;

		ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

		//printf("scale: %f, %f, %f, %f\n",ShaderUniforms.scale_coefs[0],ShaderUniforms.scale_coefs[1],ShaderUniforms.scale_coefs[2],ShaderUniforms.scale_coefs[3]);


		//VERT and RAM fog color constants
		u8* fog_colvert_bgra = (u8*)& FOG_COL_VERT;
		u8* fog_colram_bgra = (u8*)& FOG_COL_RAM;
		ShaderUniforms.ps_FOG_COL_VERT[0] = fog_colvert_bgra[2] / 255.0f;
		ShaderUniforms.ps_FOG_COL_VERT[1] = fog_colvert_bgra[1] / 255.0f;
		ShaderUniforms.ps_FOG_COL_VERT[2] = fog_colvert_bgra[0] / 255.0f;

		ShaderUniforms.ps_FOG_COL_RAM[0] = fog_colram_bgra[2] / 255.0f;
		ShaderUniforms.ps_FOG_COL_RAM[1] = fog_colram_bgra[1] / 255.0f;
		ShaderUniforms.ps_FOG_COL_RAM[2] = fog_colram_bgra[0] / 255.0f;

		//Fog density constant
		u8* fog_density = (u8*)& FOG_DENSITY;
		float fog_den_mant = fog_density[1] / 128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
		s32 fog_den_exp = (s8)fog_density[0];
		ShaderUniforms.fog_den_float = fog_den_mant * powf(2.0f, fog_den_exp);

		ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
		ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
		ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
		ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;

		ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
		ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
		ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
		ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;

		if (fog_needs_update && settings.rend.Fog)
		{
			fog_needs_update = false;
			UpdateFogTexture((u8*)FOG_TABLE, GL_TEXTURE1, gl.fog_image_format);
		}

		glcache.UseProgram(gl.modvol_shader.program);

		glUniform4fv(gl.modvol_shader.scale, 1, ShaderUniforms.scale_coefs);
		glUniform4fv(gl.modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);
		glUniform1f(gl.modvol_shader.extra_depth_scale, ShaderUniforms.extra_depth_scale);

		ShaderUniforms.PT_ALPHA = (PT_ALPHA_REF & 0xFF) / 255.0f;

		for (auto it : gl.shaders)
		{
			glcache.UseProgram(it.second.program);
			ShaderUniforms.Set(&it.second);
		}

		bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& (pvrrc.fb_X_CLIP.max + 1) / scale_x == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& (pvrrc.fb_Y_CLIP.max + 1) / scale_y == 480;

		//Color is cleared by the background plane

		glcache.Disable(GL_SCISSOR_TEST);

		glcache.DepthMask(GL_TRUE);
		glClearDepthf(0.0);
		glStencilMask(0xFF); glCheck();
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();

		//move vertex to gpu

		if (!pvrrc.isRenderFramebuffer)
		{
			//Main VBO
			glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();

			glBufferData(GL_ARRAY_BUFFER, pvrrc.verts.bytes(), pvrrc.verts.head(), GL_STREAM_DRAW); glCheck();

			upload_vertex_indices();

			//Modvol VBO
			if (pvrrc.modtrig.used())
			{
				glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.modvols); glCheck();
				glBufferData(GL_ARRAY_BUFFER, pvrrc.modtrig.bytes(), pvrrc.modtrig.head(), GL_STREAM_DRAW); glCheck();
			}

			//not all scaling affects pixel operations, scale to adjust for that
			scale_x *= scissoring_scale_x;

#if 0
			//handy to debug really stupid render-not-working issues ...
			printf("SS: %dx%d\n", screen_width, screen_height);
			printf("SCI: %d, %f\n", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
			printf("SCI: %f, %f, %f, %f\n", offs_x + pvrrc.fb_X_CLIP.min / scale_x, (pvrrc.fb_Y_CLIP.min / scale_y) * dc2s_scale_h, (pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1) / scale_x * dc2s_scale_h, (pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1) / scale_y * dc2s_scale_h);
#endif

			if (!wide_screen_on)
			{
				float width = (pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1) / scale_x;
				float height = (pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1) / scale_y;
				float min_x = pvrrc.fb_X_CLIP.min / scale_x;
				float min_y = pvrrc.fb_Y_CLIP.min / scale_y;
				if (!is_rtt)
				{
					if (SCALER_CTL.interlace && SCALER_CTL.vscalefactor >= 0x400)
					{
						// Clipping is done after scaling/filtering so account for that if enabled
						height *= SCALER_CTL.vscalefactor / 0x400;
						min_y *= SCALER_CTL.vscalefactor / 0x400;
					}
					if (settings.rend.Rotate90)
					{
						float t = width;
						width = height;
						height = t;
						t = min_x;
						min_x = min_y;
						min_y = 640 - t - height;
					}
					// Add x offset for aspect ratio > 4/3
					min_x = (min_x * dc2s_scale_h * screen_stretching + ds2s_offs_x) * screen_scaling;
					// Invert y coordinates when rendering to screen
					min_y = (screen_height - (min_y + height) * dc2s_scale_h) * screen_scaling;
					width *= dc2s_scale_h * screen_stretching * screen_scaling;
					height *= dc2s_scale_h * screen_scaling;

					if (ds2s_offs_x > 0)
					{
						float scaled_offs_x = ds2s_offs_x * screen_scaling;

						glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
						glcache.Enable(GL_SCISSOR_TEST);
						glScissor(0, 0, scaled_offs_x + 0.5f, screen_height * screen_scaling + 0.5f);
						glClear(GL_COLOR_BUFFER_BIT);
						glScissor(screen_width * screen_scaling - scaled_offs_x + 0.5f, 0, scaled_offs_x + 1.f, screen_height * screen_scaling + 0.5f);
						glClear(GL_COLOR_BUFFER_BIT);
					}
				}
				else if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
				{
					min_x *= settings.rend.RenderToTextureUpscale;
					min_y *= settings.rend.RenderToTextureUpscale;
					width *= settings.rend.RenderToTextureUpscale;
					height *= settings.rend.RenderToTextureUpscale;
				}

				glScissor(min_x + 0.5f, min_y + 0.5f, width + 0.5f, height + 0.5f);
				glcache.Enable(GL_SCISSOR_TEST);
			}

			//restore scale_x
			scale_x /= scissoring_scale_x;

			DrawStrips();
		}
		else
		{
			glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
			glClear(GL_COLOR_BUFFER_BIT);
			DrawFramebuffer(dc_width, dc_height);
			glBufferData(GL_ARRAY_BUFFER, pvrrc.verts.bytes(), pvrrc.verts.head(), GL_STREAM_DRAW);
			upload_vertex_indices();
		}
#if HOST_OS==OS_WINDOWS
		//Sleep(40); //to test MT stability
#endif

		eglCheck();

		KillTex = false;

		if (is_rtt)
			ReadRTTBuffer();
		else if (settings.rend.ScreenScaling != 100 || gl.swap_buffer_not_preserved)
			render_output_framebuffer();

		return !is_rtt;
#endif
		return false;
	}

	bool RenderLastFrame() { return render_output_framebuffer(); }
	
	void Present()
	{
		gl_swap();
		glViewport(0, 0, screen_width, screen_height);
	}

	void DrawOSD(bool clear_screen)
	{
;
	}

	virtual u32 GetTexture(TSP tsp, TCW tcw) {
		return gl_GetTexture(tsp, tcw);
	}
};


#include "deps/libpng/png.h"

FILE* pngfile;

void png_cstd_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	fread(data, 1, length, pngfile);
}

u8* loadPNGData(const string& fname, int& width, int& height)
{
	const char* filename = fname.c_str();
	FILE* file = fopen(filename, "rb");
	pngfile = file;

	if (!file)
	{
		EMUERROR("Error opening %s\n", filename);
		return NULL;
	}

	//header for testing if it is a png
	png_byte header[8];

	//read the header
	fread(header, 1, 8, file);

	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png)
	{
		fclose(file);
		printf("Not a PNG file : %s\n", filename);
		return NULL;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
		NULL, NULL);
	if (!png_ptr)
	{
		fclose(file);
		printf("Unable to create PNG struct : %s\n", filename);
		return (NULL);
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		printf("Unable to create PNG info : %s\n", filename);
		fclose(file);
		return (NULL);
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		printf("Unable to create PNG end info : %s\n", filename);
		fclose(file);
		return (NULL);
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fclose(file);
		printf("Error during setjmp : %s\n", filename);
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
	png_byte* image_data = new png_byte[rowbytes * height];
	if (!image_data)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		printf("Unable to allocate image_data while loading %s\n", filename);
		fclose(file);
		return NULL;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep* row_pointers = new png_bytep[height];
	if (!row_pointers)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		delete[] image_data;
		printf("Unable to allocate row_pointer while loading %s\n", filename);
		fclose(file);
		return NULL;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	for (int i = 0; i < height; ++i)
		row_pointers[height - 1 - i] = image_data + i * rowbytes;

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	delete[] row_pointers;
	fclose(file);

	return image_data;
}

GLuint loadPNG(const string& fname, int& width, int& height)
{
	png_byte* image_data = loadPNGData(fname, width, height);
	if (image_data == NULL)
		return TEXTURE_LOAD_ERROR;

	//Now generate the OpenGL texture object
	GLuint texture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, (GLvoid*)image_data);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	delete[] image_data;

	return texture;
}


Renderer* rend_GLES2() { return new glesrend(); }
