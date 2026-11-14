#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "gl.h"

constexpr int kMaxJoints = 16;

// Procedural swimming, a port of the Godot version's swim_motion.gd: a wave runs
// down the spine to the flukes (faster and stronger with speed), the body curves
// into turns, and the flippers flap and steer.
struct SwimPose {
  float phase = 0.0f;
  float power = 0.5f;
  float turn = 0.0f;  // -1 hard right .. 1 hard left
  void update(float dt, float speed, float boostSpeed, float turnRate);
  float heave() const;
};

struct WhaleDraw {
  glm::mat4 world{1.0f};
  glm::mat4 joints[kMaxJoints];
  float fogAmount = 0.0f;       // 0 = ignore underwater fog (the player), 1 = fog like everything else
  glm::vec3 glow{0.0f};         // extra rim light (the pod)
  glm::vec3 lightPos{0.0f};     // the whale's own light
  float lightStrength = 0.0f;
};

class WhaleModel {
 public:
  bool load(const std::string& dir);
  // Bone matrices for a pose (skin order).
  void pose(const SwimPose& swim, glm::mat4 out[kMaxJoints]) const;
  void draw(const WhaleDraw& d) const;
  // Depth-only draw for the sun's shadow map.
  void drawShadow(const WhaleDraw& d, const glm::mat4& lightViewProj) const;
  int jointCount() const { return (int)joints_.size(); }

  // File coordinates -> whale space (14 m long, head towards -Z, centred).
  static glm::mat4 fileToWhale();

 private:
  struct Joint {
    std::string name;
    int parent = -1;
    glm::vec3 t{0.0f}, s{1.0f};
    glm::quat r{1, 0, 0, 0};
    glm::mat4 inverseBind{1.0f};
    glm::mat4 restGlobal{1.0f};
  };
  int find(const char* name) const;
  glm::vec3 localAxis(int joint, glm::vec3 axis) const;

  std::vector<Joint> joints_;
  glm::mat4 rootParent_{1.0f};
  std::vector<int> spine_;
  int head_ = -1;
  int flippers_[2] = {-1, -1};

  GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
  int indexCount_ = 0;
  GLuint texBase_ = 0, texNormal_ = 0, texORM_ = 0;
  GLuint program_ = 0, shadowProgram_ = 0;
};
