#include "gl.h"

#include <GLFW/glfw3.h>

#define GL_DEFINE(type, name) type name = nullptr;
GL_FUNCTIONS(GL_DEFINE)
#undef GL_DEFINE

const char* loadGL() {
#define GL_LOAD(type, name)                                                    \
  name = reinterpret_cast<type>(glfwGetProcAddress(#name));                    \
  if (!name) return #name;
  GL_FUNCTIONS(GL_LOAD)
#undef GL_LOAD
  return nullptr;
}
