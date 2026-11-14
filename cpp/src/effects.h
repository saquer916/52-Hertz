#pragma once
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

class Effects {
 public:
  void init();
  void splash(glm::vec3 where, float strength);
  void pulse(glm::vec3 origin, glm::vec3 forward, float reach, float coneDeg, float now, glm::vec3 color);
  void update(float dt, float now);
  void drawUnderwater() const;
  void drawPulses(float now) const;
  void drawParticles() const;
  int particleCount() const { return (int)particles_.size(); }

 private:
  struct Particle {
    glm::vec3 pos, vel;
    float life, maxLife, size, growth, kind, alpha;
  };
  struct Pulse {
    glm::vec3 origin, forward, color;
    float reach, cosCone, start;
  };
  std::vector<Particle> particles_;
  std::vector<Pulse> pulses_;
  std::mt19937 rng_{99};

  GLuint particleProg_ = 0, particleVao_ = 0, particleVbo_ = 0;
  GLuint snowProg_ = 0, snowVao_ = 0, snowVbo_ = 0;
  GLuint shaftProg_ = 0, shaftVao_ = 0, shaftVbo_ = 0;
  GLuint pulseProg_ = 0, pulseVao_ = 0, pulseVbo_ = 0, pulseIbo_ = 0;
  int pulseIndexCount_ = 0;
  static constexpr int kSnow = 3000, kShafts = 70;
};
