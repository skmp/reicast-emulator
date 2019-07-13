#ifndef RGLGEN_PRIV_DECL_H__
#define RGLGEN_PRIV_DECL_H__
#ifdef __cplusplus
extern "C" {
#endif

#if defined(FIX_MISSING_GLDOUBLE_BUG)
typedef double GLdouble;
#endif

#ifndef GL_STENCIL_BITS
#define GL_STENCIL_BITS 0x0D57
#endif

#ifdef __cplusplus
}
#endif
#endif
