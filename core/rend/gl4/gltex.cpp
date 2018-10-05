#include <libretro.h>

#include "gl4.h"
#include "../gles/glcache.h"

GLuint gl4BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt)
{
	FBT& rv=fb_rtt;

	if (rv.fbo)
      glDeleteFramebuffers(1,&rv.fbo);
	if (rv.tex)
      glcache.DeleteTextures(1,&rv.tex);

	rv.TexAddr=addy>>3;

   // Find the smallest power of two texture that fits the viewport
   int fbh2 = 8;
   while (fbh2 < fbh)
      fbh2 *= 2;
   int fbw2 = 8;
   while (fbw2 < fbw)
      fbw2 *= 2;

   if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
	{
		fbw *= settings.rend.RenderToTextureUpscale;
		fbh *= settings.rend.RenderToTextureUpscale;
		fbw2 *= settings.rend.RenderToTextureUpscale;
		fbh2 *= settings.rend.RenderToTextureUpscale;
	}
	/* Get the currently bound frame buffer object. On most platforms this just gives 0. */

	/* Create a texture for rendering to */
	rv.tex = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, rv.tex);

	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, fmt, 0);

	/* Create the object that will allow us to render to the aforementioned texture */
	glGenFramebuffers(1, &rv.fbo);
	glBindFramebuffer(RARCH_GL_FRAMEBUFFER, rv.fbo);

	/* Attach the texture to the FBO */
	glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rv.tex, 0);

	/* Check that our FBO creation was successful */
	GLuint uStatus = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);

	verify(uStatus == RARCH_GL_FRAMEBUFFER_COMPLETE);

   glViewport(0, 0, fbw, fbh);		// TODO CLIP_X/Y min?

   return rv.fbo;
}
