
#include <algorithm>
#include <cmath>

#include "game.h"

namespace {
constexpr float kBreathSeconds = 40.0f;
constexpr float kRefillSeconds = 2.5f;
constexpr float kBreathLine = -3.0f;
}

void Game::updateEchoInput(float dt, const InputState& in) {
  if (in.echo && !echoHeld_) {
    holdTime_ = 0;
  }
  else if (in.echo) {
    holdTime_ += dt;
  }
  else if (!in.echo && echoHeld_) {
    breath_ -= 0.03f + 0.07f * std::clamp(holdTime_ / 1.2f, 0.0f, 1.0f);
    castEcho(holdTime_);
  }
  echoHeld_  = in.echo;
}

Echo::Call Echo::callFor(float holdSeconds) {
  float strength = std::clamp(holdSeconds / 1.2f, 0.0f, 1.0f);
  float reach = 30.0f + (160.0f - 30.0f) * strength;
  int rays = (int)(250.0f + (1200.0f - 250.f) * strength);
  float cone = (180.0f + (30.0f - 180.0f) * strength);
  return {strength, reach, rays, cone};
}

int Echo::cast(const Level& level, const Call& call, glm::vec3 origin, const glm::mat3& basis, float now,
               const std::vector<glm::vec4>& targets, int* targetHits) {
  if (targetHits) *targetHits = 0;
  std::uniform_real_distribution<float> random01(0.0f, 1.0f);
  std::vector<Dot> dots;
  for (int i = 0; i < call.rays; ++i) {
    float c = 1 + (cos(glm::radians(call.cone)) - 1) * random01(rng);
    float s = sqrt(1 - c * c);
    float spin = random01(rng) * 6.2831853f;
    glm::vec3 dir = basis * glm::vec3(s * cos(spin), s * sin(spin), -c);
      RayHit hit;
  if (level.raycast(origin, dir, call.reach, hit))
  dots.push_back({glm::vec4(hit.pos + hit.normal * 0.06f, now + hit.t / 60.0f), 0.0f});
  }

  addDots(dots);
  return (int)dots.size();
}

void Game::updateBreath(float dt) {
  if (whale_.pos.y > kBreathLine) {
    lastBreathPos_ = whale_.pos;
    lastBreathYaw_ = whale_.yaw;
    lastBreathPos_.y = std::min(lastBreathPos_.y, -2.0f);
    breath_ += dt/kRefillSeconds;
    if (breath_ > 1) breath_ = 1;
  }
  else {
    breath_ -= dt / kBreathSeconds * std::max(1.0f, whale_.speed / Whale::kCruise);
    if (breath_ <= 0) startBounceBack();
  }
}

void Game::updateVelocity(Whale& w, float dt) {
  if (w.underwater) w.vel = w.forward() * w.speed;
  else w.vel.y -= 9.8f * dt;
}

void Game::checkWin() {
  if (glm::distance(whale_.pos, level_.goal) <= 26) winGame();
}
