// rlgl.h includes <GLES2/gl2ext.h> for the extension enums. Apple's
// equivalent covers most of them; the rest, which rlgl only names inside
// blocks guarded by an extension check or a #define, are given their Khronos
// values here so the file compiles (none of these formats are used by the
// game, and the ES 3.0 core names, e.g. GL_DEPTH_COMPONENT24, are what run).
#pragma once
#include <OpenGLES/ES3/glext.h>

#ifndef GL_DEPTH_COMPONENT24_OES
#define GL_DEPTH_COMPONENT24_OES 0x81A6
#endif
#ifndef GL_DEPTH_COMPONENT32_OES
#define GL_DEPTH_COMPONENT32_OES 0x81A7
#endif
#ifndef GL_RED_EXT
#define GL_RED_EXT 0x1903
#endif
#ifndef GL_R32F_EXT
#define GL_R32F_EXT 0x822E
#endif
#ifndef GL_RGB32F_EXT
#define GL_RGB32F_EXT 0x8815
#endif
#ifndef GL_RGBA32F_EXT
#define GL_RGBA32F_EXT 0x8814
#endif
#ifndef GL_HALF_FLOAT_OES
#define GL_HALF_FLOAT_OES 0x8D61
#endif
#ifndef GL_ETC1_RGB8_OES
#define GL_ETC1_RGB8_OES 0x8D64
#endif
#ifndef GL_MIRROR_CLAMP_EXT
#define GL_MIRROR_CLAMP_EXT 0x8742
#endif
