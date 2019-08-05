
static const char* VertexShaderSource =
"\
#version 140 \n\
#define pp_Gouraud %d \n\
#define ROTATE_90 %d \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp float     extra_depth_scale; \n\
/* Vertex input */ \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in lowp vec4     in_offs; \n\
in mediump vec2  in_uv; \n\
in lowp vec4     in_base1; \n\
in lowp vec4     in_offs1; \n\
in mediump vec2  in_uv1; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
			  out mediump vec2 vtx_uv; \n\
INTERPOLATION out lowp vec4 vtx_base1; \n\
INTERPOLATION out lowp vec4 vtx_offs1; \n\
			  out mediump vec2 vtx_uv1; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	vtx_base1 = in_base1; \n\
	vtx_offs1 = in_offs1; \n\
	vtx_uv1 = in_uv1; \n\
	vec4 vpos=in_pos; \n\
	if (vpos.z < 0.0 || vpos.z > 3.4e37) \n\
	{ \n\
		gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z); \n\
		return; \n\
	} \n\
	\n\
	vpos.w = extra_depth_scale / vpos.z; \n\
	vpos.z = vpos.w; \n\
#if ROTATE_90 == 1 \n\
	vpos.xy = vec2(vpos.y, -vpos.x);  \n\
#endif \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

const char* gl4PixelPipelineShader = SHADER_HEADER
"\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_TwoVolumes %d \n\
#define pp_DepthFunc %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define PASS %d \n\
#define PI 3.1415926 \n\
 \n\
#if PASS <= 1 \n\
out vec4 FragColor; \n\
#endif \n\
 \n\
#if pp_TwoVolumes == 1 \n\
#define IF(x) if (x) \n\
#else \n\
#define IF(x) \n\
#endif \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Shader program params*/ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform highp float shade_scale_factor; \n\
uniform sampler2D tex0, tex1; \n\
layout(binding = 5) uniform sampler2D fog_table; \n\
uniform int pp_Number; \n\
uniform usampler2D shadow_stencil; \n\
uniform sampler2D DepthTex; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
 \n\
uniform ivec2 blend_mode[2]; \n\
#if pp_TwoVolumes == 1 \n\
uniform bool use_alpha[2]; \n\
uniform bool ignore_tex_alpha[2]; \n\
uniform int shading_instr[2]; \n\
uniform int fog_control[2]; \n\
#endif \n\
 \n\
uniform highp float extra_depth_scale; \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
			  in mediump vec2 vtx_uv; \n\
INTERPOLATION in lowp vec4 vtx_base1; \n\
INTERPOLATION in lowp vec4 vtx_offs1; \n\
			  in mediump vec2 vtx_uv1; \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
	highp float z = clamp(w * extra_depth_scale * sp_FOG_DENSITY, 1.0, 255.9999); \n\
	highp float exp = floor(log2(z)); \n\
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
	float idx = floor(m) + exp * 16.0 + 0.5; \n\
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
	return fog_coef.r; \n\
} \n\
 \n\
highp vec4 fog_clamp(highp vec4 col) \n\
{ \n\
#if FogClamping == 1 \n\
	return clamp(col, fog_clamp_min, fog_clamp_max); \n\
#else \n\
	return col; \n\
#endif \n\
} \n\
 \n\
void main() \n\
{ \n\
	setFragDepth(); \n\
	\n\
	#if PASS == 3 \n\
		// Manual depth testing \n\
		highp float frontDepth = texture(DepthTex, gl_FragCoord.xy / textureSize(DepthTex, 0)).r; \n\
		#if pp_DepthFunc == 0		// Never \n\
			discard; \n\
		#elif pp_DepthFunc == 1		// Greater \n\
			if (gl_FragDepth <= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 2		// Equal \n\
			if (gl_FragDepth != frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 3		// Greater or equal \n\
			if (gl_FragDepth < frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 4		// Less \n\
			if (gl_FragDepth >= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 5		// Not equal \n\
			if (gl_FragDepth == frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 6		// Less or equal \n\
			if (gl_FragDepth > frontDepth) \n\
				discard; \n\
		#endif \n\
	#endif \n\
	\n\
	// Clip outside the box \n\
	#if pp_ClipTestMode==1 \n\
		if (gl_FragCoord.x < pp_ClipTest.x || gl_FragCoord.x > pp_ClipTest.z \n\
				|| gl_FragCoord.y < pp_ClipTest.y || gl_FragCoord.y > pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	// Clip inside the box \n\
	#if pp_ClipTestMode==-1 \n\
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z \n\
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	\n\
	highp vec4 color = vtx_base; \n\
	lowp vec4 offset = vtx_offs; \n\
	mediump vec2 uv = vtx_uv; \n\
	bool area1 = false; \n\
	ivec2 cur_blend_mode = blend_mode[0]; \n\
	\n\
	#if pp_TwoVolumes == 1 \n\
		bool cur_use_alpha = use_alpha[0]; \n\
		bool cur_ignore_tex_alpha = ignore_tex_alpha[0]; \n\
		int cur_shading_instr = shading_instr[0]; \n\
		int cur_fog_control = fog_control[0]; \n\
		#if PASS == 1 \n\
			uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
			if (stencil.r == 0x81u) { \n\
				color = vtx_base1; \n\
				offset = vtx_offs1; \n\
				uv = vtx_uv1; \n\
				area1 = true; \n\
				cur_blend_mode = blend_mode[1]; \n\
				cur_use_alpha = use_alpha[1]; \n\
				cur_ignore_tex_alpha = ignore_tex_alpha[1]; \n\
				cur_shading_instr = shading_instr[1]; \n\
				cur_fog_control = fog_control[1]; \n\
			} \n\
		#endif\n\
	#endif\n\
	\n\
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1 \n\
		IF(!cur_use_alpha) \n\
			color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2 \n\
		IF(cur_fog_control == 3) \n\
			color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		highp vec4 texcol; \n\
		if (area1) \n\
			texcol = texture(tex1, uv); \n\
		else \n\
			texcol = texture(tex0, uv); \n\
		#if pp_BumpMap == 1 \n\
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1 \n\
				IF(cur_ignore_tex_alpha) \n\
					texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
		#endif\n\
		#if pp_ShadInstr==0 || pp_TwoVolumes == 1 // DECAL \n\
		IF(cur_shading_instr == 0) \n\
		{ \n\
			color=texcol; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==1 || pp_TwoVolumes == 1 // MODULATE \n\
		IF(cur_shading_instr == 1) \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==2 || pp_TwoVolumes == 1 // DECAL ALPHA \n\
		IF(cur_shading_instr == 2) \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
		#if  pp_ShadInstr==3 || pp_TwoVolumes == 1 // MODULATE ALPHA \n\
		IF(cur_shading_instr == 3) \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
			color.rgb += offset.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	#if PASS == 1 && pp_TwoVolumes == 0 \n\
		uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
		if (stencil.r == 0x81u) \n\
			color.rgb *= shade_scale_factor; \n\
	#endif \n\
	 \n\
	color = fog_clamp(color); \n\
	 \n\
	#if pp_FogCtrl==0 || pp_TwoVolumes == 1 // LUT \n\
		IF(cur_fog_control == 0) \n\
		{ \n\
			color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
		} \n\
	#endif\n\
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex \n\
		IF(cur_fog_control == 1) \n\
		{ \n\
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a); \n\
		} \n\
	#endif\n\
	 \n\
	color *= trilinear_alpha; \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
		color.a=1.0; \n\
	#endif  \n\
	\n\
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0); \n\
	\n\
	#if PASS == 1  \n\
		FragColor = color; \n\
	#elif PASS > 1 \n\
		// Discard as many pixels as possible \n\
		switch (cur_blend_mode.y) // DST \n\
		{ \n\
		case ONE: \n\
			switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
					discard; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
					if (color == vec4(0.0)) \n\
						discard; \n\
					break; \n\
				case SRC_ALPHA: \n\
					if (color.a == 0.0 || color.rgb == vec3(0.0)) \n\
						discard; \n\
					break; \n\
				case INVERSE_SRC_ALPHA: \n\
					if (color.a == 1.0 || color.rgb == vec3(0.0)) \n\
						discard; \n\
					break; \n\
			} \n\
			break; \n\
		case OTHER_COLOR: \n\
			if (cur_blend_mode.x == ZERO && color == vec4(1.0)) \n\
				discard; \n\
			break; \n\
		case INVERSE_OTHER_COLOR: \n\
			if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0)) \n\
				discard; \n\
			break; \n\
		case SRC_ALPHA: \n\
			if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0) \n\
				discard; \n\
			break; \n\
		case INVERSE_SRC_ALPHA: \n\
			switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
				case SRC_ALPHA: \n\
					if (color.a == 0.0) \n\
						discard; \n\
					break; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
					if (color == vec4(0.0)) \n\
						discard; \n\
					break; \n\
			} \n\
			break; \n\
		} \n\
		\n\
		ivec2 coords = ivec2(gl_FragCoord.xy); \n\
		uint idx =  getNextPixelIndex(); \n\
		 \n\
		Pixel pixel; \n\
		pixel.color = color; \n\
		pixel.depth = gl_FragDepth; \n\
		pixel.seq_num = uint(pp_Number); \n\
		pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx); \n\
		pixels[idx] = pixel; \n\
		\n\
		discard; \n\
		\n\
	#endif \n\
}";

static const char* ModifierVolumeShader = SHADER_HEADER
" \
/* Vertex input*/ \n\
void main() \n\
{ \n\
	setFragDepth(); \n\
	\n\
}";
