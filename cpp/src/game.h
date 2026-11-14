#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "echo.h"
#include "effects.h"
#include "hud.h"
#include "level.h"
#include "ocean.h"
#include "post.h"
#include "render_util.h"
#include "whale_model.h"

struct InputState {
  float mouseDx = 0.0f, mouseDy = 0.0f;  // pixels this frame
  bool boost = false, slow = false, echo = false;
  bool restart = false;                  // pressed this frame
  bool mouseCaptured = true;
};

struct Whale {
  static constexpr float kCruise = 8.0f, kBoost = 18.0f, kSlow = 3.0f;
  glm::vec3 pos{0.0f}, vel{0.0f};
  float yaw = 0.0f, pitch = 0.0f, targetYaw = 0.0f, targetPitch = 0.0f;
  float speed = kCruise, bank = 0.0f, turnRate = 0.0f;
  bool underwater = true;
  SwimPose swim;
  glm::mat3 basis() const;  // yaw and pitch (no bank): -Z is forward
  glm::vec3 forward() const { return basis() * glm::vec3(0, 0, -1); }
  glm::mat4 modelMatrix() const;
  glm::vec3 head() const { return pos + basis() * glm::vec3(0, 0, -6.8f); }
};

class Game {
 public:
  bool init(int width, int height, unsigned seed);
  void resize(int width, int height);
  void newMaze(unsigned seed);
  void update(float dt, const InputState& in);
  void render();
  const RenderTarget& output() const { return final_; }
  const RenderTarget& reflection() const { return reflection_; }
  std::string status() const;

  // Scripted runs (screenshots) use these to set up a shot.
  Whale& whale() { return whale_; }
  const Level& level() const { return level_; }
  void snapCamera();
  void setHudVisible(bool v) { hudVisible_ = v; }
  float breath() const { return breath_; }
  void setBreath(float b) { breath_ = b; }
  bool won() const { return won_; }

 private:
  // --- Gameplay (gameplay.cpp) ---
  void updateEchoInput(float dt, const InputState& in);
  void updateBreath(float dt);
  void updateVelocity(Whale& w, float dt);
  void checkWin();
  // --- Engine helpers the gameplay calls ---
  void castEcho(float hold);  // sends a call (uses Echo::callFor / Echo::cast)
  void startBounceBack();     // out of breath: fade out and move to lastBreathPos_
  void winGame();             // found the pod
  void updateBounceBack(float dt);

  void updateWhale(float dt, const InputState& in);
  void updateCamera(float dt);
  void podWhale(int i, glm::vec3& pos, float& yaw) const;
  void drawWhales(const WhaleDraw& player);
  void drawHud();
  void message(const std::string& text, float seconds);

  WhaleModel whaleModel_;
  Level level_;
  Ocean ocean_;
  Echo echo_;
  Effects effects_;
  Hud hud_;
  Post post_;
  RenderTarget scene_, final_, reflection_;
  GLuint shadowFbo_ = 0, shadowTex_ = 0;
  GLuint skyProg_ = 0;
  int width_ = 0, height_ = 0;

  Whale whale_;
  glm::vec3 camPos_{0.0f}, camAim_{0.0f};
  glm::vec3 sunDir_{0.0f};
  float time_ = 0.0f;
  bool echoHeld_ = false;
  float holdTime_ = 0.0f;

  float breath_ = 1.0f;
  glm::vec3 lastBreathPos_{0.0f};
  float lastBreathYaw_ = 0.0f;
  int bounceState_ = 0;  // 0 none, 1 fading out, 2 fading in
  float bounceTimer_ = 0.0f;
  int bounces_ = 0;

  bool won_ = false;
  float wonAt_ = 0.0f, nextPodPulse_ = 0.0f;
  float podAnswerAt_ = -1.0f;  // the pod calls back when your echo reaches it
  std::string message_;
  float messageUntil_ = 0.0f;
  float splashReadyAt_ = 0.0f;
  bool hudVisible_ = true;
  bool mouseCaptured_ = true;
  float lastCamDepth_ = 0.0f;
};
