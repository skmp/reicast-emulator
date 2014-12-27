//            Based on Hello_Triangle.c from
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com

//
// This file is used by the template to render a basic scene using GL.
//

#include "pch.h"
#include "HelloTriangleRenderer.h"

// These are used by the shader compilation methods.
#include <vector>
#include <iostream>
#include <fstream>

using namespace Platform;

using namespace reicast_rt;


extern int screen_width, screen_height;
bool rend_single_frame();
bool gles_init();

HelloTriangleRenderer::HelloTriangleRenderer() :
    mProgram(0)
{
	gles_init();
	/*
    // Vertex Shader source
    const std::string vs = STRING
    (
        attribute vec4 vPosition;
        void main()
        {
            gl_Position = vPosition;
        }
    );

    // Fragment Shader source
    const std::string fs = STRING
    (
        precision mediump float;
        void main()
        {
            gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    );

    // CompileProgram will throw if it fails, so we don't need to check for success.
    mProgram = CompileProgram(vs, fs);
	*/
}

HelloTriangleRenderer::~HelloTriangleRenderer()
{
	/*
		if (mProgram != 0)
		{
			glDeleteProgram(mProgram);
			mProgram = 0;
		}
	*/
}

// Draws a basic triangle
void HelloTriangleRenderer::Draw(GLsizei width, GLsizei height)
{

	screen_width = width;
	screen_height = height;

	glClearColor(0.65f, 0.65f, 0.65f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	while (!rend_single_frame());

	/*
		glViewport(0, 0, width, height);

		GLfloat vertices[] =
		{
			 0.0f,  0.5f, 0.0f,
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
		};

		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(mProgram);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
		glEnableVertexAttribArray(0);

		glDrawArrays(GL_TRIANGLES, 0, 3);
	*/
}

