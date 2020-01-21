#pragma once
#include "gles.h"

class PostProcessor
{
public:
	void Init();
	void Term();
	void SelectFramebuffer();
	void Render(GLuint output_fbo);

	GLuint GetFramebuffer() {
		if (framebuffer == 0)
			Init();
		return framebuffer;
	}

private:
	GLuint texture = 0;
	GLuint framebuffer = 0;
	GLuint depthBuffer = 0;
	GLuint vertexBuffer = 0;
	GLuint vertexArray = 0;
	float width = 0;
	float height = 0;
};

extern PostProcessor postProcessor;
