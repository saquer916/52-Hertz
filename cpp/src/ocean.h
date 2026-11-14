#pragma once
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"
#include "render_util.h"

// The sea surface at y = 0: a sum of Gerstner swells (visual only; gameplay
// treats the water line as flat y = 0).
class Ocean {
 public:
  void init();
  // Fills the wave tables of the per-frame block.
  void fillWaves(FrameData& frame) const;
  // Height of the waves above a point (for the camera's above/below test).
  float heightAt(float x, float z, float t) const;
  // A splash leaves a patch of whitewater on the surface for a few seconds.
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
  glm::vec4 splashes_[4] = {};  // x, z, start time, strength
  std::vector<glm::vec4> walls_;
  int nextSplash_ = 0;
  GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
  int indexCount_ = 0;
  GLuint program_ = 0;
};
