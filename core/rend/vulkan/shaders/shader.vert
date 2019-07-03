#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inSpec;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inCol1;
layout(location = 5) in vec4 inSpec1;
layout(location = 6) in vec4 inUV1;

layout(location = 0) out vec4 fragColor;

void main()
{
	const float L = 0, R = 640;
	const float T = 0, B = 480;

	mat4 m_ortho = mat4(
		vec4 ( 2.0f/(R-L),   0.0f,           0.0f,       0.0f ),
		vec4 ( 0.0f,        -2.0f/(T-B),     0.0f,       0.0f ),
		vec4 ( 0.0f,         0.0f,           0.5f,       0.0f ),
		vec4 ( (R+L)/(L-R), -(T+B)/(B-T),    0.5f,       1.0f )
	);


    gl_Position = m_ortho * vec4(inPosition, 1.0);
    fragColor = inColor;
}