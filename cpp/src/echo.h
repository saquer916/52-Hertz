#pragma once
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

class Level;

class Echo {
 public:
  struct Dot {
    glm::vec4 posAppear;
    float kind;
  };
  struct Call {
    float strength;
    float reach;
    int rays;
    float cone;
  };
  static Call callFor(float holdSeconds);
  int cast(const Level& level, const Call& call, glm::vec3 origin, const glm::mat3& basis, float now,
           const std::vector<glm::vec4>& targets, int* targetHits = nullptr);

  void init();
  void addDots(const std::vector<Dot>& dots);
  void draw() const;
  void clear();

  std::mt19937 rng{5252};

 private:
  static constexpr int kCapacity = 1 << 15;
  int head_ = 0, count_ = 0;
  GLuint vao_ = 0, vbo_ = 0, program_ = 0;
};
