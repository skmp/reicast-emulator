#include "gles.h"
#include "../rend.h"

/*

Drawing and related state management
Takes vertex, textures and renders to the currently set up target
*/

const static u32 CullMode[]= 
{

	GL_NONE, //0    No culling          No culling
	GL_NONE, //1    Cull if Small       Cull if ( |det| < fpu_cull_val )

	GL_FRONT, //2   Cull if Negative    Cull if ( |det| < 0 ) or ( |det| < fpu_cull_val )
	GL_BACK,  //3   Cull if Positive    Cull if ( |det| > 0 ) or ( |det| < fpu_cull_val )
};
#define INVERT_DEPTH_FUNC
const static u32 Zfunction[]=
{
	GL_NEVER,      //GL_NEVER,              //0 Never
#ifndef INVERT_DEPTH_FUNC
	GL_LESS,        //GL_LESS/*EQUAL*/,     //1 Less
	GL_EQUAL,       //GL_EQUAL,             //2 Equal
	GL_LEQUAL,      //GL_LEQUAL,            //3 Less Or Equal
	GL_GREATER,     //GL_GREATER/*EQUAL*/,  //4 Greater
	GL_NOTEQUAL,    //GL_NOTEQUAL,          //5 Not Equal
	GL_GEQUAL,      //GL_GEQUAL,            //6 Greater Or Equal
#else
    GL_GREATER,        //GL_LESS/*EQUAL*/,     //1 Less
    GL_EQUAL,       //GL_EQUAL,             //2 Equal
    GL_GEQUAL,      //GL_LEQUAL,            //3 Less Or Equal
    GL_LESS,     //GL_GREATER/*EQUAL*/,  //4 Greater
    GL_NOTEQUAL,    //GL_NOTEQUAL,          //5 Not Equal
    GL_LEQUAL,      //GL_GEQUAL,            //6 Greater Or Equal
#endif
	GL_ALWAYS,      //GL_ALWAYS,            //7 Always
};

/*
0   Zero                  (0, 0, 0, 0)
1   One                   (1, 1, 1, 1)
2   Dither Color          (OR, OG, OB, OA) 
3   Inverse Dither Color  (1-OR, 1-OG, 1-OB, 1-OA)
4   SRC Alpha             (SA, SA, SA, SA)
5   Inverse SRC Alpha     (1-SA, 1-SA, 1-SA, 1-SA)
6   DST Alpha             (DA, DA, DA, DA)
7   Inverse DST Alpha     (1-DA, 1-DA, 1-DA, 1-DA)
*/

const static u32 DstBlendGL[] =
{
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

const static u32 SrcBlendGL[] =
{
	GL_ZERO,
	GL_ONE,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

PipelineShader* CurrentShader;
extern u32 gcflip;
GLuint geom_fbo;
GLuint stencilTexId;
GLuint opaqueTexId;
GLuint depthTexId;

s32 SetTileClip(u32 val, bool set)
{
	float csx=0,csy=0,cex=0,cey=0;
	u32 clipmode=val>>28;
	s32 clip_mode;
	if (clipmode<2)
		clip_mode=0;    //always passes
	else if (clipmode&1)
		clip_mode=-1;   //render stuff outside the region
	else
		clip_mode=1;    //render stuff inside the region

	csx=(float)(val&63);
	cex=(float)((val>>6)&63);
	csy=(float)((val>>12)&31);
	cey=(float)((val>>17)&31);
	csx=csx*32;
	cex=cex*32 +32;
	csy=csy*32;
	cey=cey*32 +32;

	if (csx <= 0 && csy <= 0 && cex >= 640 && cey >= 480)
		return 0;
	
	if (set && clip_mode)
   {
      if (!pvrrc.isRTT)
      {
         csx *= scale_x;
         csy *= scale_y;
         cex *= scale_x;
         cey *= scale_y;
         float t = cey;
         cey = 480 - csy;
         csy = 480 - t;
         float dc2s_scale_h = screen_height / 480.0f;
         float ds2s_offs_x = (screen_width - dc2s_scale_h * 640) / 2;
         csx = csx * dc2s_scale_h + ds2s_offs_x;
         cex = cex * dc2s_scale_h + ds2s_offs_x;
         csy = csy * dc2s_scale_h;
         cey = cey * dc2s_scale_h;
      }
		glUniform4f(CurrentShader->pp_ClipTest, csx, csy, cex, cey);		
   }

	return clip_mode;
}

void SetCull(u32 CulliMode)
{
	if (CullMode[CulliMode] == GL_NONE)
		glcache.Disable(GL_CULL_FACE);
	else
	{
		glcache.Enable(GL_CULL_FACE);
		glcache.CullFace(CullMode[CulliMode]); //GL_FRONT/GL_BACK, ...
	}
}

static void SetTextureRepeatMode(GLuint dir, u32 clamp, u32 mirror)
{
	if (clamp)
		glcache.TexParameteri(GL_TEXTURE_2D, dir, GL_CLAMP_TO_EDGE);
	else
		glcache.TexParameteri(GL_TEXTURE_2D, dir, mirror ? GL_MIRRORED_REPEAT : GL_REPEAT);
}

template <u32 Type, bool SortingEnabled>
__forceinline void SetGPState(const PolyParam* gp, int pass, u32 cflip=0)
{
   s32 clipping = SetTileClip(gp->tileclip, false);
   int shaderId;

   if (pass == 0)
	{
		shaderId = GetProgramID(Type == ListType_Punch_Through ? 1 : 0,
				clipping + 1,
				Type == ListType_Punch_Through ? gp->pcw.Texture : 0,
				1,
				gp->tsp.IgnoreTexA,
				0,
				0,
				2,
				pass);

		CurrentShader = gl.getShader(shaderId);
		if (CurrentShader->program == -1) {
			CurrentShader->cp_AlphaTest = Type == ListType_Punch_Through ? 1 : 0;
			CurrentShader->pp_ClipTestMode = clipping;
			CurrentShader->pp_Texture = Type == ListType_Punch_Through ? gp->pcw.Texture : 0;
			CurrentShader->pp_UseAlpha = 1;
			CurrentShader->pp_IgnoreTexA = gp->tsp.IgnoreTexA;
			CurrentShader->pp_ShadInstr = 0;
			CurrentShader->pp_Offset = 0;
			CurrentShader->pp_FogCtrl = 2;
         CurrentShader->pass = pass;

			CompilePipelineShader(CurrentShader);
		}
	}
	else
   {
      shaderId = GetProgramID(Type == ListType_Punch_Through ? 1 : 0,
               SetTileClip(gp->tileclip, false) + 1,
               gp->pcw.Texture,
               gp->tsp.UseAlpha,
               gp->tsp.IgnoreTexA,
               gp->tsp.ShadInstr,
               gp->pcw.Offset,
               /*gp->tsp.FogCtrl*/2,
               pass);
      CurrentShader = gl.getShader(shaderId);

      if (CurrentShader->program == -1) {
         CurrentShader->cp_AlphaTest = Type == ListType_Punch_Through ? 1 : 0;
         CurrentShader->pp_ClipTestMode = SetTileClip(gp->tileclip, false);
         CurrentShader->pp_Texture = gp->pcw.Texture;
         CurrentShader->pp_UseAlpha = gp->tsp.UseAlpha;
         CurrentShader->pp_IgnoreTexA = gp->tsp.IgnoreTexA;
         CurrentShader->pp_ShadInstr = gp->tsp.ShadInstr;
         CurrentShader->pp_Offset = gp->pcw.Offset;
         CurrentShader->pp_FogCtrl = /*gp->tsp.FogCtrl*/2;
         CurrentShader->pass       = pass;

         CompilePipelineShader(CurrentShader);
      }
   }

   glcache.UseProgram(CurrentShader->program);
   if (Type == ListType_Opaque || Type == ListType_Punch_Through)	// TODO Can PT have a >0 and <1 alpha?
	{
		ShaderUniforms.blend_mode[0] = 1;
		ShaderUniforms.blend_mode[1] = 0;
	}
	else
	{
		ShaderUniforms.blend_mode[0] = gp->tsp.SrcInstr;
		ShaderUniforms.blend_mode[1] = gp->tsp.DstInstr;
	}
   ShaderUniforms.Set(CurrentShader);

   SetTileClip(gp->tileclip,true);

   // This bit controls which pixels are affected
   // by modvols
   const u32 stencil = (gp->pcw.Shadow!=0)?0x80:0;
   glcache.StencilFunc(GL_ALWAYS, stencil, stencil);

   if (CurrentShader->pp_Texture)
   {
      glcache.BindTexture(GL_TEXTURE_2D, gp->texid == -1 ? 0 : gp->texid);

      if (gp->texid > 0)
      {
         SetTextureRepeatMode(GL_TEXTURE_WRAP_S, gp->tsp.ClampU, gp->tsp.FlipU);
         SetTextureRepeatMode(GL_TEXTURE_WRAP_T, gp->tsp.ClampV, gp->tsp.FlipV);

         //set texture filter mode

         if (gp->tsp.FilterMode == 0)
         {
            //disable filtering, mipmaps
            glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
         }
         else
         {
            //bilinear filtering
            //PowerVR supports also trilinear via two passes, but we ignore that for now
            glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (gp->tcw.MipMapped && settings.rend.UseMipmaps) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
            glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         }
      }
   }

   if (Type== ListType_Translucent && !SortingEnabled)
   {
      glcache.Enable(GL_BLEND);
      glcache.BlendFunc(SrcBlendGL[gp->tsp.SrcInstr], DstBlendGL[gp->tsp.DstInstr]);
   }
   else
      glcache.Disable(GL_BLEND);

   //set cull mode !
   //cflip is required when exploding triangles for triangle sorting
   //gcflip is global clip flip, needed for when rendering to texture due to mirrored Y direction
   SetCull(gp->isp.CullMode ^ cflip ^ gcflip);

   /* Set Z mode, only if required */
   if (Type == ListType_Punch_Through || (Type == ListType_Translucent && SortingEnabled))
   {
      if (gp->isp.DepthMode == 7) {		// Fixes VR2 menu but not sure about this one
         glcache.DepthFunc(GL_ALWAYS);
      }
      else
      {
         glcache.DepthFunc(Zfunction[6]); // Greater or equal
         //			glcache.DepthFunc(GL_LESS);
      }
   }
   else
   {
      glcache.DepthFunc(Zfunction[gp->isp.DepthMode]);
   }

   //if (Type == ListType_Translucent)
      //glcache.DepthMask(GL_FALSE);
   //else
      glcache.DepthMask(!gp->isp.ZWriteDis);
}

template <u32 Type, bool SortingEnabled>
void DrawList(const List<PolyParam>& gply, int first, int count, int pass)
{
   PolyParam* params= &gply.head()[first];

   /* We want at least 1 PParam */
   if (count==0)
      return;

   /* set some 'global' modes for all primitives */
   if (pass == 0)
   {
      glcache.Enable(GL_STENCIL_TEST);
      glcache.StencilFunc(GL_ALWAYS,0,0);
      glcache.StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
   }
   else
   {
      glcache.StencilMask(0);
      glcache.Disable(GL_STENCIL_TEST);
   }

   while(count-->0)
   {
      if (params->count>2) /* this actually happens for some games. No idea why .. */
      {
         if (Type == ListType_Translucent && params->tsp.SrcInstr == 0 && params->tsp.DstInstr == 1)
         {
            // No-op
            params++;
            continue;
         }
         ShaderUniforms.poly_number = params - gply.head() + 1;
         SetGPState<Type,SortingEnabled>(params, pass);

         glDrawElements(GL_TRIANGLE_STRIP, params->count, GL_UNSIGNED_SHORT, (GLvoid*)(2*params->first));
      }

      params++;
   }
}

//All pixels are in area 0 by default.
//If inside an 'in' volume, they are in area 1
//if inside an 'out' volume, they are in area 0
/*
	Stencil bits:
		bit 7: mv affected (must be preserved)
		bit 1: current volume state
		but 0: summary result (starts off as 0)

	Lower 2 bits:

	IN volume (logical OR):
	00 -> 00
	01 -> 01
	10 -> 01
	11 -> 01

	Out volume (logical AND):
	00 -> 00
	01 -> 00
	10 -> 00
	11 -> 01
*/
void SetMVS_Mode(u32 mv_mode,ISP_Modvol ispc)
{
	if (mv_mode==0)	//normal trigs
	{
		//set states
		glcache.Enable(GL_DEPTH_TEST);
		//write only bit 1
      glcache.StencilMask(2);
      //no stencil testing
      glcache.StencilFunc(GL_ALWAYS, 0, 2);
		//count the number of pixels in front of the Z buffer (and only keep the lower bit of the count)
      glcache.StencilOp(GL_KEEP, GL_KEEP, GL_INVERT);

		//Cull mode needs to be set
		SetCull(ispc.CullMode);
	}
	else
	{
		//1 (last in) or 2 (last out)
		//each triangle forms the last of a volume

		//common states

		//no depth test
		glcache.Disable(GL_DEPTH_TEST);

      //write bits 1:0
      glcache.StencilMask(3);

		if (mv_mode==1)
		{
         // Inclusion volume
			//res : old : final 
			//0   : 0      : 00
			//0   : 1      : 01
			//1   : 0      : 01
			//1   : 1      : 01
			
			//if (1<=st) st=1; else st=0;
         glcache.StencilFunc(GL_LEQUAL, 1, 3);

         glcache.StencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);

			/*
			//if !=0 -> set to 10
			verifyc(dev->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_LESSEQUAL));
			verifyc(dev->SetRenderState(D3DRS_STENCILREF,1));					
			verifyc(dev->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_REPLACE));
			verifyc(dev->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_ZERO));
			*/
		}
		else
		{
         // Exclusion volume
			/*
				I've only seen a single game use it, so i guess it doesn't matter ? (Zombie revenge)
				(actually, i think there was also another, racing game)
			*/

         // The initial value for exclusion volumes is 1 so we need to invert the result before and'ing.
			//res : old : final 
			//0   : 0   : 00
			//0   : 1   : 01
			//1   : 0   : 00
			//1   : 1   : 00

			//if (1 == st) st = 1; else st = 0;
         glcache.StencilFunc(GL_EQUAL, 1, 3);
         glcache.StencilOp(GL_ZERO, GL_ZERO, GL_REPLACE);
		}
	}
}

void SetupMainVBO(void)
{
	glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs);

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x));

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col));

	glEnableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glVertexAttribPointer(VERTEX_COL_OFFS_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,vtx_spc));

	glEnableVertexAttribArray(VERTEX_UV_ARRAY);
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u));
}

static void SetupModvolVBO(void)
{
	glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.modvols);

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);

	glDisableVertexAttribArray(VERTEX_UV_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glDisableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
}

void DrawModVols(int first, int count)
{
   /* A bit of explanation:
    * In theory it works like this: generate a 1-bit stencil for each polygon
    * volume, and then AND or OR it against the overall 1-bit tile stencil at 
    * the end of the volume. */

   if (count == 0 || !settings.pvr.Emulation.ModVol)
      return;

   SetupModvolVBO();

   glcache.UseProgram(gl.modvol_shader.program);

   glcache.DepthMask(GL_FALSE);
   glcache.DepthFunc(Zfunction[4]);

   /*
mode :
normal trig : flip
last *in*   : flip, merge*in* &clear from last merge
last *out*  : flip, merge*out* &clear from last merge
*/

   /*

      Do not write to color
      Do not write to depth

      read from stencil bits 1:0
      write to stencil bits 1:0
      */

   glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

   {
      glEnable(GL_STENCIL_TEST);
      //Full emulation
      //the *out* mode is buggy

      u32 mod_base=0; //cur start triangle
      u32 mod_last=0; //last merge

      u32 cmv_count= count -1;
      ISP_Modvol* params= &pvrrc.global_param_mvo.head()[first];

      //ISP_Modvol
      for (u32 cmv=0;cmv<cmv_count;cmv++)
      {

         ISP_Modvol ispc=params[cmv];
         mod_base=ispc.id;
         u32 sz=params[cmv+1].id-mod_base;

         if (sz == 0)
            continue;

         u32 mv_mode = ispc.DepthMode;

         if (mv_mode==0)	//normal trigs
         {
            SetMVS_Mode(0,ispc);
            //Render em (counts intersections)
            //verifyc(dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,sz,pvrrc.modtrig.data+mod_base,3*4));
            glDrawArrays(GL_TRIANGLES,mod_base*3,sz*3);
         }
         else if (mv_mode<3)
         {
            while(sz)
            {
               //merge and clear all the prev. stencil bits

               //Count Intersections (last poly)
               SetMVS_Mode(0,ispc);
               glDrawArrays(GL_TRIANGLES,mod_base*3,3);

               //Sum the area
               SetMVS_Mode(mv_mode,ispc);
               glDrawArrays(GL_TRIANGLES,mod_last*3,(mod_base-mod_last+1)*3);

               //update pointers
               mod_last=mod_base+1;
               sz--;
               mod_base++;
            }
         }
      }
   }

   SetupMainVBO();

   //restore states
	glcache.Enable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_TRUE);
}

void renderABuffer(bool sortFragments);
void renderPass2(GLuint textureId, GLuint depthTexId);

void CreateGeometryTexture()
{
	glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);

	stencilTexId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, stencilTexId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);		// OpenGL >= 4.3
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// Using glTexStorage2D instead of glTexImage2D to satisfy requirement GL_TEXTURE_IMMUTABLE_FORMAT=true, needed for glTextureView below
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH32F_STENCIL8, screen_width, screen_height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTexId, 0);

	opaqueTexId = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, opaqueTexId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueTexId, 0);

	depthTexId = glcache.GenTexture();
	glTextureView(depthTexId, GL_TEXTURE_2D, stencilTexId, GL_DEPTH32F_STENCIL8, 0, 1, 0, 1);
	glcache.BindTexture(GL_TEXTURE_2D, depthTexId);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void DrawStrips(void)
{
   if (geom_fbo == 0)
   {
      glGenFramebuffers(1, &geom_fbo);
      CreateGeometryTexture();

		GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		verify(uStatus == GL_FRAMEBUFFER_COMPLETE);
   }
   else
   {
      glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);
		if (stencilTexId == 0)
		{
         CreateGeometryTexture();
		}
      glcache.ClearColor(0, 0, 0, 0);
		glcache.Disable(GL_SCISSOR_TEST);
		glcache.DepthMask(GL_TRUE);
		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   }



	SetupMainVBO();
	//Draw the strips !


	//We use sampler 0
   glActiveTexture(GL_TEXTURE0);

   RenderPass previous_pass = {0};
   for (int render_pass = 0; render_pass < pvrrc.render_passes.used(); render_pass++)
   {
      const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];

      //initial state
      glcache.Enable(GL_DEPTH_TEST);
      glcache.DepthMask(GL_TRUE);
      glcache.Disable(GL_BLEND);

      //
      // PASS 0: Geometry pass to update the depth and stencil
      //
      glBindFramebuffer(GL_FRAMEBUFFER, geom_fbo);

      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      DrawList<ListType_Opaque, false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count, 0);
      DrawList<ListType_Punch_Through, false>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count, 0);

      // Modifier volumes
      DrawModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
      //
      // PASS 1: Render OP and PT to fbo
      //      
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

      // Bind stencil buffer for the fragment shader (shadowing)
      glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, stencilTexId);

		glActiveTexture(GL_TEXTURE0);

      //Opaque
 		DrawList<ListType_Opaque,false>(pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count, 1);
 	 
 		//Alpha tested
 		DrawList<ListType_Punch_Through,false>(pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count, 1);

      //
      // PASS 2: Render opaque and PT texture to a-buffers along with depth
      //
      // Unbind stencil
      glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);

      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      renderPass2(opaqueTexId, depthTexId);

      //
      // PASS 3: Render TR to a-buffers
      //
      SetupMainVBO();

      //Alpha blended
         DrawList<ListType_Translucent,true>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count, 3);

         // FIXME Unsorted TR cannot use a-buffer because of lost ordering
         // FIXME Blinking pixels in Soulcalibur score table. Could be that some TR have same depth and rely on natural order?
         // a-buffers makes the final order unpredictable and varies each frame
         // FIXME Depth of translucent poly must be used for next render pass if any
         // FIXME Multipass in general...

      previous_pass = current_pass;
   }

   //
   // PASS 4: Render a-buffers to screen
   //
   glBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	renderABuffer(pvrrc.isAutoSort);
   
   vertex_buffer_unmap();
}
