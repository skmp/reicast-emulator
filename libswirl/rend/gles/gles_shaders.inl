
//Fragment and vertex shaders code
const char* VertexShaderSource =
"\
%s \n\
#define TARGET_GL %s \n\
#define pp_Gouraud %d \n\
#define ROTATE_90 %d \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute \n\
#define out varying \n\
#endif \n\
 \n\
 \n\
#if TARGET_GL == GL3 || TARGET_GL == GLES3 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
#else \n\
#define INTERPOLATION \n\
#endif \n\
 \n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp vec4      depth_scale; \n\
uniform highp float     extra_depth_scale; \n\
/* Vertex input */ \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in lowp vec4     in_offs; \n\
in mediump vec2  in_uv; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
              out mediump vec2 vtx_uv; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	highp vec4 vpos = in_pos; \n\
	if (vpos.z < 0.0 || vpos.z > 3.4e37) \n\
	{ \n\
		gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z); \n\
		return; \n\
	} \n\
	\n\
	vpos.w = extra_depth_scale / vpos.z; \n\
#if TARGET_GL != GLES2 \n\
	vpos.z = vpos.w; \n\
#else \n\
	vpos.z=depth_scale.x+depth_scale.y*vpos.w;  \n\
#endif \n\
#if ROTATE_90 == 1 \n\
	vpos.xy = vec2(vpos.y, -vpos.x);  \n\
#endif \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

const char* PixelPipelineShader =
"\
%s \n\
#define TARGET_GL %s \n\
\n\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define pp_TriLinear %d \n\
#define PI 3.1415926 \n\
\n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES3 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#define FOG_CHANNEL a \n\
#elif TARGET_GL == GL3 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#define FOG_CHANNEL r \n\
#else \n\
#define in varying \n\
#define texture texture2D \n\
#define FOG_CHANNEL a \n\
#endif \n\
 \n\
 \n\
#if TARGET_GL == GL3 || TARGET_GL == GLES3 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
#else \n\
#define INTERPOLATION \n\
#endif \n\
 \n\
/* Shader program params*/ \n\
/* gles has no alpha test stage, so its emulated on the shader */ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform sampler2D tex,fog_table; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
uniform highp float extra_depth_scale; \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
			  in mediump vec2 vtx_uv; \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
	highp float z = clamp(w * extra_depth_scale * sp_FOG_DENSITY, 1.0, 255.9999); \n\
	highp float exp = floor(log2(z)); \n\
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
	lowp float idx = floor(m) + exp * 16.0 + 0.5; \n\
	highp vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
	return fog_coef.FOG_CHANNEL; \n\
} \n\
 \n\
highp vec4 fog_clamp(lowp vec4 col) \n\
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
	lowp vec4 color=vtx_base; \n\
	#if pp_UseAlpha==0 \n\
		color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 \n\
		color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		lowp vec4 texcol=texture(tex, vtx_uv); \n\
		 \n\
		#if pp_BumpMap == 1 \n\
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 \n\
				texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
		#endif \n\
		#if pp_ShadInstr==0 \n\
		{ \n\
			color=texcol; \n\
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
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
			color.rgb+=vtx_offs.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	 \n\
	color = fog_clamp(color); \n\
	 \n\
	#if pp_FogCtrl == 0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
	} \n\
	#endif\n\
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_VERT.rgb,vtx_offs.a); \n\
	} \n\
	#endif\n\
	 \n\
	#if pp_TriLinear == 1 \n\
	color *= trilinear_alpha; \n\
	#endif \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
		color.a=1.0; \n\
	#endif  \n\
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);\n\
#if TARGET_GL != GLES2 \n\
	highp float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
#endif \n\
	gl_FragColor =color; \n\
}";

const char* ModifierVolumeShader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL != GLES2 && TARGET_GL != GL2 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#endif \n\
 \n\
uniform lowp float sp_ShaderColor; \n\
/* Vertex input*/ \n\
void main() \n\
{ \n\
#if TARGET_GL != GLES2 \n\
	highp float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
#endif \n\
	gl_FragColor=vec4(0.0, 0.0, 0.0, sp_ShaderColor); \n\
}";

const char* OSD_VertexShader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute \n\
#define out varying \n\
#endif \n\
 \n\
uniform highp vec4      scale; \n\
 \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in mediump vec2  in_uv; \n\
 \n\
out lowp vec4 vtx_base; \n\
out mediump vec2 vtx_uv; \n\
 \n\
void main() \n\
{ \n\
	vtx_base = in_base; \n\
	vtx_uv = in_uv; \n\
	highp vec4 vpos = in_pos; \n\
	\n\
	vpos.w = 1.0; \n\
	vpos.z = vpos.w; \n\
	vpos.xy = vpos.xy * scale.xy - scale.zw;  \n\
	gl_Position = vpos; \n\
}";

const char* OSD_Shader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL != GLES2 && TARGET_GL != GL2 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#else \n\
#define in varying \n\
#define texture texture2D \n\
#endif \n\
 \n\
in lowp vec4 vtx_base; \n\
in mediump vec2 vtx_uv; \n\
/* Vertex input*/ \n\
uniform sampler2D tex; \n\
void main() \n\
{ \n\
	mediump vec2 uv=vtx_uv; \n\
	uv.y=1.0-uv.y; \n\
	gl_FragColor = vtx_base*texture(tex,uv.st); \n\
}";