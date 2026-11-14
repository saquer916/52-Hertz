#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

constexpr int kWaveCount = 7;

// Per-frame values every shader can read (uniform block "Frame", binding 0).
// Laid out for std140: only mat4 and vec4 members.
struct FrameData {
  glm::mat4 view, proj, viewProj, invViewProj;
  glm::mat4 shadowMat;  // world -> sun shadow map texture coordinates
  glm::vec4 camPos;     // xyz, w = time in seconds
  glm::vec4 sunDir;     // xyz towards the sun
  glm::vec4 sunColor;   // rgb sunlight, w = ambient strength
  glm::vec4 fogColor;   // rgb water colour at the camera's depth, w = visibility in metres
  glm::vec4 water;      // x = camera depth below the surface, y = darkness 0..1, z = fog begin, w = 1 when underwater
  glm::vec4 whaleLight; // xyz position of the whale's own light, w = intensity
  glm::vec4 whaleHead;  // xyz where echoes come from
  glm::vec4 screen;     // xy = size in pixels, zw = 1/size
  glm::vec4 clipPlane;  // geometry with dot(plane, (p, 1)) < 0 is cut (for the reflection pass)
  glm::vec4 waveA[kWaveCount]; // xy direction, z = wavenumber k, w = amplitude
  glm::vec4 waveB[kWaveCount]; // x = steepness Q, y = angular speed, z = phase
};

// Shared GLSL (version line, Frame block, noise, sky, water fog, caustics)
// that is pasted in front of every shader.
extern const char* kCommonGLSL;

GLuint compileProgram(const char* name, const std::string& vertex, const std::string& fragment);

void initFrameBlock();
void uploadFrameBlock(const FrameData& data);

// A small tileable noise texture (R,A = fbm, G,B = its slope), bound to unit 7.
void initNoiseTexture();

void drawFullscreenTriangle();

struct RenderTarget {
  GLuint fbo = 0, color = 0, depth = 0;
  int width = 0, height = 0;
  void create(int w, int h, GLenum colorFormat, bool withDepth);
  void destroy();
};

struct Image {
  int width = 0, height = 0;
  std::vector<unsigned char> pixels;  // RGBA8
};
// Decoding is thread-safe (no GL calls); uploading must happen on the GL thread.
Image decodePNG(const std::string& path, int maxSize);
GLuint uploadTexture(const Image& image, bool srgb);
