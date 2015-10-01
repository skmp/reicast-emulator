#ifndef GLES
#define attr "in"
#define vary "out"
#else
#define attr "attribute"
#define vary "varying"
#endif
#if 1

//Fragment and vertex shaders code
//pretty much 1:1 copy of the d3d ones for now
const char* VertexShaderSource =
#ifndef GLES
	"#version 140 \n"
#endif
"\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp vec4      depth_scale; \n\
uniform highp float sp_FOG_DENSITY; \n\
/* Vertex input */ \n\
" attr " highp vec4    in_pos; \n\
" attr " lowp vec4     in_base; \n\
" attr " lowp vec4     in_offs; \n\
" attr " mediump vec2  in_uv; \n\
/* output */ \n\
" vary " lowp vec4 vtx_base; \n\
" vary " lowp vec4 vtx_offs; \n\
" vary " mediump vec2 vtx_uv; \n\
" vary " highp vec3 vtx_xyz; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	vec4 vpos=in_pos; \n\
	vtx_xyz.xy = vpos.xy;  \n\
	vtx_xyz.z = vpos.z*sp_FOG_DENSITY;  \n\
	vpos.w=1.0/vpos.z;  \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	vpos.z=depth_scale.x+depth_scale.y*vpos.w;  \n\
	gl_Position = vpos; \n\
}";


#else



const char* VertexShaderSource =
				""
				"/* Test Projection Matrix */"
				""
				"uniform highp mat4  Projection;"
				""
				""
				"/* Vertex constants */"
				""
				"uniform highp float sp_FOG_DENSITY;"
				"uniform highp vec4  scale;"
				""
				"/* Vertex output */"
				""
				"attribute highp   vec4 in_pos;"
				"attribute lowp    vec4 in_base;"
				"attribute lowp    vec4 in_offs;"
				"attribute mediump vec2 in_uv;"
				""
				"/* Transformed input */"
				""
				"varying lowp    vec4 vtx_base;"
				"varying lowp    vec4 vtx_offs;"
				"varying mediump vec2 vtx_uv;"
				"varying highp   vec3 vtx_xyz;"
				""
				"void main()"
				"{"
				"  vtx_base = in_base;"
				"  vtx_offs = in_offs;"
				"  vtx_uv   = in_uv;"
				""
				"  vec4 vpos  = in_pos;"
				"  vtx_xyz.xy = vpos.xy; "
				"  vtx_xyz.z  = vpos.z*sp_FOG_DENSITY; "
				""
				"  vpos.w     = 1.0/vpos.z; "
				"  vpos.z    *= -scale.w; "
				"  vpos.xy    = vpos.xy*scale.xy-sign(scale.xy); "
				"  vpos.xy   *= vpos.w; "
				""
				"  gl_Position = vpos;"
			//	"  gl_Position = vpos * Projection;"
				"}"
				;


#endif

/*

cp_AlphaTest 0 1        2 2
pp_ClipTestMode -1 0 1  3 6
pp_UseAlpha  0 1        2 12
pp_Texture 1
	pp_IgnoreTexA 0 1       2   2
	pp_ShadInstr 0 1 2 3    4   8
	pp_Offset 0 1           2   16
	pp_FogCtrl 0 1 2 3      4   64
pp_Texture 0
	pp_FogCtrl 0 2 3        4   4

pp_Texture: off -> 12*4=48 shaders
pp_Texture: on  -> 12*64=768 shaders
Total: 816 shaders

highp float fdecp(highp float flt,out highp float e)  \n\
{  \n\
	highp float lg2=log2(flt);  //ie , 2.5  \n\
	highp float frc=fract(lg2); //ie , 0.5  \n\
	e=lg2-frc;                  //ie , 2.5-0.5=2 (exp)  \n\
	return pow(2.0,frc);        //2^0.5 (manitsa)  \n\
}  \n\
lowp float fog_mode2(highp float invW)  \n\
{  \n\
	highp float foginvW=invW;  \n\
	foginvW=clamp(foginvW,1.0,255.0);  \n\
	  \n\
	highp float fogexp;                                 //0 ... 7  \n\
	highp float fogman=fdecp(foginvW, fogexp);          //[1,2) mantissa bits. that is 1.m  \n\
	  \n\
	highp float fogman_hi=fogman*16.0-16.0;             //[16,32) -16 -> [0,16)  \n\
	highp float fogman_idx=floor(fogman_hi);            //[0,15]  \n\
	highp float fogman_blend=fract(fogman_hi);          //[0,1) -- can also be fogman_idx-fogman_idx !  \n\
	highp float fog_idx_fr=fogexp*16.0+fogman_idx;      //[0,127]  \n\
	  \n\
	highp float fog_idx_pixel_fr=fog_idx_fr+0.5;  \n\
	highp float fog_idx_pixel_n=fog_idx_pixel_fr/128.0;//normalise to [0.5/128,127.5/128) coordinates  \n\
  \n\
	//fog is 128x1 texure  \n\
	lowp vec2 fog_coefs=texture2D(fog_table,vec2(fog_idx_pixel_n)).rg;  \n\
  \n\
	lowp float fog_coef=mix(fog_coefs.r,fog_coefs.g,fogman_blend);  \n\
	  \n\
	return fog_coef;  \n\
} \n\
*/

#ifndef GLES
#define FRAGCOL "FragColor"
#define TEXLOOKUP "texture"
#define vary "in"
#else
#define FRAGCOL "gl_FragColor"
#define TEXLOOKUP "texture2D"
#endif


const char* PixelPipelineShader =
#ifndef GLES
	"#version 140 \n"
	"out vec4 FragColor; \n"
#endif
"\
\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d.0 \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
/* Shader program params*/ \n\
/* gles has no alpha test stage, so its emulated on the shader */ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp vec2 sp_LOG_FOG_COEFS; \n\
uniform sampler2D tex,fog_table; \n\
/* Vertex input*/ \n\
" vary " lowp vec4 vtx_base; \n\
" vary " lowp vec4 vtx_offs; \n\
" vary " mediump vec2 vtx_uv; \n\
" vary " highp vec3 vtx_xyz; \n\
lowp float fog_mode2(highp float val) \n\
{ \n\
	highp float fog_idx=clamp(val,0.0,127.99); \n\
	return clamp(sp_LOG_FOG_COEFS.y*log2(fog_idx)+sp_LOG_FOG_COEFS.x,0.001,1.0); //the clamp is required due to yet another bug !\n\
} \n\
void main() \n\
{ \n\
	lowp vec4 color=vtx_base; \n\
	#if pp_UseAlpha==0 \n\
		color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 \n\
		color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(vtx_xyz.z)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		lowp vec4 texcol=" TEXLOOKUP "(tex,vtx_uv); \n\
		\n\
		#if pp_IgnoreTexA==1 \n\
			texcol.a=1.0;	 \n\
		#endif\n\
		\n\
		#if pp_ShadInstr==0 \n\
		{ \n\
			color.rgb=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==1 \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==2 \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
		#if  pp_ShadInstr==3 \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 \n\
		{ \n\
			color.rgb+=vtx_offs.rgb; \n\
			if (pp_FogCtrl==1) \n\
				color.rgb=mix(color.rgb,sp_FOG_COL_VERT.rgb,vtx_offs.a); \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	#if pp_FogCtrl==0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(vtx_xyz.z));  \n\
	} \n\
	#endif\n\
	#if cp_AlphaTest == 1 \n\
		if (cp_AlphaTestValue>color.a) discard;\n\
	#endif  \n\
	//color.rgb=vec3(vtx_xyz.z/255.0);\n\
	" FRAGCOL "=color; \n\
}";

const char* ModifierVolumeShader =
#ifndef GLES
	"#version 140 \n"
	"out vec4 FragColor; \n"
#endif
" \
uniform lowp float sp_ShaderColor; \n\
/* Vertex input*/ \n\
void main() \n\
{ \n\
	" FRAGCOL "=vec4(0.0, 0.0, 0.0, sp_ShaderColor); \n\
}";

const char* OSD_Shader =
#ifndef GLES
	"#version 140 \n"
	"out vec4 FragColor; \n"
#endif
" \
" vary " lowp vec4 vtx_base; \n\
" vary " mediump vec2 vtx_uv; \n\
/* Vertex input*/ \n\
uniform sampler2D tex; \n\
void main() \n\
{ \n\
	mediump vec2 uv=vtx_uv; \n\
	uv.y=1.0-uv.y; \n\
	" FRAGCOL "=vtx_base*" TEXLOOKUP "(tex,uv.st); \n\n\
}";
