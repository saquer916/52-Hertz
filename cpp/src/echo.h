#pragma once
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

class Level;

// Echolocation: rays go out from the whale's head, and every hit becomes a gray
// dot that appears once the echo has had time to come back, then fades away.
class Echo {
 public:
  // One dot on a wall: where it is, when it starts to fade in, and what it hit.
  struct Dot {
    glm::vec4 posAppear;  // xyz, w = time it starts fading in
    float kind;           // 0 = rock, 1 = pod
  };
  struct Call {
    float strength;  // 0 = quick click .. 1 = full 1.2 s call
    float reach;
    int rays;
    float cone;  // half-angle in degrees (180 = every direction)
  };
  // GAMEPLAY (written in gameplay.cpp): how strong a call is for how long Space was held.
  static Call callFor(float holdSeconds);
  // GAMEPLAY (written in gameplay.cpp): send the call's rays out and turn hits into dots.
  // `targets` are spheres (xyz centre, w radius) that echo back in warm gold: the pod.
  // Returns how many rays hit; `targetHits` gets how many of them hit a target.
  int cast(const Level& level, const Call& call, glm::vec3 origin, const glm::mat3& basis, float now,
           const std::vector<glm::vec4>& targets, int* targetHits = nullptr);

  // ENGINE: set up, draw and clear the dots, and add new ones.
  void init();
  void addDots(const std::vector<Dot>& dots);
  void draw() const;
  void clear();

  std::mt19937 rng{5252};  // random numbers for ray directions

 private:
  static constexpr int kCapacity = 1 << 15;
  int head_ = 0, count_ = 0;
  GLuint vao_ = 0, vbo_ = 0, program_ = 0;
};
