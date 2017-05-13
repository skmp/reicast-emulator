#include "../../types.h"
#include "pvr.h"
#include "tr.h"

#define TACALL DYNACALL

#define saturate01(x)       (((s32&)x)<0?0:(s32&)x>0x3f800000?1:x)
#define float_to_satu8(val) f32_su8_tbl[((u32&)val)>>16]

#define vdrc vd_rc

#define vrf(addr) (*(f32*)&vram.data[pvr_map32((addr)) & VRAM_MASK])
#define vri(addr) (*(u32*)&vram.data[pvr_map32((addr)) & VRAM_MASK])

u32 tileclip_val=0;
u8 f32_su8_tbl[65536];

bool pal_needs_update=true;

u8* vq_codebook;
u32 palette_index;
u32 _pal_rev_256[4]={0};
u32 _pal_rev_16[64]={0};
u32 pal_rev_256[4]={0};
u32 pal_rev_16[64]={0};
u32 palette_ram[1024];

u32 decoded_colors[3][65536];
u32 detwiddle[2][8][1024];

extern rend_context vd_rc;

/*
	TA-VTX handling

	Parsing of the TA stream and generation of vertex data !
*/

static u8 float_to_satu8_math(float val)
{
	return u8(saturate01(val)*255);
}

//vdec state variables
static ModTriangle* lmr=0;
static PolyParam nullPP;

static PolyParam* CurrentPP=&nullPP;
static List<PolyParam>* CurrentPPlist;

//TA state vars	
DECL_ALIGN(4) static u8 FaceBaseColor[4];
DECL_ALIGN(4) static u8 FaceOffsColor[4];
DECL_ALIGN(4) static u32 SFaceBaseColor;
DECL_ALIGN(4) static u32 SFaceOffsColor;

//splitter function lookup
extern u32 ta_type_lut[256];

//hehe
//as it seems, bit 1,2 are type, bit 0 is mod volume :p

//misc ones
#define LISTTYPE_NONE -1

#define SZ32 1
#define SZ64 2

typedef Ta_Dma* DYNACALL TaListFP(Ta_Dma* data,Ta_Dma* data_end);

TaListFP* TaCmd;
	
u32 CurrentList;
TaListFP* VertexDataFP;
bool ListIsFinished[5];

static INLINE f32 f16(u16 v)
{
	u32 z=v<<16;
	return *(f32*)&z;
}
//input : address in the yyyyyxxxxx format
//output : address in the xyxyxyxy format
//U : x resolution , V : y resolution
//twiddle works on 64b words

//Color conversions
#define tr_parse_color(col, base_color) \
   col[2] = (u8)(base_color); \
   col[1] = (u8)(base_color >> 8); \
   col[0] = (u8)(base_color >> 16); \
   col[3] = (u8)(base_color >> 24)

#define tr_parse_offset_color(cv, offset_color) \
   cv->vtx_spc[2] = (u8)(offset_color); \
   cv->vtx_spc[1] = (u8)(offset_color >> 8); \
   cv->vtx_spc[0] = (u8)(offset_color >> 16); \
   cv->vtx_spc[3] = (u8)(offset_color >> 24)

#define tr_parse_color_rgba(col, r, g, b, a) \
   col[0] = float_to_satu8(r); \
   col[1] = float_to_satu8(g); \
   col[2] = float_to_satu8(b); \
   col[3] = float_to_satu8(a)

#define tr_parse_color_intensity(col, base_intensity, val) \
   col[0] = val[0] * base_intensity / 256;  \
   col[1] = val[1] * base_intensity / 256; \
   col[2] = val[2] * base_intensity / 256; \
   col[3] = val[3]

#define tr_parse_offset_color_intensity(col, base_intensity) \
   col[0] = FaceOffsColor[0] * base_intensity / 256;  \
   col[1] = FaceOffsColor[1] * base_intensity / 256;  \
   col[2] = FaceOffsColor[2] * base_intensity / 256;  \
   col[3] = FaceOffsColor[3]

/* Intensity handling */

/* Notes:
 * Alpha doesn't get intensity
 * Intensity is clamped before the mul, 
 * as well as on face color to work the same as the hardware. [Fixes red dog] */

#define append_sprite(indx) \
   tr_parse_color(cv[indx].col,SFaceBaseColor); \
   tr_parse_color(cv[indx].vtx_spc,SFaceOffsColor)

#define append_sprite_yz(indx,set,st2) \
   cv[indx].y=sv->y##set; \
   cv[indx].z=sv->z##st2; \
   update_fz(sv->z##st2);

#define sprite_uv(indx,u_name,v_name) \
   cv[indx].u = f16(sv->u_name);\
   cv[indx].v = f16(sv->v_name);

#define should_append_poly_param(pp) (CurrentPP->pcw.full!=pp->pcw.full || CurrentPP->tcw.full!=pp->tcw.full || CurrentPP->tsp.full!=pp->tsp.full || CurrentPP->isp.full!=pp->isp.full) 

//Splitter function (normally ta_dma_main , modified for split dma's)

template<u32 instance>
class FifoSplitter
{
public:

	static Ta_Dma* DYNACALL NullVertexData(Ta_Dma* data,Ta_Dma* data_end)
	{
		printf("TA: Invalid state, ignoring VTX data\n");
		return data+SZ32;
	}

	//part : 0 fill all data , 1 fill upper 32B , 2 fill lower 32B
	//Poly decoder , will be moved to pvr code
	template <u32 poly_type,u32 part>
	static __forceinline Ta_Dma* TACALL ta_handle_poly(Ta_Dma* data,Ta_Dma* data_end)
   {
      TA_VertexParam* vp=(TA_VertexParam*)data;
      u32 rv=0;

      if (part==2)
         TaCmd=ta_main;

      switch (poly_type)
      {
         //32b , always in one pass :)

         /* (Non-Textured, Packed Color) */
         case 0:
            {
               TA_Vertex0 *vtx = (TA_Vertex0*)&vp->vtx0;
               Vertex     *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

               tr_parse_color(cv->col, vtx->BaseCol);
            }
            rv=SZ32;
            break;

            /* (Non-Textured, Floating Color) */
         case 1:
            {
               TA_Vertex1 *vtx = (TA_Vertex1*)&vp->vtx1;
               Vertex     *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

               tr_parse_color_rgba(cv->col, vtx->BaseR,
                     vtx->BaseG, vtx->BaseB, vtx->BaseA);
            }
            rv=SZ32;
            break;

            /* (Non-Textured, Intensity) */
         case 2:
            {
               TA_Vertex2* vtx = (TA_Vertex2*)&vp->vtx2;
               Vertex   * cv   = vert_cvt_base_((TA_Vertex0*)vtx);
               u32    satint   = float_to_satu8(vtx->BaseInt);

               tr_parse_color_intensity(cv->col, satint,
                     FaceBaseColor);
            }
            rv=SZ32;
            break;

            /* (Textured, Packed Color) */
         case 3:
            {
               TA_Vertex3 *vtx = (TA_Vertex3*)&vp->vtx3;
               Vertex     *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

               tr_parse_color(cv->col, vtx->BaseCol);

               tr_parse_offset_color(cv, vtx->OffsCol);

               cv->u = (vtx->u);
               cv->v = (vtx->v);
            }
            rv=SZ32;
            break;

            /* (Textured, Packed Color, 16bit UV) */
         case 4:
            {
               TA_Vertex4 *vtx = (TA_Vertex4*)&vp->vtx4;
               Vertex     *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

               tr_parse_color(cv->col, vtx->BaseCol);
               tr_parse_offset_color(cv, vtx->OffsCol);

               cv->u = f16(vtx->u);
               cv->v = f16(vtx->v);
            }
            rv=SZ32;
            break;

            /* (Textured, Intensity) */
         case 7:
            {
               TA_Vertex7 *vtx = (TA_Vertex7*)&vp->vtx7;
               Vertex      *cv = vert_cvt_base_((TA_Vertex0*)vtx);
               u32     satint0 = float_to_satu8(vtx->BaseInt);
               u32     satint1 = float_to_satu8(vtx->OffsInt);

               tr_parse_color_intensity(cv->col, satint0,
                     FaceBaseColor);
               tr_parse_offset_color_intensity(cv->vtx_spc, 
                     satint1);

               cv->u = (vtx->u);
               cv->v = (vtx->v);
            }
            rv=SZ32;
            break;

            /* (Textured, Intensity, 16bit UV) */
         case 8:
            {
               TA_Vertex8 *vtx = (TA_Vertex8*)&vp->vtx8;
               Vertex     *cv  = vert_cvt_base_((TA_Vertex0*)vtx);
               u32     satint0 = float_to_satu8(vtx->BaseInt);
               u32     satint1 = float_to_satu8(vtx->OffsInt);

               tr_parse_color_intensity(cv->col, satint0,
                     FaceBaseColor);
               tr_parse_offset_color_intensity(cv->vtx_spc, 
                     satint1);

               cv->u = f16(vtx->u);
               cv->v = f16(vtx->v);
            }
            rv=SZ32;
            break;

            /* (Non-Textured, Packed Color, with Two Volumes) */
         case 9:
            {
               TA_Vertex9 *vtx = (TA_Vertex9*)&vp->vtx9;
               Vertex     *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

               tr_parse_color(cv->col, vtx->BaseCol0);
            }
            rv=SZ32;
            break;

            /* (Non-Textured, Intensity, with Two Volumes) */
         case 10:
            {
               TA_Vertex10 *vtx = (TA_Vertex10*)&vp->vtx10;
               Vertex      *cv  = vert_cvt_base_((TA_Vertex0*)vtx);
               u32       satint = float_to_satu8(vtx->BaseInt0);

               tr_parse_color_intensity(cv->col, satint,
                     FaceBaseColor);
            }
            rv=SZ32;
            break;

            /* 64b , may be on 2 pass */

            /* (Textured, Floating Color) */
         case 5 :
            /*process first half*/
            if (part!=2)
            {
               rv+=SZ32;
               {
                  TA_Vertex5A *vtx = (TA_Vertex5A*)&vp->vtx5A;
                  Vertex      *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

                  //Colors are on B
                  cv->u = (vtx->u);
                  cv->v = (vtx->v);
               }
            }
            {
               TA_Vertex5B *vtx = NULL;
               /*process second half*/
               if (part==0)
                  vtx = (TA_Vertex5B*)&vp->vtx5B;
               else if (part == 2)
                  vtx = (TA_Vertex5B*)data;

               if (vtx)
               {
                  Vertex* cv=vdrc.verts.LastPtr();

                  tr_parse_color_rgba(cv->col, vtx->BaseR,
                        vtx->BaseG, vtx->BaseB, vtx->BaseA);
                  tr_parse_color_rgba(cv->vtx_spc, vtx->OffsR,
                        vtx->OffsG, vtx->OffsB, vtx->OffsA);
                  rv+=SZ32;
               }
            }
            break;

            /* (Textured, Floating Color, 16bit UV) */
         case 6 :
            /*process first half*/
            if (part!=2)
            {
               rv+=SZ32;
               {
                  TA_Vertex6A *vtx = (TA_Vertex6A*)&vp->vtx6A;
                  Vertex      *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

                  //Colors are on B

                  cv->u = f16(vtx->u);
                  cv->v = f16(vtx->v);
               }
            }

            {
               TA_Vertex6B *vtx = NULL;
               /*process second half*/
               if (part==0)
                  vtx = (TA_Vertex6B*)&vp->vtx6B;
               else if (part == 2)
                  vtx = (TA_Vertex6B*)data;

               if (vtx)
               {
                  Vertex* cv=vdrc.verts.LastPtr();

                  tr_parse_color_rgba(cv->col, vtx->BaseR,
                        vtx->BaseG, vtx->BaseB, vtx->BaseA);
                  tr_parse_color_rgba(cv->vtx_spc, vtx->OffsR,
                        vtx->OffsG, vtx->OffsB, vtx->OffsA);
                  rv+=SZ32;
               }
            }
            break;

            /* (Textured, Packed Color, with Two Volumes) */
         case 11 :
            /*process first half*/
            if (part!=2)
            {
               rv+=SZ32;
               {
                  TA_Vertex11A *vtx = (TA_Vertex11A*)&vp->vtx11A;
                  Vertex* cv=vert_cvt_base_((TA_Vertex0*)vtx);

                  tr_parse_color(cv->col, vtx->BaseCol0);
                  tr_parse_offset_color(cv, vtx->OffsCol0);

                  cv->u = (vtx->u0);
                  cv->v = (vtx->v0);
               }
            }
            /*process second half*/
            if (part==0 || part == 2)
               rv+=SZ32;
            break;
            /* (Textured, Packed Color, 16bit UV, with Two Volumes) */
         case 12 :
            /*process first half*/
            if (part!=2)
            {
               TA_Vertex12A *vtx = (TA_Vertex12A*)&vp->vtx12A;
               Vertex       *cv  = vert_cvt_base_((TA_Vertex0*)vtx);

               tr_parse_color(cv->col, vtx->BaseCol0);
               tr_parse_offset_color(cv, vtx->OffsCol0);


               cv->u = f16(vtx->u0);
               cv->v = f16(vtx->v0);

               rv+=SZ32;
            }
            /*process second half*/
            if (part==0 || part == 2)
               rv+=SZ32;
            break;

            /* (Textured, Intensity, with Two Volumes) */
         case 13 :
            /*process first half*/
            if (part!=2)
            {
               TA_Vertex13A *vtx = (TA_Vertex13A*)&vp->vtx13A;
               Vertex* cv=vert_cvt_base_((TA_Vertex0*)vtx);
               u32 satint0=float_to_satu8(vtx->BaseInt0);
               u32 satint1=float_to_satu8(vtx->OffsInt0);

               tr_parse_color_intensity(cv->col, satint0,
                     FaceBaseColor);

               tr_parse_offset_color_intensity(cv->vtx_spc, 
                     satint1);

               cv->u = (vtx->u0);
               cv->v = (vtx->v0);
               rv+=SZ32;
            }
            /*process second half*/
            if (part==0 || part == 2)
               rv+=SZ32;
            break;

            /* (Textured, Intensity, 16bit UV, with Two Volumes) */
         case 14 :
            /*process first half*/
            if (part!=2)
            {
               TA_Vertex14A *vtx = (TA_Vertex14A*)&vp->vtx14A;
               Vertex       *cv  = vert_cvt_base_((TA_Vertex0*)vtx);
               u32       satint0 = float_to_satu8(vtx->BaseInt0);
               u32       satint1 = float_to_satu8(vtx->OffsInt0);

               tr_parse_color_intensity(cv->col, satint0,
                     FaceBaseColor);

               tr_parse_offset_color_intensity(cv->vtx_spc, 
                     satint1);

               cv->u = f16(vtx->u0);
               cv->v = f16(vtx->v0);
               rv+=SZ32;
            }
            /*process second half*/
            if (part==0 || part == 2)
               rv+=SZ32;
            break;
      }

      /* In the case of the Polygon type, the last struct
       * vertex Parameter for an object must have "End of
       * Strip" specified. If Vertex Parameters with the "End
       * of Strip" specification were not input, but parameters
       * other than Vertex Parameters were input, the polygon data
       * in question is ignored and an interrupt signal is output. */
      return data+rv;
   }

	/* Code Splitter/routers */
		
	static Ta_Dma* TACALL ta_modvolB_32(Ta_Dma* data,Ta_Dma* data_end)
	{
      if (CurrentList == TA_LIST_OPAQUE_MODVOL)
      {
         TA_ModVolB* mvv = (TA_ModVolB*)data;
         lmr->y2=mvv->y2;
         lmr->z2=mvv->z2;
      }
		TaCmd=ta_main;
		return data+SZ32;
	}
		
	static Ta_Dma* TACALL ta_mod_vol_data(Ta_Dma* data,Ta_Dma* data_end)
   {
      TA_VertexParam* vp=(TA_VertexParam*)data;

      if (CurrentList == TA_LIST_OPAQUE_MODVOL)
      {
         TA_ModVolA* mvv = (TA_ModVolA*)&vp->mvolA;
         lmr=vdrc.modtrig.Append();

         lmr->x0=mvv->x0;
         lmr->y0=mvv->y0;
         lmr->z0=mvv->z0;

         lmr->x1=mvv->x1;
         lmr->y1=mvv->y1;
         lmr->z1=mvv->z1;

         lmr->x2=mvv->x2;
      }

      if (data==data_end)
      {
         //32B more needed , 32B done :)
         TaCmd=ta_modvolB_32;
         return data+SZ32;
      }

      //all 64B done
      if (CurrentList == TA_LIST_OPAQUE_MODVOL)
      {
         TA_ModVolB* mvv = (TA_ModVolB*)&vp->mvolB;

         lmr->y2=mvv->y2;
         lmr->z2=mvv->z2;
      }
      return data+SZ64;
   }

	static Ta_Dma* TACALL ta_spriteB_data(Ta_Dma* data,Ta_Dma* data_end)
	{
		//32B more needed , 32B done :)
		TaCmd=ta_main;
			
		AppendSpriteVertexB((TA_Sprite1B*)data);

		return data+SZ32;
	}

	static Ta_Dma* TACALL ta_sprite_data(Ta_Dma* data,Ta_Dma* data_end)
   {
      TA_VertexParam* vp=(TA_VertexParam*)data;
      //verify(data->pcw.ParaType == TA_PARAM_VERTEX);

      AppendSpriteVertexA(&vp->spr1A);

      if (data==data_end)
      {
         //32B more needed , 32B done :)
         TaCmd=ta_spriteB_data;
         return data+SZ32;
      }

      AppendSpriteVertexB(&vp->spr1B);

      //all 64B done
      return data+SZ64;
   }

	template <u32 poly_type,u32 poly_size>
	static Ta_Dma* TACALL ta_poly_data(Ta_Dma* data,Ta_Dma* data_end)
	{
      bool has_full_data = false;

      /*If SZ64  && 32 bytes */
		if ((poly_size!=SZ32) && (data==data_end))
			goto fist_half;

		do
      {
         ta_handle_poly<poly_type,0>(data,0);
         if (data->pcw.EndOfStrip)
            goto strip_end;
         data+=poly_size;
         has_full_data = (poly_size==SZ32 ? (data<=data_end) : (data<data_end));
      } while(has_full_data);
			
      //If SZ64  && 32 bytes
		if ((poly_size!=SZ32) && (data==data_end))
         goto fist_half;
			
		return data;

strip_end:
		TaCmd=ta_main;
		if (data->pcw.EndOfStrip)
			EndPolyStrip();
		return data+poly_size;

fist_half:
      ta_handle_poly<poly_type,1>(data,0);
      if (data->pcw.EndOfStrip)
         EndPolyStrip();
      TaCmd=ta_handle_poly<poly_type,2>;

      return data+SZ32;
	}

	//Second part of poly data
	template <int t>
	static Ta_Dma* TACALL ta_poly_B_32(Ta_Dma* data,Ta_Dma* data_end)
	{
      f32 r, g, b, a;
		if (t==2)
      {
         TA_PolyParam2B* pp=(TA_PolyParam2B*)data;
         r = pp->FaceColorR;
         g = pp->FaceColorG;
         b = pp->FaceColorB;
         a = pp->FaceColorA;

         tr_parse_color_rgba(FaceOffsColor, pp->FaceOffsetR,
               pp->FaceOffsetG, pp->FaceOffsetB, pp->FaceOffsetA);
      }
      else
      {
         TA_PolyParam4B* pp=(TA_PolyParam4B*)data;
         r = pp->FaceColor0R;
         g = pp->FaceColor0G;
         b = pp->FaceColor0B;
         a = pp->FaceColor0A;
      }

      tr_parse_color_rgba(FaceBaseColor, r, g, b, a);
	
		TaCmd=ta_main;
		return data+SZ32;
	}

public:
	
	static Ta_Dma* TACALL ta_main(Ta_Dma* data,Ta_Dma* data_end)
   {
      do
      {
         if ((data->pcw.ParaType == TA_PARAM_POLY_OR_VOL ||
             (data->pcw.ParaType == TA_PARAM_SPRITE)))
         {
            /* Tile clip mode */
#if 0
            /* Group_En bit seems ignored, thanks p1pkin  */
            if (data->pcw.Group_En)
#endif
               tileclip_val=(tileclip_val&(~0xF0000000)) | (data->pcw.User_Clip << 28);


            if (CurrentList== LISTTYPE_NONE)
            {
#if 0
               printf("Starting list %d\n",new_list);
#endif

               switch (data->pcw.ListType)
               {
                  case TA_LIST_OPAQUE:
                     CurrentPPlist=&vdrc.global_param_op;
                     break;
                  case TA_LIST_PUNCH_THROUGH:
                     CurrentPPlist=&vdrc.global_param_pt;
                     break;
                  case TA_LIST_TRANSLUCENT:
                     CurrentPPlist=&vdrc.global_param_tr;
                     break;
               }

               CurrentList = data->pcw.ListType;
               CurrentPP   = &nullPP;
            }
         }

         switch (data->pcw.ParaType)
         {
            //Control parameter
            //32Bw3
            case TA_PARAM_END_OF_LIST:
               if (CurrentList == LISTTYPE_NONE)
               {
                  CurrentList=data->pcw.ListType;
                  //printf("End_Of_List : list error\n");
               }
               else
               {
                  /* end of list should be all 0's ...
                   * end a list only if it was really started */
                  CurrentPP=&nullPP;
                  CurrentPPlist=0;
                  if (CurrentList == TA_LIST_OPAQUE_MODVOL)
                  {
                     ISP_Modvol p;
                     p.id=vdrc.modtrig.used();
                     *vdrc.global_param_mvo.Append()=p;
                  }
               }

               //printf("End list %X\n",CurrentList);
               ListIsFinished[CurrentList] = true;
               CurrentList                 = LISTTYPE_NONE;
               VertexDataFP                = NullVertexData;
               data+=SZ32;
               break;
               //32B
            case TA_PARAM_USER_TILE_CLIP:
               {
                  u32 xmin = data->data_32[3]&63;
                  u32 ymin = data->data_32[4]&31;
                  u32 xmax = data->data_32[5]&63;
                  u32 ymax = data->data_32[6]&31;
                  u32 rv=tileclip_val & 0xF0000000;
                  rv|=xmin; //6 bits
                  rv|=xmax<<6; //6 bits
                  rv|=ymin<<12; //5 bits
                  rv|=ymax<<17; //5 bits
                  tileclip_val=rv;
               }
               data+=SZ32;
               break;
               //32B
            case TA_PARAM_OBJ_LIST_SET:
               die("ParamType_Object_List_Set");
               // *cough* ignore it :p
               data+=SZ32;
               break;

               //Global Parameter
               //ModVolue :32B
               //PolyType :32B/64B
            case TA_PARAM_POLY_OR_VOL:
               if (IsModVolList(CurrentList))
               {
                  if (CurrentList == TA_LIST_OPAQUE_MODVOL)
                  {
                     TA_ModVolParam *param = (TA_ModVolParam*)data;
                     ISP_Modvol* p=vdrc.global_param_mvo.Append();
                     p->full=param->isp.full;
                     p->VolumeLast=param->pcw.Volume;
                     p->id=vdrc.modtrig.used();
                  }
                  VertexDataFP=ta_mod_vol_data;
                  data+=SZ32;
               }
               else
               {
                  static TaListFP* ta_poly_data_lut[15] = 
                  {
                     ta_poly_data<0,SZ32>,
                     ta_poly_data<1,SZ32>,
                     ta_poly_data<2,SZ32>,
                     ta_poly_data<3,SZ32>,
                     ta_poly_data<4,SZ32>,
                     ta_poly_data<5,SZ64>,
                     ta_poly_data<6,SZ64>,
                     ta_poly_data<7,SZ32>,
                     ta_poly_data<8,SZ32>,
                     ta_poly_data<9,SZ32>,
                     ta_poly_data<10,SZ32>,
                     ta_poly_data<11,SZ64>,
                     ta_poly_data<12,SZ64>,
                     ta_poly_data<13,SZ64>,
                     ta_poly_data<14,SZ64>,
                  };
                  bool append_poly_param = false;
                  u32  append_data       = SZ32;
                  u32 uid                = ta_type_lut[data->pcw.obj_ctrl];
                  u32 psz                = uid>>30;
                  u32 pdid               = (u8)(uid);
                  u32 ppid               = (u8)(uid>>8);

                  VertexDataFP           = ta_poly_data_lut[pdid];

                  if (data != data_end || psz==1)
                  {
                     //32/64b , full

                     //poly , 32B/64B
                     switch (ppid)
                     {
                        case 0:
                           {
                              TA_PolyParam0* pp=(TA_PolyParam0*)data;
                              if (should_append_poly_param(pp))
                                 append_poly_param = true;
                           }
                           break;
                        case 1:
                           {
                              TA_PolyParam1* pp=(TA_PolyParam1*)data;

                              if (should_append_poly_param(pp))
                                 append_poly_param = true;

                              tr_parse_color_rgba(FaceBaseColor, pp->FaceColorR, pp->FaceColorG,
                                    pp->FaceColorB, pp->FaceColorA);
                           }
                           break;
                        case 2:
                           {
                              Ta_Dma* pp=(Ta_Dma*)data;
                              TA_PolyParam2A* ppa=(TA_PolyParam2A*)&pp[0];
                              TA_PolyParam2B* ppb=(TA_PolyParam2B*)&pp[1];

                              if (should_append_poly_param(ppa))
                                 append_poly_param = true;

                              tr_parse_color_rgba(FaceBaseColor, ppb->FaceColorR, ppb->FaceColorG,
                                    ppb->FaceColorB, ppb->FaceColorA);
                              tr_parse_color_rgba(FaceOffsColor, ppb->FaceOffsetR, ppb->FaceOffsetG,
                                    ppb->FaceOffsetB, ppb->FaceOffsetA);
                           }
                           break;
                        case 3:
                           {
                              TA_PolyParam3* pp=(TA_PolyParam3*)data;
                              if (should_append_poly_param(pp))
                                 append_poly_param = true;
                           }
                           break;
                        case 4:
                           {
                              Ta_Dma* pp=(Ta_Dma*)data;
                              TA_PolyParam4A* ppa=(TA_PolyParam4A*)&pp[0];
                              TA_PolyParam4B* ppb=(TA_PolyParam4B*)&pp[1];

                              if (should_append_poly_param(ppa))
                                 append_poly_param = true;

                              tr_parse_color_rgba(FaceBaseColor, ppb->FaceColor0R, ppb->FaceColor0G,
                                    ppb->FaceColor0B, ppb->FaceColor0A);
                           }
                           break;
                     }

                     append_data = psz;
                  }
                  else
                  {
                     //64b , , second part
                     static TaListFP* ta_poly_param_b_lut[5]=
                     {
                        (TaListFP*)0,
                        (TaListFP*)0,
                        ta_poly_B_32<2>,
                        (TaListFP*)0,
                        ta_poly_B_32<4>
                     };
                     bool append_poly_param = false;

                     //AppendPolyParam64A((TA_PolyParamA*)data);
                     //64b , first part
                     switch (ppid)
                     {
                        case 2:
                           {
                              TA_PolyParam2A* pp=(TA_PolyParam2A*)data;

                              if (should_append_poly_param(pp))
                                 append_poly_param = true;
                           }
                           break;
                        case 4:
                           {
                              TA_PolyParam4A* pp=(TA_PolyParam4A*)data;

                              if (should_append_poly_param(pp))
                                 append_poly_param = true;
                           }
                           break;
                        default:
                           break;
                     }

                     //Handle next 32B ;)
                     TaCmd=ta_poly_param_b_lut[ppid];

                     append_data = SZ32;
                  }

                  if (append_poly_param)
                  {
                     /* Polys  -- update code on sprites if that gets updated too -- */
                     TA_PolyParam0 *npp  = (TA_PolyParam0*)data;
                     PolyParam     *d_pp = CurrentPP;
                     if (CurrentPP->count!=0)
                     {
                        d_pp=CurrentPPlist->Append(); 
                        CurrentPP=d_pp;
                     }
                     d_pp->first=vdrc.idx.used(); 
                     d_pp->count=0; 

                     d_pp->isp=npp->isp; 
                     d_pp->tsp=npp->tsp; 
                     d_pp->tcw=npp->tcw;
                     d_pp->pcw=npp->pcw; 
                     d_pp->tileclip=tileclip_val;

                     d_pp->texid = -1;

                     if (d_pp->pcw.Texture)
                        d_pp->texid = renderer->GetTexture(d_pp->tsp,d_pp->tcw);
                  }

                  data += append_data;
               }
               break;
               //32B
               //Sets Sprite info , and switches to ta_sprite_data function
            case TA_PARAM_SPRITE:
               VertexDataFP=ta_sprite_data;
               //printf("Sprite \n");
               AppendSpriteParam((TA_SpriteParam*)data);
               data+=SZ32;
               break;

               //Variable size
            case TA_PARAM_VERTEX:
               //printf("VTX:0x%08X\n",VertexDataFP);
               //verify(VertexDataFP!=NullVertexData);
               data=VertexDataFP(data,data_end);
               break;

               //not handled
               //Assumed to be 32B
            case 3:
            case 6:
               die("Unhandled parameter");
               data+=SZ32;
               break;
         }
      }while(data<=data_end);

      return data;
   }

	//Fill in lookup table
	FifoSplitter()
	{
		for (int i=0;i<256;i++)
		{
			PCW pcw;
			pcw.obj_ctrl = i;
			u32 rv       = poly_data_type_id(pcw);
			u32 type     = poly_header_type_size(pcw);

			if (type& 0x80)
				rv|=(SZ64<<30);
			else
				rv|=(SZ32<<30);

			rv|=(type&0x7F)<<8;

			ta_type_lut[i]=rv;
		}
	}
	/*
	Volume,Col_Type,Texture,Offset,Gouraud,16bit_UV

	0   0   0   (0) x   invalid Polygon Type 0  Polygon Type 0
	0   0   1   x   x   0       Polygon Type 0  Polygon Type 3
	0   0   1   x   x   1       Polygon Type 0  Polygon Type 4

	0   1   0   (0) x   invalid Polygon Type 0  Polygon Type 1
	0   1   1   x   x   0       Polygon Type 0  Polygon Type 5
	0   1   1   x   x   1       Polygon Type 0  Polygon Type 6

	0   2   0   (0) x   invalid Polygon Type 1  Polygon Type 2
	0   2   1   0   x   0       Polygon Type 1  Polygon Type 7
	0   2   1   0   x   1       Polygon Type 1  Polygon Type 8
	0   2   1   1   x   0       Polygon Type 2  Polygon Type 7
	0   2   1   1   x   1       Polygon Type 2  Polygon Type 8

	0   3   0   (0) x   invalid Polygon Type 0  Polygon Type 2
	0   3   1   x   x   0       Polygon Type 0  Polygon Type 7
	0   3   1   x   x   1       Polygon Type 0  Polygon Type 8

	1   0   0   (0) x   invalid Polygon Type 3  Polygon Type 9
	1   0   1   x   x   0       Polygon Type 3  Polygon Type 11
	1   0   1   x   x   1       Polygon Type 3  Polygon Type 12

	1   2   0   (0) x   invalid Polygon Type 4  Polygon Type 10
	1   2   1   x   x   0       Polygon Type 4  Polygon Type 13
	1   2   1   x   x   1       Polygon Type 4  Polygon Type 14

	1   3   0   (0) x   invalid Polygon Type 3  Polygon Type 10
	1   3   1   x   x   0       Polygon Type 3  Polygon Type 13
	1   3   1   x   x   1       Polygon Type 3  Polygon Type 14

	Sprites :
	(0) (0) 0 (0) (0) invalid Sprite  Sprite Type 0
	(0) (0) 1  x   (0) (1)     Sprite  Sprite Type 1

	*/
	//helpers 0-14
	static u32 poly_data_type_id(PCW pcw)
	{
      if (pcw.Volume)
      {
         if (pcw.Texture)
         {
            if (pcw.Col_Type == 0)
               return pcw.UV_16bit ? 12 : 11;
            if (pcw.Col_Type == 2 || pcw.Col_Type == 3)
               return pcw.UV_16bit ? 14 : 13;
         }

         if (pcw.Col_Type == 0)
            return 9;
         if (pcw.Col_Type == 2 || pcw.Col_Type == 3)
            return 10;
      }

      if (pcw.Texture)
      {
         if (pcw.Col_Type == 0)
            return pcw.UV_16bit ? 4 : 3;
         if (pcw.Col_Type == 1)
            return pcw.UV_16bit ? 6 : 5;
         if (pcw.Col_Type == 2 || pcw.Col_Type == 3)
            return pcw.UV_16bit ? 8 : 7;
      }

      if (pcw.Col_Type == 1)
         return 1;
      if (pcw.Col_Type == 2 || pcw.Col_Type == 3)
         return 2;

      return 0;
	}
	//0-4 | 0x80
	static u32 poly_header_type_size(PCW pcw)
	{
		if (pcw.Volume)
		{
         if (pcw.Col_Type == 0 || pcw.Col_Type == 3)
               return 3 | 0;              //Polygon Type 3 -- SZ32
         if (pcw.Col_Type == 2)
               return 4 | 0x80;           //Polygon Type 4 -- SZ64
		}

      if (pcw.Col_Type == 2)
      {
         if (pcw.Texture && !pcw.Offset)
            return 1 | 0;               //Polygon Type 1 -- SZ32
         if (pcw.Texture && pcw.Offset)
            return 2 | 0x80;         //Polygon Type 2 -- SZ64
         if (!pcw.Texture)
            return 1;
      }

      return 0;
	}


	void vdec_init(void)
	{
		vd_rc.Clear();

		//allocate storage for BG poly
		vd_rc.global_param_op.Append();
		u16* idx=vd_rc.idx.Append(4);
		int vbase=vd_rc.verts.used();

		idx[0]=vbase+0;
		idx[1]=vbase+1;
		idx[2]=vbase+2;
		idx[3]=vbase+3;
		vd_rc.verts.Append(4);

		TaCmd             = ta_main;
		CurrentList       = LISTTYPE_NONE;
		ListIsFinished[0] = false;
      ListIsFinished[1] = false;
      ListIsFinished[2] = false;
      ListIsFinished[3] = false;
      ListIsFinished[4] = false;
	}
		
	//Poly Strip handling
	//We unite Strips together by duplicating the [last,first].On odd sized strips
	//a second [first] vert is needed to make sure Culling works fine :)
	__forceinline
		static void EndPolyStrip()
	{
		CurrentPP->count=vdrc.idx.used() - CurrentPP->first;

		int vbase=vdrc.verts.used();

		*vdrc.idx.Append()=vbase-1;
		*vdrc.idx.Append()=vbase;

		if (CurrentPP->count&1)
			*vdrc.idx.Append()=vbase;
#if STRIPS_AS_PPARAMS
		if (CurrentPPlist==&vdrc.global_param_tr)
		{
			PolyParam* d_pp =CurrentPPlist->Append(); 
			*d_pp=*CurrentPP;
			CurrentPP=d_pp;
			d_pp->first=vdrc.idx.used(); 
			d_pp->count=0; 
		}
#endif
	}
	
	static inline void update_fz(float z)
	{
		if ((s32&)vdrc.fZ_max<(s32&)z && (s32&)z<0x49800000)
			vdrc.fZ_max=z;
	}

   /* Poly Vertex handlers
    * Append vertex base */
	static Vertex* vert_cvt_base_(TA_Vertex0* vtx)
	{
		f32 invW=vtx->xyz[2];
		*vdrc.idx.Append()=vdrc.verts.used();
		Vertex* cv=vdrc.verts.Append();
		cv->x=vtx->xyz[0];
		cv->y=vtx->xyz[1];
		cv->z=invW;
		update_fz(invW);
		return cv;
	}

	//Sprites
	__forceinline
		static void AppendSpriteParam(TA_SpriteParam* spr)
	{
		//printf("Sprite\n");
		PolyParam* d_pp=CurrentPP;
		if (CurrentPP->count!=0)
		{
			d_pp=CurrentPPlist->Append(); 
			CurrentPP=d_pp;
		}

		d_pp->first=vdrc.idx.used(); 
		d_pp->count=0;
		d_pp->isp=spr->isp; 
		d_pp->tsp=spr->tsp; 
		d_pp->tcw=spr->tcw; 
		d_pp->pcw=spr->pcw; 
		d_pp->tileclip=tileclip_val;

		d_pp->texid = -1;
		
		if (d_pp->pcw.Texture) {
			d_pp->texid = renderer->GetTexture(d_pp->tsp,d_pp->tcw);
		}

		SFaceBaseColor=spr->BaseCol;
		SFaceOffsColor=spr->OffsCol;
	}


	//Sprite Vertex Handlers
	__forceinline
		static void AppendSpriteVertexA(TA_Sprite1A* sv)
	{
		u16* idx=vdrc.idx.Append(6);
		u32 vbase=vdrc.verts.used();

		idx[0]=vbase+0;
		idx[1]=vbase+1;
		idx[2]=vbase+2;
		idx[3]=vbase+3;
		idx[4]=vbase+3;
		idx[5]=vbase+4;

		CurrentPP->count=vdrc.idx.used()-CurrentPP->first-2;

		Vertex* cv = vdrc.verts.Append(4);

		//Fill static stuff
		append_sprite(0);
		append_sprite(1);
		append_sprite(2);
		append_sprite(3);

		cv[2].x=sv->x0;
		cv[2].y=sv->y0;
		cv[2].z=sv->z0;
		update_fz(sv->z0);

		cv[3].x=sv->x1;
		cv[3].y=sv->y1;
		cv[3].z=sv->z1;
		update_fz(sv->z1);

		cv[1].x=sv->x2;
	}

	static void CaclulateSpritePlane(Vertex* base)
	{
		const Vertex& A=base[2];
		const Vertex& B=base[3];
		const Vertex& C=base[1];
		Vertex& P=base[0];
		//Vector AB = B-A;
		//Vector AC = C-A;
		//Vector AP = P-A;
		float AC_x=C.x-A.x,AC_y=C.y-A.y,AC_z=C.z-A.z,
			AB_x=B.x-A.x,AB_y=B.y-A.y,AB_z=B.z-A.z,
			AP_x=P.x-A.x,AP_y=P.y-A.y;

		float P_y=P.y,P_x=P.x,P_z=P.z,A_x=A.x,A_y=A.y,A_z=A.z;

		float AB_v=B.v-A.v,AB_u=B.u-A.u,
			AC_v=C.v-A.v,AC_u=C.u-A.u;

		float /*P_v,P_u,*/A_v=A.v,A_u=A.u;

		float k3 = (AC_x * AB_y - AC_y * AB_x);

#if 0
		if (k3 == 0)
		{
			/*throw new Exception("WTF?!"); */
		}
#endif

		float k2 = (AP_x * AB_y - AP_y * AB_x) / k3;

		float k1 = 0;

		if (AB_x == 0)
			k1 = (P_y - A_y - k2 * AC_y) / AB_y;
		else
			k1 = (P_x - A_x - k2 * AC_x) / AB_x;

		P.z = A_z + k1 * AB_z + k2 * AC_z;
		P.u = A_u + k1 * AB_u + k2 * AC_u;
		P.v = A_v + k1 * AB_v + k2 * AC_v;
	}

	__forceinline
		static void AppendSpriteVertexB(TA_Sprite1B* sv)
	{
		Vertex* cv=vdrc.verts.LastPtr();
		cv-=3;

		cv[1].y=sv->y2;
		cv[1].z=sv->z2;
		update_fz(sv->z2);

		cv[0].x=sv->x3;
		cv[0].y=sv->y3;


		sprite_uv(2, u0,v0);
		sprite_uv(3, u1,v1);
		sprite_uv(1, u2,v2);
		//sprite_uv(0, u0,v2);//or sprite_uv(u2,v0); ?

		CaclulateSpritePlane(cv);

		update_fz(cv[0].z);

#if STRIPS_AS_PPARAMS
		if (CurrentPPlist==&vdrc.global_param_tr)
		{
			PolyParam* d_pp =CurrentPPlist->Append(); 
			*d_pp=*CurrentPP;
			CurrentPP=d_pp;
			d_pp->first=vdrc.idx.used(); 
			d_pp->count=0;
		}
#else
		if (CurrentPP->count)
		{
			Vertex* vert=vert_reappend;
			vert[-1].x=vert[0].x;
			vert[-1].y=vert[0].y;
			vert[-1].z=vert[0].z;
			CurrentPP->count+=2;
		}
#endif
	}
};


FifoSplitter<0> TAFifo0;

int ta_parse_cnt = 0;

/*
	Also: gotta stage textures here
*/
bool ta_parse_vdrc(TA_context* ctx)
{
	vd_ctx = ctx;
	vd_rc  = vd_ctx->rend;

	ta_parse_cnt++;

	if ((ta_parse_cnt %  ( settings.pvr.ta_skip + 1)) == 0)
	{
		TAFifo0.vdec_init();
		
		Ta_Dma* ta_data=(Ta_Dma*)vd_rc.proc_start;
		Ta_Dma* ta_data_end=((Ta_Dma*)vd_rc.proc_end)-1;

		do
		{
			ta_data =TaCmd(ta_data,ta_data_end);
		}while(ta_data<=ta_data_end);
	}

	vd_ctx->rend = vd_rc;
	vd_ctx = 0;
#if !defined(TARGET_NO_THREADS)
   slock_unlock(ctx->rend_inuse);
#endif

	return true;
}

//decode a vertex in the native pvr format
//used for bg poly
static void decode_pvr_vertex(u32 base,u32 ptr,Vertex* cv)
{
	//ISP
	//TSP
	//TCW
	ISP_TSP isp;
	TSP tsp;
	TCW tcw;

	isp.full=vri(base);
	tsp.full=vri(base+4);
	tcw.full=vri(base+8);

	//XYZ
	//UV
	//Base Col
	//Offset Col

	//XYZ are _allways_ there :)
	cv->x=vrf(ptr);
   ptr+=4;
	cv->y=vrf(ptr);
   ptr+=4;
	cv->z=vrf(ptr);
   ptr+=4;

	if (isp.Texture)
	{	//Do texture , if any
		if (isp.UV_16b)
		{
			u32 uv=vri(ptr);
			cv->u = f16((u16)uv);
			cv->v = f16((u16)(uv >> 16));
			ptr+=4;
		}
		else
		{
			cv->u=vrf(ptr);
         ptr+=4;
			cv->v=vrf(ptr);
         ptr+=4;
		}
	}

	//Color
	u32 col=vri(ptr);ptr+=4;

   tr_parse_color(cv->col, col);

	if (isp.Offset)
	{
		//Intensity color (can be missing too ;p)
		u32 col=vri(ptr);
      ptr+=4;

      tr_parse_offset_color(cv, col);
	}
}

void vtxdec_init(void)
{
	/*
		0x3b80 ~ 0x3f80 -> actual useful range. Rest is clamping to 0 or 255 ~
	*/

	for (u32 i=0;i<65536;i++)
	{
		u32 fr=i<<16;
		
		f32_su8_tbl[i]=float_to_satu8_math((f32&)fr);
	}

	for (u32 i=0;i<65536;i++)
	{
		u32 fr=i<<16;
		f32 ff=(f32&)fr;
	}
}


static OnLoad ol_vtxdec(&vtxdec_init);

void FillBGP(TA_context* ctx)
{
	
	//Render pre-code
	//--BG poly
	u32 param_base=PARAM_BASE & 0xF00000;


	PolyParam* bgpp=ctx->rend.global_param_op.head();
	Vertex* cv=ctx->rend.verts.head();

	//Get the strip base
	u32 strip_base=(param_base + ISP_BACKGND_T.tag_address*4);	//this is *not* VRAM_MASK on purpose.It fixes naomi bios and quite a few naomi games
	//i have *no* idea why that happens, they manage to set the render target over there as well
	//and that area is *not* written by the games (they instead write the params on 000000 instead of 800000)
	//could be a h/w bug ? param_base is 400000 and tag is 100000*4
	//Calculate the vertex size
	//Update: Looks like I was handling the bank interleave wrong for 16 megs ram, could that be it?

	u32 strip_vs=3 + ISP_BACKGND_T.skip;
	u32 strip_vert_num=ISP_BACKGND_T.tag_offset;

   /* double parameters for volumes */
	if (FPU_SHAD_SCALE.intensity_shadow!=0 && ISP_BACKGND_T.shadow)
	{
		strip_vs+=ISP_BACKGND_T.skip;//2x the size needed :p
	}
	strip_vs*=4;
	//Get vertex ptr
	u32 vertex_ptr=strip_vert_num*strip_vs+strip_base +3*4;
	//now , all the info is ready :p

	bgpp->texid = -1;

	bgpp->isp.full=vri(strip_base);
	bgpp->tsp.full=vri(strip_base+4);
	bgpp->tcw.full=vri(strip_base+8);
	bgpp->count=4;
	bgpp->first=0;
	bgpp->tileclip=0;//disabled ! HA ~

	bgpp->isp.DepthMode=7;// -> this makes things AWFULLY slow .. sometimes
	bgpp->isp.CullMode=0;// -> so that its not culled, or somehow else hidden !
	bgpp->tsp.FogCtrl=2;
	//Set some pcw bits .. I should really get rid of pcw ..
	bgpp->pcw.UV_16bit=bgpp->isp.UV_16b;
	bgpp->pcw.Gouraud=bgpp->isp.Gouraud;
	bgpp->pcw.Offset=bgpp->isp.Offset;
	bgpp->pcw.Texture=bgpp->isp.Texture;

	float scale_x= (SCALER_CTL.hscale) ? 2.f:1.f;	//if AA hack the hacked pos value hacks
	for (int i=0;i<3;i++)
	{
		decode_pvr_vertex(strip_base,vertex_ptr,&cv[i]);
		vertex_ptr+=strip_vs;
	}

	float ZV=0;

	cv[0].x=-2000;
	cv[0].y=-2000;
	cv[0].z=ISP_BACKGND_D.f;

	cv[1].x=640*scale_x + 2000;
	cv[1].y=0;
	cv[1].z=ISP_BACKGND_D.f;

	cv[2].x=-2000;
	cv[2].y=480+2000;
	cv[2].z=ISP_BACKGND_D.f;

	cv[3]=cv[2];
	cv[3].x=640*scale_x+2000;
	cv[3].y=480+2000;
	cv[3].z=ISP_BACKGND_D.f;
}

bool UsingAutoSort(void)
{
	if (((FPU_PARAM_CFG >> 21) & 1) == 0)
		return ((ISP_FEED_CFG & 1) == 0);
   return ((vri(REGION_BASE) >> 29) & 1) == 0;
}

static u32 twiddle_slow(u32 x,u32 y,u32 x_sz,u32 y_sz)
{
	u32 sh=0;
	u32 rv=0;//low 2 bits are directly passed  -> needs some misc stuff to work.However
			 //Pvr internally maps the 64b banks "as if" they were twiddled :p

	x_sz>>=1;
	y_sz>>=1;
	while(x_sz!=0 || y_sz!=0)
	{
		if (y_sz)
		{
			u32 temp=y&1;
			rv|=temp<<sh;

			y_sz>>=1;
			y>>=1;
			sh++;
		}
		if (x_sz)
		{
			u32 temp=x&1;
			rv|=temp<<sh;

			x_sz>>=1;
			x>>=1;
			sh++;
		}
	}	
	return rv;
}

static void BuildTwiddleTables(void)
{
	for (u32 s=0;s<8;s++)
	{
		u32 x_sz=1024;
		u32 y_sz=8<<s;
		for (u32 i=0;i<x_sz;i++)
		{
			detwiddle[0][s][i]=twiddle_slow(i,0,x_sz,y_sz);
			detwiddle[1][s][i]=twiddle_slow(0,i,y_sz,x_sz);
		}
	}
}

static OnLoad btt(&BuildTwiddleTables);

void palette_update(void)
{
   if (pal_needs_update==false)
      return;
   memcpy(pal_rev_256,_pal_rev_256,sizeof(pal_rev_256));
   memcpy(pal_rev_16,_pal_rev_16,sizeof(pal_rev_16));

   pal_needs_update=false;
   switch(PAL_RAM_CTRL&3)
   {
      case TA_PAL_ARGB1555:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB1555(PALETTE_RAM[i]);
         break;

      case TA_PAL_RGB565:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB565(PALETTE_RAM[i]);
         break;

      case TA_PAL_ARGB4444:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB4444(PALETTE_RAM[i]);
         break;

      case TA_PAL_ARGB8888:
         for (int i=0;i<1024;i++)
            palette_ram[i]=ARGB8888(PALETTE_RAM[i]);
         break;
   }

}
