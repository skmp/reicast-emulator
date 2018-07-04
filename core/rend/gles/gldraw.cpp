#include "gles.h"
#include "../rend.h"

#include <algorithm>

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
   GL_GREATER,     //GL_LESS/*EQUAL*/,     //1 Less
   GL_EQUAL,       //GL_EQUAL,             //2 Equal
   GL_GEQUAL,      //GL_LEQUAL,            //3 Less Or Equal
   GL_LESS,        //GL_GREATER/*EQUAL*/,  //4 Greater
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

extern int screen_width;
extern int screen_height;

PipelineShader* CurrentShader;
extern u32 gcflip;

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
__forceinline void SetGPState(const PolyParam* gp, u32 cflip)
{
   if (gp->pcw.Texture && gp->tsp.FilterMode > 1)
	{
		ShaderUniforms.trilinear_alpha = 0.25 * (gp->tsp.MipMapD & 0x3);
		if (gp->tsp.FilterMode == 2)
			// Trilinear pass A
			ShaderUniforms.trilinear_alpha = 1.0 - ShaderUniforms.trilinear_alpha;
	}
	else
		ShaderUniforms.trilinear_alpha = 1.0;

   CurrentShader = &gl.program_table[
									 GetProgramID(Type == ListType_Punch_Through ? 1 : 0,
											 	  SetTileClip(gp->tileclip, false) + 1,
												  gp->pcw.Texture,
												  gp->tsp.UseAlpha,
												  gp->tsp.IgnoreTexA,
												  gp->tsp.ShadInstr,
												  gp->pcw.Offset,
												  gp->tsp.FogCtrl,
                                      gp->pcw.Gouraud,
                                      gp->tcw.PixelFmt == 4)];

   if (CurrentShader->program == -1)
      CompilePipelineShader(CurrentShader);
   else
   {
      glcache.UseProgram(CurrentShader->program);
      ShaderUniforms.Set(CurrentShader);
   }
   SetTileClip(gp->tileclip,true);

   // This bit controls which pixels are affected
   // by modvols
   const u32 stencil = (gp->pcw.Shadow!=0)?0x80:0;
   glcache.StencilFunc(GL_ALWAYS, stencil, stencil);

   glcache.BindTexture(GL_TEXTURE_2D, gp->texid == -1 ? 0 : gp->texid);
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

   if (Type== ListType_Translucent)
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
      glcache.DepthFunc(GL_GEQUAL);
   }
   else
   {
      glcache.DepthFunc(Zfunction[gp->isp.DepthMode]);
   }

   if (SortingEnabled && settings.pvr.Emulation.AlphaSortMode == 0)
      glcache.DepthMask(GL_FALSE);
   else
      glcache.DepthMask(!gp->isp.ZWriteDis);
}

template <u32 Type, bool SortingEnabled>
void DrawList(const List<PolyParam>& gply, int first, int count)
{
   PolyParam* params= &gply.head()[first];

   /* We want at least 1 PParam */
   if (count==0)
      return;

   /* set some 'global' modes for all primitives */
   glcache.Enable(GL_STENCIL_TEST);
   glcache.StencilFunc(GL_ALWAYS,0,0);
   glcache.StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);

   while(count-->0)
   {
      if (params->count>2) /* this actually happens for some games. No idea why .. */
      {
         SetGPState<Type,SortingEnabled>(params, 0);
         glDrawElements(GL_TRIANGLE_STRIP, params->count, GL_UNSIGNED_SHORT, (GLvoid*)(2*params->first));
      }

      params++;
   }
}

bool operator<(const PolyParam &left, const PolyParam &right)
{
   /* put any condition you want to sort on here */
	return left.zvZ  < right.zvZ;
#if 0
	return left.zMin < right.zMax;
#endif
}

//Sort based on min-z of each strip
void SortPParams(int first, int count)
{
   u16 *idx_base      = NULL;
   Vertex *vtx_base   = NULL;
   PolyParam *pp      = NULL;
   PolyParam *pp_end  = NULL;

   if (pvrrc.verts.used()==0 || count <=1)
      return;

   vtx_base          = pvrrc.verts.head();
   idx_base          = pvrrc.idx.head();
   pp                = &pvrrc.global_param_tr.head()[first];
   pp_end            = pp + count;

   while(pp!=pp_end)
   {
      if (pp->count<2)
         pp->zvZ=0;
      else
      {
         u16*      idx   = idx_base+pp->first;
         Vertex*   vtx   = vtx_base+idx[0];
         Vertex* vtx_end = vtx_base + idx[pp->count-1]+1;
         u32 zv          = 0xFFFFFFFF;

         while(vtx!=vtx_end)
         {
            zv = min(zv,(u32&)vtx->z);
            vtx++;
         }

         pp->zvZ=(f32&)zv;
      }
      pp++;
   }

   std::stable_sort(pvrrc.global_param_tr.head() + first,
         pvrrc.global_param_tr.head() + first + count);
}

Vertex* vtx_sort_base;

struct IndexTrig
{
	u16 id[3];
	u16 pid;
	f32 z;
};

struct SortTrigDrawParam
{
	PolyParam* ppid;
	u16 first;
	u16 count;
};

float min3(float v0,float v1,float v2)
{
	return min(min(v0,v1),v2);
}

float max3(float v0,float v1,float v2)
{
	return max(max(v0,v1),v2);
}

float minZ(Vertex* v,u16* mod)
{
	return min(min(v[mod[0]].z,v[mod[1]].z),v[mod[2]].z);
}

bool operator<(const IndexTrig &left, const IndexTrig &right)
{
	return left.z<right.z;
}

//are two poly params the same?
bool PP_EQ(PolyParam* pp0, PolyParam* pp1)
{
   return 
      (pp0->pcw.full&PCW_DRAW_MASK)==(pp1->pcw.full&PCW_DRAW_MASK) 
      && pp0->isp.full==pp1->isp.full 
      && pp0->tcw.full==pp1->tcw.full
      && pp0->tsp.full==pp1->tsp.full
      && pp0->tileclip==pp1->tileclip;
}

static vector<SortTrigDrawParam>	pidx_sort;

void GenSorted(int first, int count)
{
   static vector<IndexTrig> lst;
   static vector<u16> vidx_sort;

   static u32 vtx_cnt;
   int idx            = -1;
   int pfsti          =  0;

   pidx_sort.clear();

   if (pvrrc.verts.used()==0 || count <=1)
      return;

   Vertex* vtx_base=pvrrc.verts.head();
   u16* idx_base=pvrrc.idx.head();

   PolyParam* pp_base= &pvrrc.global_param_tr.head()[first];
   PolyParam* pp=pp_base;
   PolyParam* pp_end= pp + count;

   Vertex* vtx_arr=vtx_base+idx_base[pp->first];
   vtx_sort_base=vtx_base;
   int vtx_count=idx_base[pp_end[-1].first+pp_end[-1].count-1]-idx_base[pp->first];
   if (vtx_count>vtx_cnt)
      vtx_cnt=vtx_count;

#if PRINT_SORT_STATS
   printf("TVTX: %d || %d\n",vtx_cnt,vtx_count);
#endif

   if (vtx_count<=0)
      return;

   /* Make lists of all triangles, with their PID and VID */

   lst.resize(vtx_count*4);

   while(pp != pp_end)
   {
      Vertex *vtx     = NULL;
      Vertex *vtx_end = NULL;
      u16 *idx        = NULL;
      u32 flip        = 0;
      u32 ppid        = (pp-pp_base);

      if (pp->count <= 2)
      {
         pp++;
         continue;
      }

      idx             = idx_base + pp->first;
      vtx             = vtx_base+idx[0];
      vtx_end         = vtx_base + idx[pp->count-1]-1;

      while(vtx != vtx_end)
      {
         Vertex *v0   = &vtx[0];
         Vertex *v1   = &vtx[1];
         Vertex *v2   = &vtx[2];

         if (flip)
         {
            v0=&vtx[2];
            v1=&vtx[1];
            v2=&vtx[0];
         }

         u16* d =lst[pfsti].id;
         Vertex *vb = vtx_base;
         d[0]=v0-vb;
         d[1]=v1-vb;
         d[2]=v2-vb;

         lst[pfsti].pid= ppid ;
         lst[pfsti].z = pp->isp.DepthMode == 7 ? 10000000.0 : minZ(vtx_base,lst[pfsti].id);
         pfsti++;

         flip ^= 1;
         vtx++;
      }
      pp++;
   }

   u32 aused=pfsti;

   lst.resize(aused);

   /* sort them */
   std::stable_sort(lst.begin(),lst.end());

   /* Merge PIDs/draw commands if two different PIDs are actually equal */

   for (u32 k = 1; k < aused; k++)
   {
      if (lst[k].pid == lst[k-1].pid)
         continue;

      if (PP_EQ(&pp_base[lst[k].pid],&pp_base[lst[k-1].pid]))
         lst[k].pid=lst[k-1].pid;
   }

   /* Reassemble vertex indices into drawing commands */

   vidx_sort.resize(aused*3);

   for (u32 i=0; i<aused; i++)
   {
      SortTrigDrawParam stdp;
      int   pid          = lst[i].pid;
      u16* midx          = lst[i].id;

      vidx_sort[i*3 + 0] = midx[0];
      vidx_sort[i*3 + 1] = midx[1];
      vidx_sort[i*3 + 2] = midx[2];

      if (idx == pid)
         continue;

      stdp.ppid  = pp_base + pid;
      stdp.first = (u16)(i*3);
      stdp.count = 0;

      if (idx!=-1)
      {
         SortTrigDrawParam *last = &pidx_sort[pidx_sort.size()-1];

         if (last)
            last->count=stdp.first-last->first;
      }

      pidx_sort.push_back(stdp);
      idx=pid;
   }

   SortTrigDrawParam *stdp = &pidx_sort[pidx_sort.size()-1];

   if (stdp)
      stdp->count=aused*3-stdp->first;

#if PRINT_SORT_STATS
   printf("Reassembled into %d from %d\n",pidx_sort.size(),pp_end-pp_base);
#endif

   /* Upload to GPU if needed, otherwise return */
   if (pidx_sort.size())
   {
      /* Bind and upload sorted index buffer */
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs2);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,vidx_sort.size()*2,&vidx_sort[0],GL_STREAM_DRAW);
   }
}

void DrawSorted(u32 count)
{
   //if any drawing commands, draw them
	if (pidx_sort.size())
   {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs2);
      u32 count=pidx_sort.size();

      {
         //set some 'global' modes for all primitives

         glcache.Enable(GL_STENCIL_TEST);
         glcache.StencilFunc(GL_ALWAYS, 0, 0);
         glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

         for (u32 p=0; p<count; p++)
         {
            PolyParam* params = pidx_sort[p].ppid;
            if (pidx_sort[p].count>2) //this actually happens for some games. No idea why ..
            {
               SetGPState<ListType_Translucent, true>(params, 0);
               glDrawElements(GL_TRIANGLES, pidx_sort[p].count, GL_UNSIGNED_SHORT, (GLvoid*)(2*pidx_sort[p].first));
            }
            params++;
         }

         if (settings.rend.TranslucentPolygonDepthMask)
			{
				// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
				glcache.Disable(GL_BLEND);

				glcache.StencilMask(0);

				// We use the modifier volumes shader because it's fast. We don't need textures, etc.
				glcache.UseProgram(gl.modvol_shader.program);
				glUniform1f(gl.modvol_shader.sp_ShaderColor, 1.f);

				glcache.DepthFunc(GL_GEQUAL);
				glcache.DepthMask(GL_TRUE);

				for (u32 p = 0; p < count; p++)
				{
					PolyParam* params = pidx_sort[p].ppid;
					if (pidx_sort[p].count > 2 && !params->isp.ZWriteDis) {
						// FIXME no clipping in modvol shader
						//SetTileClip(gp->tileclip,true);

						SetCull(params->isp.CullMode ^ gcflip);

						glDrawElements(GL_TRIANGLES, pidx_sort[p].count, GL_UNSIGNED_SHORT, (GLvoid*)(2 * pidx_sort[p].first));
					}
				}
				glcache.StencilMask(0xFF);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}

      }
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
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc)
{
	if (mv_mode == Xor)
	{
		// set states
		glcache.Enable(GL_DEPTH_TEST);
		// write only bit 1
      glcache.StencilMask(2);
      // no stencil testing
      glcache.StencilFunc(GL_ALWAYS, 0, 2);
		// count the number of pixels in front of the Z buffer (xor zpass)
      glcache.StencilOp(GL_KEEP, GL_KEEP, GL_INVERT);

		//Cull mode needs to be set
		SetCull(ispc.CullMode);
	}
	else if (mv_mode == Or)
   {
      // set states
      glcache.Enable(GL_DEPTH_TEST);
		// write only bit 1
      glcache.StencilMask(2);
      // no stencil testing
      glcache.StencilFunc(GL_ALWAYS, 2, 2);
      // Or'ing of all triangles
      glcache.StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

      // Cull mode needs to be set
      SetCull(ispc.CullMode);
   }
   else
	{
      // Inclusion or Exclusion volume

		// no depth test
		glcache.Disable(GL_DEPTH_TEST);
      // write bits 1:0
      glcache.StencilMask(3);

		if (mv_mode == Inclusion)
		{
         // Inclusion volume
			//res : old : final 
			//0   : 0      : 00
			//0   : 1      : 01
			//1   : 0      : 01
			//1   : 1      : 01
			
			// if (1<=st) st=1; else st=0;
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

			// if (1 == st) st = 1; else st = 0;
         glcache.StencilFunc(GL_EQUAL, 1, 3);
         glcache.StencilOp(GL_ZERO, GL_ZERO, GL_KEEP);
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

   glcache.Enable(GL_BLEND);
   glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glcache.UseProgram(gl.modvol_shader.program);
   glUniform1f(gl.modvol_shader.sp_ShaderColor, 1 - FPU_SHAD_SCALE.scale_factor / 256.f);

   glcache.DepthMask(GL_FALSE);
   glcache.DepthFunc(Zfunction[4]);

   {
      //Full emulation
      //
      glcache.Enable(GL_STENCIL_TEST);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

      ModifierVolumeParam* params = &pvrrc.global_param_mvo.head()[first];

      int mod_base = -1;

      for (u32 cmv = 0; cmv < count; cmv++)
      {
         ModifierVolumeParam& param = params[cmv];

         if (param.count == 0)
            continue;

         u32 mv_mode = param.isp.DepthMode;

         if (mod_base == -1)
				mod_base = param.first;

         if (!param.isp.VolumeLast && mv_mode > 0)
            SetMVS_Mode(Or, param.isp);		// OR'ing (open volume or quad)
         else
            SetMVS_Mode(Xor, param.isp);	// XOR'ing (closed volume)
         glDrawArrays(GL_TRIANGLES, param.first * 3, param.count * 3);

         if (mv_mode == 1 || mv_mode == 2)
         {
            // Sum the area
            SetMVS_Mode(mv_mode == 1 ? Inclusion : Exclusion, param.isp);
            glDrawArrays(GL_TRIANGLES, mod_base * 3, (param.first + param.count - mod_base) * 3);
            mod_base = -1;
         }
      }
      //disable culling
      SetCull(0);
      //enable color writes
      glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

      //black out any stencil with '1'
      glcache.Enable(GL_BLEND);
      glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glcache.Enable(GL_STENCIL_TEST);
      glcache.StencilFunc(GL_EQUAL, 0x81, 0x81); //only pixels that are Modvol enabled, and in area 1

      //clear the stencil result bit
      glcache.StencilMask(0x3); /* write to LSB */
      glcache.StencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

      //don't do depth testing
      glcache.Disable(GL_DEPTH_TEST);

      SetupMainVBO();
      glDrawArrays(GL_TRIANGLE_STRIP,0,4);

      //Draw and blend
      //glDrawArrays(GL_TRIANGLES, count, 2);
   }

   //restore states
   glcache.Enable(GL_DEPTH_TEST);
}

void DrawStrips(void)
{
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

#if 0
      glClearDepth(0.f);
#endif
      glcache.DepthMask(GL_TRUE);
      glcache.StencilMask(0xFF);
      glClear(GL_DEPTH_BUFFER_BIT );

      //Opaque
      DrawList<ListType_Opaque, false>(pvrrc.global_param_op, 
            previous_pass.op_count, current_pass.op_count - previous_pass.op_count);

      //Alpha tested
      DrawList<ListType_Punch_Through, false>(pvrrc.global_param_pt,
            previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count);

      // Modifier volumes
      DrawModVols(previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);

      if (current_pass.autosort)
         GenSorted(previous_pass.tr_count,
               current_pass.tr_count - previous_pass.tr_count);

      //Alpha blended
      if (settings.pvr.Emulation.AlphaSortMode == 0)
      {
         if (current_pass.autosort)
            DrawSorted(render_pass < pvrrc.render_passes.used() - 1);
         else
            DrawList<ListType_Translucent, false>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
      }
      else if (settings.pvr.Emulation.AlphaSortMode == 1)
      {
         if (current_pass.autosort)
            SortPParams(previous_pass.tr_count,
                  current_pass.tr_count - previous_pass.tr_count);
         DrawList<ListType_Translucent, true>(pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count );
      }

      previous_pass = current_pass;
   }

   vertex_buffer_unmap();
}
