#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

constexpr int kWaveCount = 7;

struct FrameData {
  glm::mat4 view, proj, viewProj, invViewProj;
  glm::mat4 shadowMat;
  glm::vec4 camPos;
  glm::vec4 sunDir;
  glm::vec4 sunColor;
  glm::vec4 fogColor;
  glm::vec4 water;
  glm::vec4 whaleLight;
  glm::vec4 whaleHead;
  glm::vec4 screen;
  glm::vec4 clipPlane;
  glm::vec4 waveA[kWaveCount];
  glm::vec4 waveB[kWaveCount];
};

extern const char* kCommonGLSL;

GLuint compileProgram(const char* name, const std::string& vertex, const std::string& fragment);

void initFrameBlock();
void uploadFrameBlock(const FrameData& data);

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
  std::vector<unsigned char> pixels;
};
Image decodePNG(const std::string& path, int maxSize);
GLuint uploadTexture(const Image& image, bool srgb);
