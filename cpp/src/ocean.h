#pragma once
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"
#include "render_util.h"

class Ocean {
 public:
  void init();
  void fillWaves(FrameData& frame) const;
  float heightAt(float x, float z, float t) const;
  void addSplash(glm::vec2 xz, float time, float strength);
  void draw(const glm::vec3& camPos) const;
  void setWalls(const std::vector<glm::vec4>& footprints);

 private:
  struct Wave {
    glm::vec2 dir;
    float k, amp, steep, omega, phase;
  };
  glm::vec3 displace(glm::vec2 xz, float t) const;
  Wave waves_[kWaveCount];
  glm::vec4 splashes_[4] = {};
  std::vector<glm::vec4> walls_;
  int nextSplash_ = 0;
  GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
  int indexCount_ = 0;
  GLuint program_ = 0;
};
