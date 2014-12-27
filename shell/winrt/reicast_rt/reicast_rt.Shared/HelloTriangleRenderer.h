#pragma once

#include "pch.h"

namespace reicast_rt
{
    class HelloTriangleRenderer
    {
    public:
        HelloTriangleRenderer();
        ~HelloTriangleRenderer();
        void Draw(GLsizei width, GLsizei height);

    private:
        GLuint mProgram;
    };
}