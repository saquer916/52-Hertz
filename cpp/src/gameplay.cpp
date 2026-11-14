// Gameplay for 52 Hertz: echolocation, breathing, breaching and winning.
// The engine (rendering, ocean, whale model, maze, effects) calls these.
//
// Handy pieces you can use in here:
//   whale_.pos, whale_.vel        position and velocity (glm::vec3, metres and m/s)
//   whale_.speed, whale_.forward() current swim speed, and the direction the nose points
//   whale_.underwater, whale_.yaw  true below y = 0; which way it faces (radians)
//   whale_.head()                 point at the whale's nose (where echoes start)
//   time_                         seconds since the game started
//   holdTime_, echoHeld_          for timing how long Space is held
//   breath_ (0..1), lastBreathPos_, lastBreathYaw_
//   level_.goal                   where the pod is
//   level_.raycast(from, dir, maxDist, hit)  -> true if it hit; hit.t, hit.pos, hit.normal
//   castEcho(holdSeconds)         sends a call (it calls your Echo::callFor and Echo::cast)
//   startBounceBack()             out of breath: fade out, move to lastBreathPos_, fade in
//   winGame()                     shows the ending
//   message("text", seconds)      shows a line of text on screen

#include <algorithm>
#include <cmath>

#include "game.h"

namespace {
constexpr float kBreathSeconds = 50.0f;  // a full breath lasts this long under water
constexpr float kRefillSeconds = 2.5f;   // empty to full at the surface
constexpr float kBreathLine = -3.0f;     // you can breathe above this depth
}  // namespace

// ---------------------------------------------------------------------------
// 1. Space: tap for a short click, hold up to 1.2 s for a long call.
//    (Same idea as your Godot _process.)
// ---------------------------------------------------------------------------
void Game::updateEchoInput(float dt, const InputState& in) {
  // in.echo is true while Space is held. echoHeld_ is what it was last frame.
  // - just pressed  (in.echo && !echoHeld_): reset holdTime_ to 0
  // - being held    (in.echo):              add dt to holdTime_
  // - just released (!in.echo && echoHeld_): castEcho(holdTime_)
  // Then remember this frame for next time: echoHeld_ = in.echo;
}

// ---------------------------------------------------------------------------
// 2. How strong a call is, from how long Space was held.
// ---------------------------------------------------------------------------
Echo::Call Echo::callFor(float holdSeconds) {
  // strength = clamp(holdSeconds / 1.2, 0, 1)
  // reach:  30 m for a tap  -> 160 m for a full call
  // rays:   250             -> 1200
  // cone:   180 degrees (every direction) -> 30 degrees (a beam straight ahead)
  // Tip: a + (b - a) * strength goes from a to b as strength goes 0 -> 1.
  return {0.0f, 30.0f, 0, 180.0f};  // {strength, reach, rays, cone}
}

// ---------------------------------------------------------------------------
// 3. Send the rays out and turn every hit into a dot.
// ---------------------------------------------------------------------------
int Echo::cast(const Level& level, const Call& call, glm::vec3 origin, const glm::mat3& basis, float now,
               const std::vector<glm::vec4>& targets, int* targetHits) {
  if (targetHits) *targetHits = 0;
  std::uniform_real_distribution<float> random01(0.0f, 1.0f);  // random01(rng) -> 0..1
  std::vector<Dot> dots;

  // For each of call.rays rays:
  //   a. Pick a random direction inside the cone, then turn it to face where the whale faces:
  //        float c = 1 + (cos(radians(call.cone)) - 1) * random01(rng);
  //        float s = sqrt(1 - c * c);
  //        float spin = random01(rng) * 6.2831853f;
  //        glm::vec3 dir = basis * glm::vec3(s * cos(spin), s * sin(spin), -c);   // -Z is forward
  //   b. RayHit hit; if (level.raycast(origin, dir, call.reach, hit)) { ... }
  //   c. For a hit, add a dot. It should appear after the echo travels there and back:
  //        dots.push_back({glm::vec4(hit.pos + hit.normal * 0.06f, now + hit.t / 60.0f), 0.0f});
  //   d. Bonus: rays that hit one of `targets` (the pod, spheres: xyz centre, w radius)
  //      make a gold dot (kind 1.0f) and count toward *targetHits. That's how the pod "answers".

  addDots(dots);  // hands the dots to the engine to draw
  return (int)dots.size();
}

// ---------------------------------------------------------------------------
// 4. Breathing: drains under water, refills near the surface.
// ---------------------------------------------------------------------------
void Game::updateBreath(float dt) {
  // If whale_.pos.y > kBreathLine (near the surface):
  //   - breath_ goes up by dt / kRefillSeconds (but not past 1)
  //   - remember where you breathed: lastBreathPos_ = whale_.pos; lastBreathYaw_ = whale_.yaw;
  //     (Tip: keep the y no higher than -2 so you don't bounce back into the air.)
  // Otherwise:
  //   - breath_ goes down by dt / kBreathSeconds
  //   - if breath_ <= 0: startBounceBack();
}

// ---------------------------------------------------------------------------
// 5. Swimming vs. flying: under water the whale goes where its nose points;
//    out of the water (a breach) it keeps its speed and gravity pulls it down.
// ---------------------------------------------------------------------------
void Game::updateVelocity(Whale& w, float dt) {
  // If w.underwater: w.vel = w.forward() * w.speed;
  // Otherwise:       gravity: w.vel.y -= 9.8f * dt;
  w.vel = w.forward() * w.speed;  // placeholder so the whale swims until you write this
}

// ---------------------------------------------------------------------------
// 6. Win: reach the pod.
// ---------------------------------------------------------------------------
void Game::checkWin() {
  // If the whale is within about 26 m of level_.goal: winGame();
  // glm::distance(a, b) gives the distance between two points.
}
