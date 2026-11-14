#include "effects.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/type_ptr.hpp>

#include "render_util.h"

namespace {

const char* kParticleVS = R"GLSL(
layout(location = 0) in vec4 aPosSize;
layout(location = 1) in vec4 aVelAlpha;
layout(location = 2) in float aKind;
out vec2 vUV;
out vec4 vColor;
flat out float vKindOut;
void main() {
  const vec2 corners[4] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
  vec2 c = corners[gl_VertexID];
  vec3 vp = (uView * vec4(aPosSize.xyz, 1.0)).xyz;
  float size = aPosSize.w;
  if (aKind < 0.5) {
    vec3 vv = mat3(uView) * aVelAlpha.xyz;
    vec2 axis = length(vv.xy) > 1e-3 ? normalize(vv.xy) : vec2(0.0, 1.0);
    vec2 side = vec2(-axis.y, axis.x);
    float len = size + length(vv.xy) * 0.045;
    vp.xy += side * c.x * size * 0.22 + axis * c.y * len;
  } else {
    vp.xy += c * size;
  }
  gl_Position = uProj * vec4(vp, 1.0);
  vUV = c;
  vKindOut = aKind;
  vec3 light = uSunColor.rgb * 0.4 + skyAmbient(vec3(0.0, 1.0, 0.0)) * 0.8;
  float a = aVelAlpha.w * (aKind > 1.5 ? 0.12 : (aKind > 0.5 ? 0.85 : 0.5));
  a *= smoothstep(-0.4, 0.2, aPosSize.y);
  a *= smoothstep(1.5, 6.0, -vp.z);
  vColor = vec4(light * (aKind > 1.5 ? 0.95 : 1.1), a);
}
)GLSL";

const char* kParticleFS = R"GLSL(
in vec2 vUV;
in vec4 vColor;
flat in float vKindOut;
out vec4 fragColor;
void main() {
  float d = length(vUV);
  float a = 1.0 - smoothstep(0.35, 1.0, d);
  if (vKindOut < 0.5) {
    a = (1.0 - smoothstep(0.0, 1.0, abs(vUV.x))) * (1.0 - smoothstep(0.2, 1.0, abs(vUV.y)));
  } else if (vKindOut > 1.5) {
    a = pow(1.0 - smoothstep(0.0, 1.0, d), 1.5);
    a *= 0.5 + texture(uNoise, vUV * 0.3 + gl_FragCoord.xy * 0.0007).r;
  }
  a *= vColor.a;
  if (a < 0.003) discard;
  fragColor = vec4(vColor.rgb * a, a);
}
)GLSL";

const char* kSnowVS = R"GLSL(
layout(location = 0) in vec4 aSeed;
out vec2 vUV;
out vec4 vColor;
void main() {
  const vec2 corners[4] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
  vec2 c = corners[gl_VertexID];
  const float box = 36.0;
  vec3 drift = vec3(sin(TIME * 0.1 + aSeed.x * 20.0) * 0.4, -0.06 * TIME, cos(TIME * 0.08 + aSeed.z * 17.0) * 0.4);
  vec3 rel = aSeed.xyz * box + drift - uCamPos.xyz;
  rel -= box * floor(rel / box + 0.5);
  vec3 p = uCamPos.xyz + rel;
  float d = length(rel);
  float a = (1.0 - smoothstep(9.0, 17.0, d)) * smoothstep(2.0, 4.0, d) * step(p.y, -0.3);
  vec4 clip = uViewProj * vec4(p, 1.0);
  float radius = 0.008 + 0.014 * aSeed.w;
  float px = uProj[1][1] * radius / max(clip.w, 1e-3) * uScreen.y * 0.5;
  float grow = max(px, 0.7) / max(px, 1e-4);
  a *= clamp(px / 0.7, 0.2, 1.0);
  if (a < 0.01) { gl_Position = vec4(2, 2, 2, 1); return; }
  clip.xy += c * radius * grow * vec2(uProj[0][0], uProj[1][1]);
  gl_Position = clip;
  vUV = c;
  float dl = distance(p, uWhaleLight.xyz);
  float lit = pow(clamp(1.0 - dl / 30.0, 0.0, 1.0), 2.0) * uWhaleLight.w;
  vec3 col = vec3(0.6, 0.75, 0.8) * lit * 0.5 + waterScatter(-p.y) * 2.5 + vec3(0.5, 0.7, 0.75) * sunReach(p.y) * 0.12;
  vColor = vec4(col, a * 0.55);
}
)GLSL";

const char* kSoftDotFS = R"GLSL(
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
void main() {
  float a = (1.0 - smoothstep(0.3, 1.0, length(vUV))) * vColor.a;
  if (a < 0.003) discard;
  fragColor = vec4(vColor.rgb * a, a);
}
)GLSL";

const char* kShaftVS = R"GLSL(
layout(location = 0) in vec4 aSeed;
out vec2 vUV;
out float vBright;
void main() {
  const vec2 corners[4] = vec2[](vec2(-1, 0), vec2(1, 0), vec2(-1, 1), vec2(1, 1));
  vec2 c = corners[gl_VertexID];
  const float tile = 120.0;
  vec2 rel = aSeed.xy * tile - uCamPos.xz;
  rel -= tile * floor(rel / tile + 0.5);
  vec3 top = vec3(uCamPos.x + rel.x, 0.0, uCamPos.z + rel.y);
  vec3 axis = refract(-uSunDir.xyz, vec3(0.0, 1.0, 0.0), 1.0 / 1.33);
  float len = 38.0;
  vec3 mid = top + axis * len * 0.5;
  vec3 side = normalize(cross(axis, uCamPos.xyz - mid));
  vec3 p = top + axis * len * c.y + side * c.x * aSeed.z;
  gl_Position = uViewProj * vec4(p, 1.0);
  vUV = c;
  float flicker = 0.55 + 0.45 * sin(TIME * (0.4 + aSeed.w * 0.5) + aSeed.w * 30.0);
  float camDepth = uWater.x;
  vec3 toCam = uCamPos.xyz - top;
  vec3 closest = top + axis * clamp(dot(toCam, axis), 0.0, len);
  float near = smoothstep(5.0, 14.0, distance(uCamPos.xyz, closest));
  float far = 1.0 - smoothstep(25.0, 60.0, distance(uCamPos.xz, top.xz));
  vBright = flicker * near * far * sunShadow(top + vec3(0.0, 0.5, 0.0), vec3(0.0)) *
            (1.0 - smoothstep(4.0, 22.0, camDepth)) * step(0.5, uWater.w);
}
)GLSL";

const char* kShaftFS = R"GLSL(
in vec2 vUV;
in float vBright;
out vec4 fragColor;
void main() {
  float across = 1.0 - vUV.x * vUV.x;
  float along = pow(1.0 - vUV.y, 1.6) * smoothstep(0.0, 0.05, vUV.y);
  float a = across * across * along * vBright;
  vec3 col = vec3(0.55, 0.85, 0.8) * uSunColor.rgb * a * 0.045;
  fragColor = vec4(col, 0.0);
}
)GLSL";

const char* kPulseVS = R"GLSL(
layout(location = 0) in vec3 aDir;
uniform vec3 uOrigin;
uniform float uRadius;
out vec3 vDir;
out vec3 vWorld;
void main() {
  vDir = aDir;
  vWorld = uOrigin + aDir * uRadius;
  gl_Position = uViewProj * vec4(vWorld, 1.0);
}
)GLSL";

const char* kPulseFS = R"GLSL(
in vec3 vDir;
in vec3 vWorld;
uniform vec3 uForward;
uniform float uCosCone;
uniform vec3 uColor;
uniform float uFade;
out vec4 fragColor;
void main() {
  vec3 V = normalize(uCamPos.xyz - vWorld);
  float rim = pow(1.0 - abs(dot(normalize(vDir), V)), 2.5);
  float cone = smoothstep(uCosCone - 0.06, uCosCone + 0.06, dot(normalize(vDir), uForward));
  float ripple = 0.75 + 0.25 * sin(dot(vDir, vec3(17.0, 23.0, 19.0)) + TIME * 3.0);
  float a = (rim * 0.85 + 0.08) * cone * uFade * ripple;
  fragColor = vec4(uColor * a, 0.0);
}
)GLSL";

}

void Effects::init() {
  std::uniform_real_distribution<float> uni(0.0f, 1.0f);

  particleProg_ = compileProgram("splash", kParticleVS, kParticleFS);
  glGenVertexArrays(1, &particleVao_);
  glBindVertexArray(particleVao_);
  glGenBuffers(1, &particleVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
  glBufferData(GL_ARRAY_BUFFER, 4096 * 9 * sizeof(float), nullptr, GL_STREAM_DRAW);
  for (int i = 0; i < 3; ++i) {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, i == 2 ? 1 : 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(i * 4 * sizeof(float)));
    glVertexAttribDivisor(i, 1);
  }

  snowProg_ = compileProgram("marine snow", kSnowVS, kSoftDotFS);
  std::vector<glm::vec4> seeds(kSnow);
  for (auto& s : seeds) s = glm::vec4(uni(rng_), uni(rng_), uni(rng_), uni(rng_));
  glGenVertexArrays(1, &snowVao_);
  glBindVertexArray(snowVao_);
  glGenBuffers(1, &snowVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, snowVbo_);
  glBufferData(GL_ARRAY_BUFFER, seeds.size() * sizeof(glm::vec4), seeds.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
  glVertexAttribDivisor(0, 1);

  shaftProg_ = compileProgram("light shafts", kShaftVS, kShaftFS);
  std::vector<glm::vec4> shafts(kShafts);
  for (auto& s : shafts) s = glm::vec4(uni(rng_), uni(rng_), 0.5f + 1.8f * uni(rng_), uni(rng_));
  glGenVertexArrays(1, &shaftVao_);
  glBindVertexArray(shaftVao_);
  glGenBuffers(1, &shaftVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, shaftVbo_);
  glBufferData(GL_ARRAY_BUFFER, shafts.size() * sizeof(glm::vec4), shafts.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
  glVertexAttribDivisor(0, 1);

  pulseProg_ = compileProgram("pulse", kPulseVS, kPulseFS);
  std::vector<glm::vec3> dirs;
  std::vector<uint32_t> idx;
  const int rings = 24, segs = 48;
  for (int r = 0; r <= rings; ++r)
    for (int s = 0; s <= segs; ++s) {
      float th = 3.14159265f * r / rings, ph = 6.2831853f * s / segs;
      dirs.push_back({std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)});
    }
  for (int r = 0; r < rings; ++r)
    for (int s = 0; s < segs; ++s) {
      uint32_t a = r * (segs + 1) + s, b = a + segs + 1;
      idx.insert(idx.end(), {a, b, a + 1, a + 1, b, b + 1});
    }
  pulseIndexCount_ = (int)idx.size();
  glGenVertexArrays(1, &pulseVao_);
  glBindVertexArray(pulseVao_);
  glGenBuffers(1, &pulseVbo_);
  glBindBuffer(GL_ARRAY_BUFFER, pulseVbo_);
  glBufferData(GL_ARRAY_BUFFER, dirs.size() * sizeof(glm::vec3), dirs.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &pulseIbo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pulseIbo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
  glBindVertexArray(0);
}

void Effects::splash(glm::vec3 where, float strength) {
  strength = std::clamp(strength, 0.25f, 1.5f);
  std::uniform_real_distribution<float> uni(0.0f, 1.0f);
  auto rnd = [&](float a, float b) { return a + (b - a) * uni(rng_); };
  float scale = 0.7f + 0.6f * strength / 1.5f;
  float amount = std::clamp(strength, 0.35f, 1.0f);
  for (int i = 0; i < (int)(170 * amount); ++i) {
    float a = rnd(0, 6.2831853f);
    glm::vec3 out(std::cos(a), 0.0f, std::sin(a));
    float r = rnd(1.2f, 3.2f) * scale;
    glm::vec3 v = out * rnd(2.0f, 6.0f) * scale + glm::vec3(0, rnd(6.0f, 13.0f) * (0.55f + 0.45f * strength), 0);
    float life = rnd(1.1f, 2.0f);
    particles_.push_back({where + out * r, v, life, life, rnd(0.12f, 0.3f) * scale, 0.0f, 0.0f, 1.0f});
  }
  for (int i = 0; i < (int)(220 * amount); ++i) {
    float a = rnd(0, 6.2831853f);
    glm::vec3 out(std::cos(a), 0.0f, std::sin(a));
    glm::vec3 v = out * rnd(1.0f, 7.5f) * scale + glm::vec3(0, rnd(3.0f, 14.0f) * (0.5f + 0.5f * strength), 0);
    float life = rnd(0.8f, 1.8f);
    particles_.push_back({where + out * rnd(0.5f, 3.0f) * scale, v, life, life, rnd(0.04f, 0.1f), 0.0f, 1.0f, 1.0f});
  }
  for (int i = 0; i < (int)(18 * amount); ++i) {
    float a = rnd(0, 6.2831853f);
    glm::vec3 out(std::cos(a), 0.0f, std::sin(a));
    glm::vec3 v = out * rnd(0.6f, 2.2f) + glm::vec3(0, rnd(0.8f, 3.5f), 0);
    float life = rnd(2.5f, 4.0f);
    particles_.push_back(
        {where + out * rnd(0.0f, 3.0f) + glm::vec3(0, rnd(0.5f, 3.0f), 0), v, life, life, rnd(1.0f, 2.0f) * scale,
         rnd(0.5f, 1.1f), 2.0f, 1.0f});
  }
}

void Effects::pulse(glm::vec3 origin, glm::vec3 forward, float reach, float coneDeg, float now, glm::vec3 color) {
  pulses_.push_back({origin, forward, color, reach, std::cos(glm::radians(coneDeg)), now});
}

void Effects::update(float dt, float now) {
  pulses_.erase(std::remove_if(pulses_.begin(), pulses_.end(),
                               [&](const Pulse& p) { return (now - p.start) * 70.0f > p.reach; }),
                pulses_.end());
  for (Particle& p : particles_) {
    p.life -= dt;
    bool mist = p.kind > 1.5f;
    float drag = mist ? 1.6f : 0.25f;
    p.vel *= std::exp(-drag * dt);
    p.vel.y -= (mist ? 0.6f : 9.8f) * dt;
    p.pos += p.vel * dt;
    p.size += p.growth * dt;
    float t = p.life / p.maxLife;
    p.alpha = mist ? std::min(1.0f, (1.0f - t) * 6.0f) * t : std::min(1.0f, t * 3.0f);
    if (!mist && p.pos.y < -0.3f && p.vel.y < 0.0f) p.life = 0.0f;
  }
  particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life <= 0; }),
                   particles_.end());
  if (particles_.size() > 4096) particles_.erase(particles_.begin(), particles_.begin() + (particles_.size() - 4096));
}

void Effects::drawUnderwater() const {
  glUseProgram(shaftProg_);
  glBindVertexArray(shaftVao_);
  glBlendFunc(GL_ONE, GL_ONE);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, kShafts);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(snowProg_);
  glBindVertexArray(snowVao_);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, kSnow);
}

void Effects::drawPulses(float now) const {
  glUseProgram(pulseProg_);
  glBindVertexArray(pulseVao_);
  glBlendFunc(GL_ONE, GL_ONE);
  for (const Pulse& p : pulses_) {
    float age = now - p.start;
    float radius = age * 70.0f;
    if (age < 0.0f || radius > p.reach) continue;
    float fade = (1.0f - radius / p.reach) * std::min(1.0f, age * 8.0f);
    glUniform3fv(glGetUniformLocation(pulseProg_, "uOrigin"), 1, glm::value_ptr(p.origin));
    glUniform1f(glGetUniformLocation(pulseProg_, "uRadius"), std::max(radius, 0.5f));
    glUniform3fv(glGetUniformLocation(pulseProg_, "uForward"), 1, glm::value_ptr(p.forward));
    glUniform1f(glGetUniformLocation(pulseProg_, "uCosCone"), p.cosCone);
    glUniform3fv(glGetUniformLocation(pulseProg_, "uColor"), 1, glm::value_ptr(p.color));
    glUniform1f(glGetUniformLocation(pulseProg_, "uFade"), fade);
    glDrawElements(GL_TRIANGLES, pulseIndexCount_, GL_UNSIGNED_INT, nullptr);
  }
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void Effects::drawParticles() const {
  if (particles_.empty()) return;
  std::vector<float> data;
  data.reserve(particles_.size() * 9);
  for (const Particle& p : particles_)
    data.insert(data.end(), {p.pos.x, p.pos.y, p.pos.z, p.size, p.vel.x, p.vel.y, p.vel.z, p.alpha, p.kind});
  glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
  glBufferData(GL_ARRAY_BUFFER, 4096 * 9 * sizeof(float), nullptr, GL_STREAM_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, data.size() * sizeof(float), data.data());
  glUseProgram(particleProg_);
  glBindVertexArray(particleVao_);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)particles_.size());
}
