#ifndef LIBRETRO_SDK_GLSM_SYM_H
#define LIBRETRO_SDK_GLSM_SYM_H

#include "glsm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define glBindTexture               rglBindTexture
#define glActiveTexture             rglActiveTexture
#define glFramebufferTexture2D      rglFramebufferTexture2D
#define glFramebufferRenderbuffer   rglFramebufferRenderbuffer
#define glDeleteFramebuffers        rglDeleteFramebuffers
#define glRenderbufferStorage       rglRenderbufferStorage
#define glBindRenderbuffer          rglBindRenderbuffer
#define glDeleteRenderbuffers       rglDeleteRenderbuffers
#define glGenRenderbuffers          rglGenRenderbuffers
#define glGenFramebuffers           rglGenFramebuffers
#define glBindFramebuffer           rglBindFramebuffer
#define glGenerateMipmap            rglGenerateMipmap
#define glCheckFramebufferStatus    rglCheckFramebufferStatus
#define glBindFragDataLocation      rglBindFragDataLocation
#define glBindAttribLocation        rglBindAttribLocation
#define glLinkProgram               rglLinkProgram
#define glGetProgramiv              rglGetProgramiv
#define glGetShaderiv               rglGetShaderiv
#define glAttachShader              rglAttachShader
#define glShaderSource              rglShaderSource
#define glCompileShader             rglCompileShader
#define glCreateProgram             rglCreateProgram
#define glGetShaderInfoLog          rglGetShaderInfoLog
#define glGetProgramInfoLog         rglGetProgramInfoLog
#define glIsProgram                 rglIsProgram
#define glEnableVertexAttribArray   rglEnableVertexAttribArray
#define glDisableVertexAttribArray  rglDisableVertexAttribArray
#define glVertexAttribPointer       rglVertexAttribPointer
#define glGetUniformLocation        rglGetUniformLocation
#define glGenBuffers                rglGenBuffers
#define glDisable(T)                rglDisable(S##T)
#define glEnable(T)                 rglEnable(S##T)
#define glUseProgram                rglUseProgram
#define glDepthMask                 rglDepthMask
#define glStencilMask               rglStencilMask
#define glBufferData                rglBufferData
#define glBindBuffer                rglBindBuffer
#define glCreateShader              rglCreateShader
#define glDeleteShader              rglDeleteShader
#define glUniform1f                 rglUniform1f
#define glUniform1i                 rglUniform1i
#define glUniform2fv                rglUniform2fv
#define glUniform3fv                rglUniform3fv
#define glUniform4f                 rglUniform4f
#define glUniform4fv                rglUniform4fv
#define glBlendFunc                 rglBlendFunc
#define glBlendFuncSeparate         rglBlendFuncSeparate
#define glDepthFunc                 rglDepthFunc

void glBindTexture(GLenum target, GLuint texture);
void glActiveTexture(GLenum texture);
void rglFramebufferTexture2D(GLenum target, GLenum attachment,
      GLenum textarget, GLuint texture, GLint level);
void rglFramebufferRenderbuffer(GLenum target, GLenum attachment,
      GLenum renderbuffertarget, GLuint renderbuffer);
void rglDeleteFramebuffers(GLsizei n, GLuint *framebuffers);
void rglRenderbufferStorage(GLenum target, GLenum internalFormat,
      GLsizei width, GLsizei height);
void rglBindRenderbuffer(GLenum target, GLuint renderbuffer);
void rglDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers);
void rglGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
void rglGenFramebuffers(GLsizei n, GLuint *ids);
void rglBindFramebuffer(GLenum target, GLuint framebuffer);
void rglGenerateMipmap(GLenum target);
GLenum rglCheckFramebufferStatus(GLenum target);
void rglBindFragDataLocation(GLuint program, GLuint colorNumber,
                                   const char * name);
void rglBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
void rglLinkProgram(GLuint program);
void rglGetProgramiv(GLuint shader, GLenum pname, GLint *params);
void rglGetShaderiv(GLuint shader, GLenum pname, GLint *params);
void rglAttachShader(GLuint program, GLuint shader);
void rglShaderSource(GLuint shader, GLsizei count,
      const GLchar **string, const GLint *length);
void rglCompileShader(GLuint shader);
GLuint rglCreateProgram(void);
void rglGetShaderInfoLog(GLuint shader, GLsizei maxLength,
      GLsizei *length, GLchar *infoLog);
void rglGetProgramInfoLog(GLuint shader, GLsizei maxLength,
      GLsizei *length, GLchar *infoLog);
GLboolean rglIsProgram(GLuint program);
void rglEnableVertexAttribArray(GLuint index);
void rglDisableVertexAttribArray(GLuint index);
void rglVertexAttribPointer(GLuint name, GLint size,
      GLenum type, GLboolean normalized, GLsizei stride,
      const GLvoid* pointer);
GLint rglGetUniformLocation(GLuint program, const GLchar *name);
void rglGenBuffers(GLsizei n, GLuint *buffers);
void rglDisable(GLenum cap);
void rglEnable(GLenum cap);
void rglUseProgram(GLuint program);
void rglDepthMask(GLboolean flag);
void rglStencilMask(GLenum mask);
void rglBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
void rglBindBuffer(GLenum target, GLuint buffer);
GLuint rglCreateShader(GLenum shader);
void rglDeleteShader(GLuint shader);
void rglUniform1f(GLint location, GLfloat v0);
void rglUniform1i(GLint location, GLint v0);
void rglUniform2fv(GLint location, GLsizei count, const GLfloat *value);
void rglUniform3fv(GLint location, GLsizei count, const GLfloat *value);
void rglUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void rglUniform4fv(GLint location, GLsizei count, const GLfloat *value);
void rglBlendFunc(GLenum sfactor, GLenum dfactor);
void rglBlendFuncSeparate(GLenum sfactor, GLenum dfactor);
void rglDepthFunc(GLenum func);

#ifdef __cplusplus
}
#endif

#endif
