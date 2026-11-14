#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

// Immediate-mode 2D drawing in pixels: rounded rectangles, rings and text
// (stb_easy_font, scaled up).
class Hud {
 public:
  enum Align { Left, Center, Right };
  void init();
  void begin(int width, int height);
  void rect(glm::vec2 pos, glm::vec2 size, glm::vec4 color, float radius = 0.0f);
  // An arc of a ring from angle a0 to a1 (radians, 0 = up, clockwise).
  void ring(glm::vec2 center, float radius, float thickness, float a0, float a1, glm::vec4 color);
  void text(glm::vec2 pos, const std::string& s, float scale, glm::vec4 color, Align align = Left,
            bool shadow = true);
  float textWidth(const std::string& s, float scale) const;
  void end();

 private:
  struct Vertex {
    glm::vec2 pos;
    glm::vec4 color;
    glm::vec2 local;  // position relative to the shape's centre
    glm::vec4 shape;  // rect: half size, radius, 0 | ring: radius, thickness, a0, a1 (with kind in color sign)
    float kind;       // 0 = rect, 1 = ring
  };
  void quad(glm::vec2 a, glm::vec2 b, glm::vec4 color, glm::vec2 center, glm::vec4 shape, float kind);
  std::vector<Vertex> verts_;
  int width_ = 1, height_ = 1;
  GLuint vao_ = 0, vbo_ = 0, program_ = 0;
};
