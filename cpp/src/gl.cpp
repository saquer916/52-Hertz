#include "gl.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>

#ifdef __EMSCRIPTEN__
extern "C" void glClearDepthf(GLfloat depth);  // provided by Emscripten's WebGL library
#endif

// Functions OpenGL 4.1 (macOS) doesn't have; the game works without them there.
static bool optionalOnGL41(const char* name) {
#ifdef __EMSCRIPTEN__
  std::fprintf(stderr, "(WebGL has no %s; carrying on without it)\n", name);
  return true;
#elif defined(HERTZ_GL41)
  return std::strcmp(name, "glClipControl") == 0 || std::strcmp(name, "glDebugMessageCallback") == 0;
#else
  (void)name;
  return false;
#endif
}

#define GL_DEFINE(type, name) type name = nullptr;
GL_FUNCTIONS(GL_DEFINE)
#undef GL_DEFINE

const char* loadGL() {
#define GL_LOAD(type, name)                                                    \
  name = reinterpret_cast<type>(glfwGetProcAddress(#name));                    \
  if (!name && !optionalOnGL41(#name)) return #name;
  GL_FUNCTIONS(GL_LOAD)
#undef GL_LOAD
#ifdef __EMSCRIPTEN__
  // WebGL only has the float version of glClearDepth.
  if (!glClearDepth) glClearDepth = [](GLdouble d) { glClearDepthf((GLfloat)d); };
#endif
  return nullptr;
}
