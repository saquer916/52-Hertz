#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

constexpr float kMaxPitch = glm::radians(70.0f);
constexpr float kMouseSensitivity = 0.0025f;
constexpr float kTurnResponsiveness = 3.0f;
constexpr float kBreathLine = -3.0f;
constexpr int kShadowSize = 2048;

const char* kSkyVS = R"GLSL(
out vec2 vNdc;
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
  vNdc = p;
  gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

const char* kSkyFS = R"GLSL(
in vec2 vNdc;
out vec4 fragColor;
void main() {
  vec4 w = uInvViewProj * vec4(vNdc, 1.0, 1.0);
  vec3 dir = normalize(w.xyz / w.w - uCamPos.xyz);
  vec3 col;
  if (uWater.w > 0.5) {
    col = applyFog(vec3(0.0), uCamPos.xyz + dir * 1.0e4);
  } else {
    col = skyColor(dir, uWater.w > -0.5);
    if (dir.y < 0.0) col = mix(col, waterScatter(3.0), smoothstep(0.0, -0.05, dir.y));
  }
  fragColor = vec4(col, 1.0);
}
)GLSL";

float lerpAngle(float a, float b, float t) {
  float d = std::fmod(b - a + 3.0f * 3.14159265f, 2.0f * 3.14159265f) - 3.14159265f;
  return a + d * t;
}

float angleDifference(float from, float to) {
  return std::fmod(to - from + 3.0f * 3.14159265f, 2.0f * 3.14159265f) - 3.14159265f;
}

float moveToward(float v, float target, float step) {
  if (std::fabs(target - v) <= step) return target;
  return v + (target > v ? step : -step);
}

glm::vec3 waterScatterCPU(float depth) {
  float k = std::clamp(depth / 25.0f, 0.0f, 1.0f);
  return glm::vec3(0.022f, 0.105f, 0.14f) * std::pow(1.0f - k, 2.6f);
}

}

glm::mat3 Whale::basis() const {
  glm::mat4 r = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0));
  r = glm::rotate(r, pitch, glm::vec3(1, 0, 0));
  return glm::mat3(r);
}

glm::mat4 Whale::modelMatrix() const {
  glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
  m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
  m = glm::rotate(m, pitch, glm::vec3(1, 0, 0));
  m = glm::rotate(m, bank, glm::vec3(0, 0, 1));
  m = glm::translate(m, glm::vec3(0, swim.heave(), 0));
  return m * WhaleModel::fileToWhale();
}

bool Game::init(int w, int h, unsigned seed) {
#ifndef HERTZ_GL41
  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
#endif
  initFrameBlock();
  initNoiseTexture();
  if (!whaleModel_.load(ASSET_DIR "/humpback_whale")) return false;
  level_.init();
  ocean_.init();
  echo_.init();
  effects_.init();
  hud_.init();
  post_.init();
  skyProg_ = compileProgram("sky", kSkyVS, kSkyFS);

  glGenTextures(1, &shadowTex_);
  glBindTexture(GL_TEXTURE_2D, shadowTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, kShadowSize, kShadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glGenFramebuffers(1, &shadowFbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  sunDir_ = glm::normalize(glm::vec3(0.42f, 0.27f, 0.87f));
  resize(w, h);
  newMaze(seed);
  message("Find the pod. Tap SPACE to click, hold it for a long call.", 9.0f);
  return true;
}

void Game::resize(int w, int h) {
  if (w == width_ && h == height_) return;
  width_ = std::max(w, 16);
  height_ = std::max(h, 16);
  scene_.create(width_, height_, GL_RGBA16F, true);
  reflection_.create(std::max(width_ / 2, 8), std::max(height_ / 2, 8), GL_RGBA16F, true);
  final_.create(width_, height_, GL_RGBA8, false);
  post_.resize(width_, height_);
}

void Game::newMaze(unsigned seed) {
  level_.generate(seed);
  ocean_.setWalls(level_.footprints());
  echo_.clear();
  whale_ = Whale{};
  whale_.pos = level_.start;
  whale_.yaw = whale_.targetYaw = level_.startYaw;
  whale_.vel = whale_.forward() * whale_.speed;
  breath_ = 1.0f;
  lastBreathPos_ = level_.start;
  lastBreathYaw_ = level_.startYaw;
  bounceState_ = 0;
  won_ = false;
  snapCamera();
}

void Game::message(const std::string& text, float seconds) {
  message_ = text;
  messageUntil_ = time_ + seconds;
}

void Game::update(float dt, const InputState& in) {
  time_ += dt;
  mouseCaptured_ = in.mouseCaptured;
  if (in.restart) {
    newMaze(level_.seedUsed + 1 + (unsigned)(time_ * 1000.0f));
    message("A new maze. Find the pod.", 4.0f);
  }

  if (in.mouseCaptured) {
    whale_.targetYaw -= in.mouseDx * kMouseSensitivity;
    whale_.targetPitch = std::clamp(whale_.targetPitch - in.mouseDy * kMouseSensitivity, -kMaxPitch, kMaxPitch);
  }

  updateEchoInput(dt, in);
  updateWhale(dt, in);
  if (bounceState_ == 0) updateBreath(dt);
  else updateBounceBack(dt);
  checkWin();

  if (podAnswerAt_ > 0.0f && time_ >= podAnswerAt_) {
    podAnswerAt_ = -1.0f;
    effects_.pulse(level_.goal, glm::vec3(0, 0, -1), 160.0f, 180.0f, time_, glm::vec3(1.0f, 0.75f, 0.4f) * 0.5f);
    effects_.pulse(level_.goal, glm::vec3(0, 0, -1), 160.0f, 180.0f, time_ + 0.35f, glm::vec3(1.0f, 0.75f, 0.4f) * 0.3f);
    if (!won_) message("Something out there answered your call...", 4.0f);
  }
  if (won_ && time_ >= nextPodPulse_) {
    effects_.pulse(level_.goal, glm::vec3(0, 0, -1), 140.0f, 180.0f, time_, glm::vec3(1.0f, 0.75f, 0.4f) * 0.35f);
    nextPodPulse_ = time_ + 1.6f;
  }

  updateCamera(dt);
  effects_.update(dt, time_);
}

void Game::startBounceBack() {
  breath_ = 0.0f;
  bounceState_ = 1;
  bounceTimer_ = 0.0f;
}

void Game::updateBounceBack(float dt) {
  bounceTimer_ += dt;
  if (bounceState_ == 1 && bounceTimer_ >= 0.9f) {
    whale_.pos = lastBreathPos_;
    whale_.yaw = whale_.targetYaw = lastBreathYaw_;
    whale_.pitch = whale_.targetPitch = 0.0f;
    whale_.speed = Whale::kCruise;
    whale_.vel = whale_.forward() * whale_.speed;
    whale_.underwater = true;
    breath_ = 1.0f;
    bounces_++;
    snapCamera();
    bounceState_ = 2;
    bounceTimer_ = 0.0f;
    message("Out of breath! You bounced back to where you last breathed.", 4.0f);
  } else if (bounceState_ == 2 && bounceTimer_ >= 0.9f) {
    bounceState_ = 0;
  }
}

void Game::winGame() {
  if (won_) return;
  won_ = true;
  wonAt_ = time_;
  nextPodPulse_ = time_;
  message("The pod answers. 52 Hertz is not alone anymore.", 1e9f);
}

void Game::updateWhale(float dt, const InputState& in) {
  Whale& w = whale_;
  float target = Whale::kCruise;
  if (in.boost) target = Whale::kBoost;
  else if (in.slow) target = Whale::kSlow;
  w.speed = moveToward(w.speed, target, 6.0f * dt);

  float t = 1.0f - std::exp(-kTurnResponsiveness * dt);
  float prevYaw = w.yaw;
  w.yaw = lerpAngle(w.yaw, w.targetYaw, t);
  if (w.underwater) {
    w.pitch = lerpAngle(w.pitch, w.targetPitch, t);
  } else {
    float flight = std::atan2(w.vel.y, glm::length(glm::vec2(w.vel.x, w.vel.z)));
    w.pitch = lerpAngle(w.pitch, flight, t);
    w.targetPitch = std::clamp(w.pitch, -kMaxPitch, kMaxPitch);
  }
  w.turnRate = dt > 0.0f ? angleDifference(prevYaw, w.yaw) / dt : 0.0f;
  w.bank += (w.turnRate * 0.35f - w.bank) * t;

  bool wasUnder = w.underwater;
  w.underwater = w.pos.y < 0.0f;
  if (w.underwater != wasUnder) {
    float vy = std::fabs(w.vel.y);
    if (vy >= 1.5f && time_ >= splashReadyAt_) {
      splashReadyAt_ = time_ + 0.6f;
      float strength = std::clamp(vy / 10.0f, 0.3f, 1.5f);
      glm::vec3 where(w.pos.x, 0.0f, w.pos.z);
      effects_.splash(where, strength);
      ocean_.addSplash(glm::vec2(where.x, where.z), time_, strength);
    }
  }

  updateVelocity(w, dt);

  w.pos += w.vel * dt;
  glm::mat3 b = w.basis();
  for (int iter = 0; iter < 3; ++iter) {
    bool any = false;
    for (float z : {-5.1f, -2.55f, 0.0f, 2.55f, 5.1f}) {
      glm::vec3 c = w.pos + b * glm::vec3(0, 0, z), n;
      glm::vec3 before = c;
      if (level_.resolveSphere(c, 1.4f, n)) {
        w.pos += c - before;
        any = true;
        if (glm::dot(n, n) > 0.0f) {
          n = glm::normalize(n);
          float into = glm::dot(w.vel, n);
          if (into < 0.0f) w.vel -= n * into;
        }
      }
    }
    if (!any) break;
  }

  w.swim.update(dt, w.speed, Whale::kBoost, w.turnRate);
}

void Game::castEcho(float hold) {
  Echo::Call call = Echo::callFor(hold);
  glm::vec3 origin = whale_.head();
  std::vector<glm::vec4> pod;
  for (int i = 0; i < 3; ++i) {
    glm::vec3 p;
    float yaw;
    podWhale(i, p, yaw);
    pod.push_back(glm::vec4(p, 3.0f));
  }
  int podHits = 0;
  echo_.cast(level_, call, origin, whale_.basis(), time_, pod, &podHits);
  if (podHits > 0 && podAnswerAt_ < time_) {
    podAnswerAt_ = time_ + glm::distance(origin, level_.goal) / 60.0f + 0.6f;
  }
  effects_.pulse(origin, whale_.forward(), std::min(call.reach, 60.0f + call.strength * 60.0f), call.cone, time_,
                 glm::vec3(0.35f, 0.6f, 0.7f) * (0.25f + 0.2f * call.strength));
}

void Game::podWhale(int i, glm::vec3& pos, float& yaw) const {
  float a = time_ * 0.11f + i * 2.094f;
  float r = 9.0f + 2.5f * i;
  pos = level_.goal + glm::vec3(std::cos(a) * r, std::sin(time_ * 0.3f + i * 1.3f) * 1.5f + (i - 1) * 3.0f,
                                std::sin(a) * r);
  yaw = 3.14159265f - a;
}

void Game::render() {
  glm::vec3 up(0, 1, 0);
  glm::mat4 view = glm::lookAt(camPos_, camAim_, up);
  float f = 1.0f / std::tan(glm::radians(75.0f) * 0.5f);
  float aspect = (float)width_ / height_;
  const float nearZ = 0.1f;
  glm::mat4 proj(0.0f);
  proj[0][0] = f / aspect;
  proj[1][1] = f;
  proj[2][3] = -1.0f;
  proj[3][2] = nearZ;

  float waveH = ocean_.heightAt(camPos_.x, camPos_.z, time_);
  float camDepth = waveH - camPos_.y;
  bool underwater = camDepth > (lastCamDepth_ > 0.0f ? -0.05f : 0.05f);
  lastCamDepth_ = underwater ? std::max(camDepth, 0.001f) : std::min(camDepth, -0.001f);
  float depthT = std::clamp(camDepth / 25.0f, 0.0f, 1.0f);
  float visibility = 140.0f * std::pow(1.5f / 140.0f, depthT);

  glm::vec3 center(0.0f, -15.0f, -75.0f);
  glm::mat4 lightView = glm::lookAt(center + sunDir_ * 500.0f, center, up);
  glm::mat4 lightProj = glm::ortho(-210.0f, 210.0f, -210.0f, 210.0f, 1.0f, 1100.0f);
  glm::mat4 lightVP = lightProj * lightView;
  glm::mat4 bias = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.0f)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 1.0f));

  glm::mat4 whaleM = whale_.modelMatrix();
  glm::vec3 whaleLight = glm::vec3(glm::translate(glm::mat4(1.0f), whale_.pos) *
                                   glm::mat4(whale_.basis()) * glm::vec4(0, 9, 8, 1));

  FrameData fd{};
  fd.view = view;
  fd.proj = proj;
  fd.viewProj = proj * view;
  fd.invViewProj = glm::inverse(fd.viewProj);
  fd.shadowMat = bias * lightVP;
  fd.camPos = glm::vec4(camPos_, time_);
  fd.sunDir = glm::vec4(sunDir_, 0.0f);
  fd.sunColor = glm::vec4(glm::vec3(1.0f, 0.84f, 0.66f) * 2.7f, 1.0f);
  fd.fogColor = glm::vec4(waterScatterCPU(std::max(camDepth, 0.0f)), visibility);
  fd.water = glm::vec4(camDepth, depthT, visibility * 0.4f * depthT, underwater ? 1.0f : 0.0f);
  fd.whaleLight = glm::vec4(whaleLight, 1.0f);
  fd.whaleHead = glm::vec4(whale_.head(), 0.0f);
  fd.screen = glm::vec4(width_, height_, 1.0f / width_, 1.0f / height_);
  fd.clipPlane = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  ocean_.fillWaves(fd);
  uploadFrameBlock(fd);

  WhaleDraw wd;
  wd.world = whaleM;
  whaleModel_.pose(whale_.swim, wd.joints);
  wd.fogAmount = 0.0f;
  wd.lightPos = whaleLight;
  wd.lightStrength = 1.0f;

  glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
  glViewport(0, 0, kShadowSize, kShadowSize);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glClearDepth(1.0);
  glClear(GL_DEPTH_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.0f, 4.0f);
  level_.drawShadow(lightVP);
  whaleModel_.drawShadow(wd, lightVP);
  glDisable(GL_POLYGON_OFFSET_FILL);

  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_2D, shadowTex_);
  glActiveTexture(GL_TEXTURE0);

  if (!underwater) {
    glm::mat4 flip = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    FrameData rd = fd;
    rd.view = view * flip;
    rd.viewProj = proj * rd.view;
    rd.invViewProj = glm::inverse(rd.viewProj);
    rd.camPos = glm::vec4(camPos_.x, -camPos_.y, camPos_.z, time_);
    rd.water.w = -1.0f;
    rd.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, 0.3f);
    uploadFrameBlock(rd);
    glBindFramebuffer(GL_FRAMEBUFFER, reflection_.fbo);
    glViewport(0, 0, reflection_.width, reflection_.height);
    glClearDepth(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_GREATER);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(skyProg_);
    drawFullscreenTriangle();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
#ifndef HERTZ_ES
    glEnable(GL_CLIP_DISTANCE0);
#endif
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    level_.draw();
    drawWhales(wd);
    glFrontFace(GL_CCW);
#ifndef HERTZ_ES
    glDisable(GL_CLIP_DISTANCE0);
#endif
    uploadFrameBlock(fd);
  }
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, reflection_.color);
  glActiveTexture(GL_TEXTURE0);

  glBindFramebuffer(GL_FRAMEBUFFER, scene_.fbo);
  glViewport(0, 0, width_, height_);
  glClearDepth(0.0);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDepthFunc(GL_GREATER);

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glUseProgram(skyProg_);
  drawFullscreenTriangle();
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  level_.draw();
  drawWhales(wd);

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  if (underwater) effects_.drawUnderwater();
  echo_.draw();
  effects_.drawPulses(time_);
  glDepthMask(GL_TRUE);
  ocean_.draw(camPos_);
  glDepthMask(GL_FALSE);
  effects_.drawParticles();
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  PostSettings ps;
  float fade = 0.0f;
  if (bounceState_ == 1) fade = std::min(1.0f, bounceTimer_ / 0.9f);
  if (bounceState_ == 2) fade = 1.0f - std::min(1.0f, bounceTimer_ / 0.9f);
  ps.fade = fade;
  ps.underwater = underwater ? 1.0f - depthT * 0.5f : 0.0f;
  post_.run(scene_, final_, ps);

  if (hudVisible_) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud();
    glDisable(GL_BLEND);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Game::drawWhales(const WhaleDraw& player) {
  glm::vec3 up(0, 1, 0);
  whaleModel_.draw(player);
  for (int i = 0; i < 3; ++i) {
    glm::vec3 p;
    float yaw;
    podWhale(i, p, yaw);
    WhaleDraw pd;
    SwimPose sp;
    sp.phase = std::fmod(time_ * 6.2831853f * 0.75f + i * 2.0f, 6.2831853f);
    sp.power = 0.45f;
    sp.turn = -0.15f;
    pd.world = glm::translate(glm::mat4(1.0f), p) * glm::rotate(glm::mat4(1.0f), yaw, up) *
               glm::rotate(glm::mat4(1.0f), 0.15f, glm::vec3(0, 0, 1)) *
               glm::scale(glm::mat4(1.0f), glm::vec3(0.72f)) * WhaleModel::fileToWhale();
    whaleModel_.pose(sp, pd.joints);
    pd.fogAmount = won_ ? 0.15f : 0.45f;
    float glow = won_ ? 0.5f + 0.25f * std::sin((time_ - wonAt_) * 4.0f) : 0.3f;
    pd.glow = glm::vec3(1.0f, 0.75f, 0.45f) * glow;
    pd.lightPos = p + glm::vec3(0, 8, 0);
    pd.lightStrength = 0.7f;
    whaleModel_.draw(pd);
  }

}

void Game::drawHud() {
  hud_.begin(width_, height_);
  float s = height_ / 720.0f;
  glm::vec2 c(width_ * 0.5f, height_ * 0.5f);

  float bw = 300.0f * s, bh = 12.0f * s;
  glm::vec2 bp(c.x - bw * 0.5f, height_ - 46.0f * s);
  float low = breath_ < 0.25f ? 0.5f + 0.5f * std::sin(time_ * 9.0f) : 0.0f;
  hud_.rect(bp - 3.0f * s, glm::vec2(bw, bh) + 6.0f * s, glm::vec4(0.0f, 0.02f, 0.04f, 0.55f), (bh + 6 * s) * 0.5f);
  glm::vec3 full(0.55f, 0.92f, 1.0f), empty(1.0f, 0.3f, 0.25f);
  glm::vec3 col = glm::mix(empty, full, std::clamp((breath_ - 0.1f) / 0.5f, 0.0f, 1.0f));
  if (breath_ > 0.001f)
    hud_.rect(bp, glm::vec2(std::max(bw * breath_, bh), bh), glm::vec4(col * (0.85f + 0.4f * low), 0.95f), bh * 0.5f);
  bool breathing = whale_.pos.y > kBreathLine && breath_ < 0.999f;
  std::string label = breathing ? "BREATHING..." : (breath_ < 0.25f ? "BREATH - SURFACE NOW!" : "BREATH");
  hud_.text(glm::vec2(c.x, bp.y - 20.0f * s), label, 2.0f * s, glm::vec4(0.85f, 0.95f, 1.0f, 0.9f), Hud::Center);

  char buf[128];
  float depth = std::max(0.0f, -whale_.pos.y);
  if (whale_.pos.y > 0.0f) std::snprintf(buf, sizeof buf, "AIRBORNE  %.1f m", whale_.pos.y);
  else std::snprintf(buf, sizeof buf, "DEPTH  %.0f m", depth);
  hud_.text(glm::vec2(22.0f * s, 22.0f * s), buf, 2.0f * s, glm::vec4(0.8f, 0.9f, 1.0f, 0.85f));
  std::snprintf(buf, sizeof buf, "SPEED  %.0f m/s", whale_.speed);
  hud_.text(glm::vec2(22.0f * s, 44.0f * s), buf, 2.0f * s, glm::vec4(0.8f, 0.9f, 1.0f, 0.6f));

  if (echoHeld_) {
    float k = std::clamp(holdTime_ / 1.2f, 0.0f, 1.0f);
    glm::vec2 rc(c.x + 190.0f * s, height_ - 40.0f * s);
    hud_.ring(rc, 16.0f * s, 3.0f * s, 0.0f, 6.2831853f, glm::vec4(0.6f, 0.85f, 1.0f, 0.18f));
    hud_.ring(rc, 16.0f * s, 3.0f * s, 0.0f, 6.2831853f * k, glm::vec4(0.6f, 0.9f, 1.0f, 0.9f));
    hud_.text(glm::vec2(rc.x + 26.0f * s, rc.y - 7.0f * s), k >= 1.0f ? "LONG CALL" : "CALL", 1.8f * s,
              glm::vec4(0.75f, 0.92f, 1.0f, 0.85f), Hud::Left);
  }

  if (time_ < 8.0f) {
    float a = std::clamp(8.0f - time_, 0.0f, 1.0f) * std::clamp(time_ * 1.5f, 0.0f, 1.0f);
    hud_.text(glm::vec2(c.x, height_ * 0.14f), "52 HERTZ", 7.0f * s, glm::vec4(0.9f, 0.96f, 1.0f, a), Hud::Center);
    hud_.text(glm::vec2(c.x, height_ * 0.14f + 64.0f * s), "a whale whose calls no other whale answers", 2.2f * s,
              glm::vec4(0.75f, 0.88f, 0.95f, a * 0.85f), Hud::Center);
  }
  if (time_ < messageUntil_ && !message_.empty()) {
    float a = std::clamp(messageUntil_ - time_, 0.0f, 1.0f);
    float y = height_ - 128.0f * s - (won_ ? 30.0f * s : 0.0f);
    hud_.text(glm::vec2(c.x, y), message_, 2.4f * s, glm::vec4(1.0f, 0.95f, 0.85f, a), Hud::Center);
    if (won_)
      hud_.text(glm::vec2(c.x, y + 30.0f * s), "Press R for a new maze", 1.8f * s,
                glm::vec4(0.9f, 0.9f, 0.9f, 0.8f), Hud::Center);
  }
  if (!mouseCaptured_)
    hud_.text(glm::vec2(c.x, height_ * 0.45f), "Click to swim", 3.0f * s, glm::vec4(1, 1, 1, 0.9f), Hud::Center);
  hud_.text(glm::vec2(width_ - 22.0f * s, 22.0f * s), "Mouse steer  W boost  S slow  Space echo  Esc mouse", 1.6f * s,
            glm::vec4(0.8f, 0.9f, 1.0f, 0.45f), Hud::Right);
  hud_.end();
}

void Game::snapCamera() {
  camPos_ = whale_.pos + whale_.basis() * glm::vec3(0, 3.5f, 12.0f);
  camAim_ = whale_.pos + whale_.forward() * 8.0f;
}

void Game::updateCamera(float dt) {
  glm::vec3 desired = whale_.pos + whale_.basis() * glm::vec3(0, 3.5f, 12.0f);
  glm::vec3 d = desired - whale_.pos;
  float len = glm::length(d);
  RayHit hit;
  if (len > 0.001f && level_.raycast(whale_.pos, d/len, len, hit)) desired = hit.pos + hit.normal * 0.5f;
  camPos_ += (desired - camPos_) * (1.0f - std::exp(-4.0f * dt));
  glm::vec3 n;
  level_.resolveSphere(camPos_, 0.4f, n);
  camAim_  = whale_.pos + whale_.forward() * 8.0f;
}

std::string Game::status() const {
  char buf[256];
  std::snprintf(buf, sizeof buf, "pos (%.1f, %.1f, %.1f) speed %.1f breath %.0f%% %s", whale_.pos.x, whale_.pos.y,
                whale_.pos.z, whale_.speed, breath_ * 100.0f, won_ ? "WON" : "");
  return buf;
}
