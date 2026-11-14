#pragma once
#include <vector>

#include <glm/glm.hpp>

#include "gl.h"

struct Box {
  glm::vec3 lo, hi;
};

struct RayHit {
  float t = 0.0f;
  glm::vec3 pos{0.0f}, normal{0.0f};
};

// The cliff-wall maze: a 5x4 grid of 50 m cells carved by a randomised
// depth-first search that likes to keep going straight (a port of gen_maze.py).
class Level {
 public:
  static constexpr int kCols = 7, kRows = 6;
  static constexpr float kCell = 50.0f, kThick = 5.0f;
  static constexpr float kFloorY = -59.0f, kWallTop = 30.0f;

  void init();  // GL resources
  void generate(unsigned seed);
  void draw() const;
  void drawShadow(const glm::mat4& lightViewProj) const;

  bool raycast(glm::vec3 from, glm::vec3 dir, float maxDist, RayHit& hit) const;
  bool resolveSphere(glm::vec3& center, float radius, glm::vec3& normal) const;
  static float rockOffset(glm::vec3 p);
  // Height of the (gently rolling) seafloor you see.
  static float floorHeight(float x, float z);

  glm::vec3 start{0.0f, -10.0f, 0.0f};
  float startYaw = -1.5707963f;  // facing +X
  glm::vec3 goal{0.0f};
  glm::vec3 exitGap{0.0f};
  unsigned seedUsed = 0;
  int shortestPath = 0;
  glm::vec4 bounds{0.0f};  // min x, min z, max x, max z of the maze
  // The shortest way through: cell centres from the start to the exit, then the pod.
  std::vector<glm::vec3> route;
  // Wall footprints (min x, min z, max x, max z) for the sea's shoreline foam.
  std::vector<glm::vec4> footprints() const;

 private:
  void buildMesh();
  std::vector<Box> walls_;
  GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
  int indexCount_ = 0;
  GLuint program_ = 0, shadowProgram_ = 0;
};
