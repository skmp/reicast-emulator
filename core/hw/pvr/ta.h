#pragma once
#include "pvr.h"
#include "ta.h"

#include "helper_classes.h"

#include "hw/holly/holly.h"
#include "hw/sh4/sh4_if.h"

#ifndef TARGET_NO_THREADS
#include <rthreads/rthreads.h>
#endif

//bits that affect drawing (for caching params)
#define PCW_DRAW_MASK (0x000000CC)

#define IsModVolList(list) (((list)&1)!=0)

enum
{
   TA_LIST_OPAQUE                  = 0,
   TA_LIST_OPAQUE_MODVOL,
   TA_LIST_TRANSLUCENT,
   TA_LIST_TRANSLUCENT_MODVOL,
   TA_LIST_PUNCH_THROUGH,
   TA_NUM_LISTS
};

enum
{
   /* Control Parameter */
   TA_PARAM_END_OF_LIST                     = 0,
   TA_PARAM_USER_TILE_CLIP,
   TA_PARAM_OBJ_LIST_SET,
   /* Reserved */
   TA_PARAM_RESERVED0,
   /* Global Parameter */
   TA_PARAM_POLY_OR_VOL,
   TA_PARAM_SPRITE,
   /* Reserved */
   TA_PARAM_RESERVED1,
   /* Vertex , Sprite or ModVolume Parameter */
   TA_PARAM_VERTEX,
   TA_NUM_PARAMS
};

enum
{
   TA_PIXEL_1555 = 0,
   TA_PIXEL_565,
   TA_PIXEL_4444,
   TA_PIXEL_YUV422,
   TA_PIXEL_BUMPMAP,
   TA_PIXEL_4BPP,
   TA_PIXEL_8BPP,
   TA_PIXEL_RESERVED
};

enum
{
   TA_PAL_ARGB1555 = 0,
   TA_PAL_RGB565,
   TA_PAL_ARGB4444,
   TA_PAL_ARGB8888
};

#pragma pack(push, 1)   // n = 1
//	Global Param/misc structs
//4B
union PCW
{
	struct
	{
#ifdef MSB_FIRST
		// Para Control
		u32 ParaType    : 3;
		u32 EndOfStrip  : 1;
		u32 Res_1       : 1;
		u32 ListType    : 3;

		// Group Control
		u32 Group_En    : 1;
		u32 Res_2       : 3;
		u32 Strip_Len   : 2;
		u32 User_Clip   : 2;

		u32 Reserved    : 8; //0000 0000

		//Obj Control        //affects drawing ?
		u32 Shadow      : 1; //1
		u32 Volume      : 1; //1
		u32 Col_Type    : 2; //00
		u32 Texture     : 1; //1
		u32 Offset      : 1; //1
		u32 Gouraud     : 1; //0
		u32 UV_16bit    : 1; //0
#else
		//Obj Control        //affects drawing ?
		u32 UV_16bit    : 1; //0
		u32 Gouraud     : 1; //0
		u32 Offset      : 1; //1
		u32 Texture     : 1; //1
		u32 Col_Type    : 2; //00
		u32 Volume      : 1; //1
		u32 Shadow      : 1; //1

		u32 Reserved    : 8; //0000 0000

		// Group Control
		u32 User_Clip   : 2;
		u32 Strip_Len   : 2;
		u32 Res_2       : 3;
		u32 Group_En    : 1;

		// Para Control
		u32 ListType    : 3;
		u32 Res_1       : 1;
		u32 EndOfStrip  : 1;
		u32 ParaType    : 3;
#endif
	};
	u8 obj_ctrl;
	struct
	{
		u32 padin  : 8;
		u32 S6X    : 1;    //set by TA preprocessing if sz64
		u32 padin2 : 19;
		u32 PTEOS  : 4;
	};
	u32 full;
};


//// ISP/TSP Instruction Word

union ISP_TSP
{
	struct
	{
#ifdef MSB_FIRST
		u32 DepthMode   : 3;
		u32 CullMode    : 2;
		u32 ZWriteDis   : 1;
		u32 Texture     : 1; // -- up to here --
		u32 Offset      : 1; //
		u32 Gouraud     : 1; //by the ones on PCW
		u32 UV_16b      : 1; //In TA they are replaced
		u32 CacheBypass : 1;
		u32 DCalcCtrl   : 1;
		u32 Reserved    : 20;
#else
		u32 Reserved    : 20;
		u32 DCalcCtrl   : 1;
		u32 CacheBypass : 1;
		u32 UV_16b      : 1; //In TA they are replaced
		u32 Gouraud     : 1; //by the ones on PCW
		u32 Offset      : 1; //
		u32 Texture     : 1; // -- up to here --
		u32 ZWriteDis   : 1;
		u32 CullMode    : 2;
		u32 DepthMode   : 3;
#endif
	};
	u32 full;
};

union ISP_Modvol
{
	struct
	{
#ifdef MSB_FIRST
		u32 DepthMode  : 3;
		u32 CullMode   : 2;
		u32 VolumeLast : 1;
		u32 id         : 26;
#else
		u32 id         : 26;
		u32 VolumeLast : 1;
		u32 CullMode   : 2;
		u32 DepthMode  : 3;
#endif
	};
	u32 full;
};


//// END ISP/TSP Instruction Word


//// TSP Instruction Word

union TSP
{
	struct 
	{
#ifdef MSB_FIRST
		u32 SrcInstr    : 3;
		u32 DstInstr    : 3;
		u32 SrcSelect   : 1; // Primary Accum
		u32 DstSelect   : 1; // Secondary Accum
		u32 FogCtrl     : 2;
		u32 ColorClamp  : 1;
		u32 UseAlpha    : 1;
		u32 IgnoreTexA  : 1;
		u32 FlipU       : 1;
		u32 FlipV       : 1;
		u32 ClampU      : 1;
		u32 ClampV      : 1;
		u32 FilterMode  : 2;
		u32 SupSample   : 1;
		u32 MipMapD     : 4;
		u32 ShadInstr   : 2;
		u32 TexU        : 3;
		u32 TexV        : 3;
#else
		u32 TexV        : 3;
		u32 TexU        : 3;
		u32 ShadInstr   : 2;
		u32 MipMapD     : 4;
		u32 SupSample   : 1;
		u32 FilterMode  : 2;
		u32 ClampV      : 1;
		u32 ClampU      : 1;
		u32 FlipV       : 1;
		u32 FlipU       : 1;
		u32 IgnoreTexA  : 1;
		u32 UseAlpha    : 1;
		u32 ColorClamp  : 1;
		u32 FogCtrl     : 2;
		u32 DstSelect   : 1; // Secondary Accum
		u32 SrcSelect   : 1; // Primary Accum
		u32 DstInstr    : 3;
		u32 SrcInstr    : 3;
#endif
	};
	u32 full;
} ;


//// END TSP Instruction Word


/// Texture Control Word
union TCW
{
	struct
	{
#ifdef MSB_FIRST
		u32 MipMapped : 1;
		u32 VQ_Comp   : 1;
		u32 PixelFmt  : 3;
		u32 ScanOrder : 1;
		u32 StrideSel : 1;
		u32 Reserved  : 4;
		u32 TexAddr   :21;
#else
		u32 TexAddr   :21;
		u32 Reserved  : 4;
		u32 StrideSel : 1;
		u32 ScanOrder : 1;
		u32 PixelFmt  : 3;
		u32 VQ_Comp   : 1;
		u32 MipMapped : 1;
#endif
	} ;
	struct
	{
#ifdef MSB_FIRST
		u32 PalSelect : 6;
		u32 pading_0  :21;
#else
		u32 pading_0  :21;
		u32 PalSelect : 6;
#endif
	} ;
	u32 full;
};

/// END Texture Control Word

//32B
struct Ta_Dma
{
	//0
	//Parameter Control Word
	PCW pcw;
	//4
	union
	{
		u8  data_8[32-4];
		u32 data_32[8-1];
	};
};

//Poly Param types :/

/*
Polygon Type 0(Packed/Floating Color)
0x00    Parameter Control Word
0x04    ISP/TSP Instruction Word
0x08    TSP Instruction Word
0x0C    Texture Control Word
0x10    (ignored)
0x14    (ignored)
0x18    Data Size for Sort DMA
0x1C    Next Address for Sort DMA
*/
//32B
struct TA_PolyParam0
{
	PCW pcw;
	ISP_TSP isp;

	TSP tsp;
	TCW tcw;

	u32 ign1;
	u32 ign2;

	//for sort dma
	u32 SDMA_SIZE;
	u32 SDMA_ADDR;
};

/*
Polygon Type 1(Intensity, no Offset Color)
0x00    Parameter Control Word
0x04    ISP/TSP Instruction Word
0x08    TSP Instruction Word
0x0C    Texture Control Word
0x10    Face Color Alpha
0x14    Face Color R
0x18    Face Color G
0x1C    Face Color B
*/
//32B
struct TA_PolyParam1
{
	PCW pcw;
	ISP_TSP isp;

	TSP tsp;
	TCW tcw;

	f32 FaceColorA;
	f32 FaceColorR;
	f32 FaceColorG;
	f32 FaceColorB;
};


/*
Polygon Type 2(Intensity, use Offset Color)
0x00    Parameter Control Word
0x04    ISP/TSP Instruction Word
0x08    TSP Instruction Word
0x0C    Texture Control Word
0x10    (ignored)
0x14    (ignored)
0x18    Data Size for Sort DMA
0x1C    Next Address for Sort DMA
0x20    Face Color Alpha
0x24    Face Color R
0x28    Face Color G
0x2C    Face Color B
0x30    Face Offset Color Alpha
0x34    Face Offset Color R
0x38    Face Offset Color G
0x3C    Face Offset Color B
*/
//32B
struct TA_PolyParam2A
{
	PCW pcw;
	ISP_TSP isp;

	TSP tsp;
	TCW tcw;

	u32 ign1;
	u32 ign2;

	//for sort dma
	u32 SDMA_SIZE;
	u32 SDMA_ADDR;
};
//32B
struct TA_PolyParam2B
{
	//Face color
	f32 FaceColorA, FaceColorR,FaceColorG, FaceColorB;
	//Offset color :)
	f32 FaceOffsetA, FaceOffsetR, FaceOffsetG, FaceOffsetB;
};
/*
Polygon Type 3(Packed Color, with Two Volumes)
0x00    Parameter Control Word
0x04    ISP/TSP Instruction Word
0x08    TSP Instruction Word 0
0x0C    Texture Control Word 0
0x10    TSP Instruction Word 1
0x14    Texture Control Word 1
0x18    Data Size for Sort DMA
0x1C    Next Address for Sort DMA
*/
//32B
struct TA_PolyParam3
{
	PCW pcw;
	ISP_TSP isp;

	//for 1st volume
	TSP tsp;
	TCW tcw;

	//for 2nd volume format
	TSP tsp1;
	TCW tcw1;

	//for sort dma
	u32 SDMA_SIZE;
	u32 SDMA_ADDR;
};

/*
Polygon Type 4(Intensity, with Two Volumes)
0x00    Parameter Control Word
0x04    ISP/TSP Instruction Word
0x08    TSP Instruction Word 0
0x0C    Texture Control Word 0
0x10    TSP Instruction Word 1
0x14    Texture Control Word 1
0x18    Data Size for Sort DMA
0x1C    Next Address for Sort DMA
0x20    Face Color Alpha 0
0x24    Face Color R 0
0x28    Face Color G 0
0x2C    Face Color B 0
0x30    Face Color Alpha 1
0x34    Face Color R 1
0x38    Face Color G 1
0x3C    Face Color B 1
*/

//32B
struct TA_PolyParam4A
{
	PCW pcw;
	ISP_TSP isp;

	//for 1st volume
	TSP tsp;
	TCW tcw;

	//for 2nd volume format
	TSP tsp1;
	TCW tcw1;

	//for sort dma
	u32 SDMA_SIZE;
	u32 SDMA_ADDR;
};
//32B
struct TA_PolyParam4B
{
	//Face color 0
	f32 FaceColor0A, FaceColor0R,FaceColor0G, FaceColor0B;
	//Face color 1
	f32 FaceColor1A, FaceColor1R, FaceColor1G, FaceColor1B;
};

///Mod vol param types
struct TA_ModVolParam
{
	PCW pcw;
	ISP_TSP isp;

	u32  ign[8-2];
};

//Sprite
struct TA_SpriteParam
{
	PCW pcw;
	ISP_TSP isp;

	TSP tsp;
	TCW tcw;

	u32 BaseCol;
	u32 OffsCol;

	//for sort DMA
	u32 SDMA_SIZE;
	u32 SDMA_ADDR;
};

//	Vertex Param Structs
//28B
struct TA_Vertex0   // (Non-Textured, Packed Color)
{
	f32 xyz[3];
	u32 ignore_1;
	u32 ignore_2;
	u32 BaseCol;
	u32 ignore_3;
};
//28B
struct TA_Vertex1   // (Non-Textured, Floating Color)
{
	f32 xyz[3];
	f32 BaseA, BaseR,
		BaseG, BaseB;
};
//28B
struct TA_Vertex2   // (Non-Textured, Intensity)
{
	f32 xyz[3];
	u32 ignore_1;
	u32 ignore_2;
	f32 BaseInt;
	u32 ignore_3;
};
//28B
struct TA_Vertex3   // (Packed Color)
{
	f32 xyz[3];
	f32 u,v;
	u32 BaseCol;
	u32 OffsCol;
};
//28B
struct TA_Vertex4   // (Packed Color, 16bit UV)
{
	f32 xyz[3];
	u16 v,u; //note the opposite order here !
	u32 ignore_1;
	u32 BaseCol;
	u32 OffsCol;
};
//28B
struct TA_Vertex5A  // (Floating Color)
{
	f32 xyz[3];
	f32 u,v;
	u32 ignore_1;
	u32 ignore_2;
};
//32B
struct TA_Vertex5B
{
	f32 BaseA, BaseR,
		BaseG, BaseB;
	f32 OffsA, OffsR,
		OffsG, OffsB;
};
//28B
struct TA_Vertex6A  // (Floating Color, 16bit UV)
{
	f32 xyz[3];
	u16 v,u; //note the opposite order here !
	u32 ignore_1;
	u32 ignore_2;
	u32 ignore_3;
};
//32B
struct TA_Vertex6B
{
	f32 BaseA, BaseR,
		BaseG, BaseB;
	f32 OffsA, OffsR,
		OffsG, OffsB;
};
//28B
struct TA_Vertex7   // (Intensity)
{
	f32 xyz[3];
	f32 u,v;
	f32 BaseInt;
	f32 OffsInt;

};
//28B
struct TA_Vertex8   // (Intensity, 16bit UV)
{
	f32 xyz[3];
	u16 v,u; //note the opposite order here !
	u32 ignore_1;
	f32 BaseInt;
	f32 OffsInt;
};
//28B
struct TA_Vertex9   // (Non-Textured, Packed Color, with Two Volumes)
{
	f32 xyz[3];
	u32 BaseCol0;
	u32 BaseCol1;
	u32 ignore_1;
	u32 ignore_2;
};
//28B
struct TA_Vertex10   // (Non-Textured, Intensity, with Two Volumes)
{
	f32 xyz[3];
	f32 BaseInt0;
	f32 BaseInt1;
	u32 ignore_1;
	u32 ignore_2;
};

//28B
struct TA_Vertex11A  // (Textured, Packed Color, with Two Volumes)
{
	f32 xyz[3];
	f32 u0,v0;
	u32 BaseCol0, OffsCol0;
};
//32B
struct TA_Vertex11B
{
	f32 u1,v1;
	u32 BaseCol1, OffsCol1;
	u32 ignore_1, ignore_2;
	u32 ignore_3, ignore_4;
};

//28B
struct TA_Vertex12A   // (Textured, Packed Color, 16bit UV, with Two Volumes)
{
	f32 xyz[3];
	u16 v0,u0; //note the opposite order here !
	u32 ignore_1;
	u32 BaseCol0, OffsCol0;
};
//32B
struct TA_Vertex12B
{
	u16 v1,u1; //note the opposite order here !
	u32 ignore_2;
	u32 BaseCol1, OffsCol1;
	u32 ignore_3, ignore_4;
	u32 ignore_5, ignore_6;
};
//28B
struct TA_Vertex13A  // (Textured, Intensity, with Two Volumes)
{
	f32 xyz[3];
	f32 u0,v0;
	f32 BaseInt0, OffsInt0;
};
//32B
struct TA_Vertex13B
{
	f32 u1,v1;
	f32 BaseInt1, OffsInt1;
	u32 ignore_1, ignore_2;
	u32 ignore_3, ignore_4;
};

//28B
struct TA_Vertex14A  // (Textured, Intensity, 16bit UV, with Two Volumes)
{
	f32 xyz[3];
	u16 v0,u0; //note the opposite order here !
	u32 ignore_1;
	f32 BaseInt0, OffsInt0;
};
//32B
struct TA_Vertex14B
{
	u16 v1,u1; //note the opposite order here !
	u32 ignore_2;
	f32 BaseInt1, OffsInt1;
	u32 ignore_3, ignore_4;
	u32 ignore_5, ignore_6;
};

//28B
struct TA_Sprite0A  // Line ?
{
	f32 x0,y0,z0;
	f32 x1,y1,z1;
	f32 x2;

};
//32B
struct TA_Sprite0B  // Line ?
{
	f32 y2,z2;
	f32 x3,y3;
	u32 ignore_1, ignore_2;
	u32 ignore_3, ignore_4;
};
//28B
struct TA_Sprite1A
{
	f32 x0,y0,z0;
	f32 x1,y1,z1;
	f32 x2;
};
//32B
struct TA_Sprite1B
{
	f32 y2,z2;
	f32 x3,y3;
	u32 ignore_1;

	u16 v0; u16 u0;
	u16 v1; u16 u1;
	u16 v2; u16 u2;
};

//28B
struct TA_ModVolA
{
	PCW pcw;
	f32 x0,y0,z0;
	f32 x1,y1,z1;
	f32 x2;  //3+3+1=7*4=28
};
//32B
struct TA_ModVolB
{
	f32 y2,z2;     //2
	u32 ignore[6]; //8
};

//all together , and pcw ;)
struct TA_VertexParam
{
	union
	{
		struct
		{
			PCW pcw;

			union
			{
				u8 Raw[64-4];

				TA_Vertex0 vtx0;
				TA_Vertex1 vtx1;
				TA_Vertex2 vtx2;
				TA_Vertex3 vtx3;
				TA_Vertex4 vtx4;

				struct
				{
					TA_Vertex5A vtx5A;
					TA_Vertex5B vtx5B;
				};

				struct
				{
					TA_Vertex6A vtx6A;
					TA_Vertex6B vtx6B;
				};

				TA_Vertex7  vtx7;
				TA_Vertex8  vtx8;
				TA_Vertex9  vtx9;
				TA_Vertex10 vtx10;



				struct
				{
					TA_Vertex11A vtx11A;
					TA_Vertex11B vtx11B;
				};


				struct
				{
					TA_Vertex12A vtx12A;
					TA_Vertex12B vtx12B;
				};

				struct
				{
					TA_Vertex13A vtx13A;
					TA_Vertex13B vtx13B;
				};

				struct
				{
					TA_Vertex14A vtx14A;
					TA_Vertex14B vtx14B;
				};

				struct
				{
					TA_Sprite0A spr0A;
					TA_Sprite0B spr0B;
				};

				struct
				{
					TA_Sprite1A spr1A;
					TA_Sprite1B spr1B;
				};
			};

		};
		struct
		{
			TA_ModVolA mvolA;
			TA_ModVolB mvolB;
		};
	};
};

#pragma pack(pop)


struct TA_context;

void ta_init();
void ta_reset();
void ta_term();


void ta_vtx_ListCont();
void ta_vtx_ListInit();
void ta_vtx_SoftReset();

void DYNACALL ta_vtx_data32(void* data);
void ta_vtx_data(u32* data, u32 size);

bool ta_parse_vdrc(TA_context* ctx);

#define STRIPS_AS_PPARAMS 1


//Vertex storage types
struct Vertex
{
	float x,y,z;

	u8 col[4];
	u8 vtx_spc[4];

	float u,v;
};

struct PolyParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;

	u32 texid;

	TSP tsp;
	TCW tcw;
	PCW pcw;
	ISP_TSP isp;
	float zvZ;
	u32 tileclip;
	//float zMin,zMax;
};

struct ModParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;
};

struct ModTriangle
{
	f32 x0,y0,z0,x1,y1,z1,x2,y2,z2;
};

struct  tad_context
{
	u8* thd_data;
	u8* thd_root;
	u8* thd_old_data;

   void Clear()
   {
      thd_old_data = thd_data = thd_root;
   }

   void ClearPartial()
	{
		thd_old_data = thd_data;
		thd_data = thd_root;
	}

   u8* End()
   {
      return thd_data == thd_root ? thd_old_data : thd_data;
   }

   void Reset(u8* ptr)
	{
		thd_data = thd_root = thd_old_data = ptr;
	}
};

struct rend_context
{
	u8* proc_start;
	u8* proc_end;

	f32 fZ_min;
	f32 fZ_max;

	bool Overrun;
	bool isRTT;
	bool isAutoSort;

	double early;

	FB_X_CLIP_type    fb_X_CLIP;
	FB_Y_CLIP_type    fb_Y_CLIP;

	List<Vertex>      verts;
	List<u16>         idx;
	List<ModTriangle> modtrig;
	List<ISP_Modvol>  global_param_mvo;

	List<PolyParam>   global_param_op;
	List<PolyParam>   global_param_pt;
	List<PolyParam>   global_param_tr;

	void Clear()
	{
		verts.Clear();
		idx.Clear();
		global_param_op.Clear();
		global_param_pt.Clear();
		global_param_tr.Clear();
		modtrig.Clear();
		global_param_mvo.Clear();

		Overrun=false;
		fZ_min= 1000000.0f;
		fZ_max= 1.0f;
	}
};

//vertex lists
struct TA_context
{
	u32 Address;
	u32 LastUsed;

#if !defined(TARGET_NO_THREADS)
	slock_t *thd_inuse;
	slock_t *rend_inuse;
#endif

	tad_context tad;
	rend_context rend;

	
	/*
		Dreamcast games use up to 20k vtx, 30k idx, 1k (in total) parameters.
		at 30 fps, thats 600kvtx (900 stripped)
		at 20 fps thats 1.2M vtx (~ 1.8M stripped)

		allocations allow much more than that !

		some stats:
			recv:   idx: 33528, vtx: 23451, op: 128, pt: 4, tr: 133, mvo: 14, modt: 342
			sc:     idx: 26150, vtx: 17417, op: 162, pt: 12, tr: 244, mvo: 6, modt: 2044
			doa2le: idx: 47178, vtx: 34046, op: 868, pt: 0, tr: 354, mvo: 92, modt: 976 (overruns)
			ika:    idx: 46748, vtx: 33818, op: 984, pt: 9, tr: 234, mvo: 10, modt: 16, ov: 0  
			ct:     idx: 30920, vtx: 21468, op: 752, pt: 0, tr: 360, mvo: 101, modt: 732, ov: 0
			sa2:    idx: 36094, vtx: 24520, op: 1330, pt: 10, tr: 177, mvo: 39, modt: 360, ov: 0
	*/

	void MarkRend()
	{
		rend.proc_start = tad.thd_root;
		rend.proc_end   = tad.End();
	}
	void Alloc()
	{
#if !defined(TARGET_NO_THREADS)
      thd_inuse  = slock_new();
      rend_inuse = slock_new();
#endif
      tad.Reset((u8*)malloc(2*1024*1024));

		rend.verts.InitBytes(1024*1024,&rend.Overrun); //up to 1 mb of vtx data/frame = ~ 38k vtx/frame
		rend.idx.Init(60*1024,&rend.Overrun);			//up to 60K indexes ( idx have stripification overhead )
		rend.global_param_op.Init(4096,&rend.Overrun);
		rend.global_param_pt.Init(4096,&rend.Overrun);
		rend.global_param_mvo.Init(4096,&rend.Overrun);
		rend.global_param_tr.Init(4096,&rend.Overrun);

		rend.modtrig.Init(8192,&rend.Overrun);
		
		Reset();
	}

	void Reset()
	{
      tad.Clear();

#if !defined(TARGET_NO_THREADS)
      slock_lock(rend_inuse);
#endif
		rend.Clear();
		rend.proc_end = rend.proc_start = tad.thd_root;
#if !defined(TARGET_NO_THREADS)
      slock_unlock(rend_inuse);
#endif
	}

	void Free()
	{
#if !defined(TARGET_NO_THREADS)
      slock_free(thd_inuse);
      slock_free(rend_inuse);
      thd_inuse  = NULL;
      rend_inuse = NULL;
#endif
		free(tad.thd_root);
		rend.verts.Free();
		rend.idx.Free();
		rend.global_param_op.Free();
		rend.global_param_pt.Free();
		rend.global_param_tr.Free();
		rend.modtrig.Free();
		rend.global_param_mvo.Free();
	}
};


extern TA_context* ta_ctx;
extern tad_context ta_tad;

extern TA_context*  vd_ctx;
extern rend_context vd_rc;

TA_context* tactx_Pop(u32 addr);

void tactx_Recycle(TA_context* poped_ctx);

/*
	Ta Context

	Rend Context
*/

#define TACTX_NONE (0xFFFFFFFF)

void SetCurrentTARC(u32 addr);
bool QueueRender(TA_context* ctx);
TA_context* DequeueRender();
void FinishRender(TA_context* ctx);

//must be moved to proper header
void FillBGP(TA_context* ctx);
bool UsingAutoSort(void);
void ta_ctx_init(void);
void ta_ctx_free(void);
