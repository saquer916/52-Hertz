#include "ocean.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace {

const char* kOceanVS = R"GLSL(
layout(location = 0) in vec2 aGrid;
uniform vec2 uCenter;
out vec3 vWorld;
out vec3 vNormal;
out float vFoam;
out float vDist;

void main() {
  vec2 xz = uCenter + aGrid;
  float r = length(aGrid);
  vec3 p = vec3(xz.x, 0.0, xz.y);
  vec3 dPdx = vec3(1.0, 0.0, 0.0), dPdz = vec3(0.0, 0.0, 1.0);
  float jxx = 1.0, jzz = 1.0, jxz = 0.0;
  for (int i = 0; i < 7; i++) {
    vec2 D = uWaveA[i].xy;
    float k = uWaveA[i].z;
    float wavelength = 6.2831853 / k;
    float A = uWaveA[i].w * (1.0 - smoothstep(wavelength * 4.0, wavelength * 9.0, r));
    float Q = uWaveB[i].x;
    float th = k * dot(D, xz) - uWaveB[i].y * TIME + uWaveB[i].z;
    float c = cos(th), s = sin(th);
    p.x += Q * A * D.x * c;
    p.z += Q * A * D.y * c;
    p.y += A * s;
    float wa = k * A;
    dPdx += vec3(-Q * D.x * D.x * wa * s, D.x * wa * c, -Q * D.x * D.y * wa * s);
    dPdz += vec3(-Q * D.x * D.y * wa * s, D.y * wa * c, -Q * D.y * D.y * wa * s);
  }
  vNormal = normalize(cross(dPdz, dPdx));
  float jac = dPdx.x * dPdz.z - dPdx.z * dPdz.x;
  vFoam = clamp((0.86 - jac) * 4.0, 0.0, 1.0) + clamp(p.y - 0.75, 0.0, 1.0) * 0.6;
  vWorld = p;
  vDist = r;
  gl_Position = uViewProj * vec4(p, 1.0);
}
)GLSL";

const char* kOceanFS = R"GLSL(
in vec3 vWorld;
in vec3 vNormal;
in float vFoam;
in float vDist;
uniform vec4 uSplash[4];
layout(binding = 5) uniform sampler2D uReflection;
uniform vec4 uWalls[48];
uniform int uWallCount;
out vec4 fragColor;

float ggx(float NdotH, float a) {
  float a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d);
}

void main() {
  vec3 p = vWorld;
  float dist = length(uCamPos.xyz - p);
  vec2 t = vec2(TIME);
  vec2 g1 = texture(uNoise, p.xz / 23.0 + t * vec2(0.011, 0.004)).gb - 0.5;
  vec2 g2 = texture(uNoise, p.xz / 9.0 + t * vec2(-0.006, 0.017)).gb - 0.5;
  vec2 g3 = texture(uNoise, p.xz / 3.3 + t * vec2(0.031, -0.022)).gb - 0.5;
  float detail = 1.0 - smoothstep(30.0, 400.0, dist);
  vec2 slope = (g1 * 0.5 + g2 * 0.45 + g3 * 0.35 * (1.0 - smoothstep(10.0, 60.0, dist))) * detail;
  vec3 N = normalize(vNormal + vec3(slope.x, 0.0, slope.y) * 0.9);
  vec3 V = normalize(uCamPos.xyz - p);
  vec3 L = uSunDir.xyz;
  float shadow = sunShadow(p, vec3(0.0));

  float lace = texture(uNoise, p.xz / 4.0 + t * 0.01).a;
  float lace2 = texture(uNoise, p.xz / 1.7 - t * 0.013).r;
  float foam = smoothstep(0.35, 1.0, vFoam * (0.6 + lace)) * 0.85;
  for (int i = 0; i < 4; i++) {
    float age = TIME - uSplash[i].z;
    if (age < 0.0 || age > 9.0 || uSplash[i].w <= 0.0) continue;
    float R = (3.0 + 6.0 * sqrt(age)) * (0.7 + 0.5 * uSplash[i].w);
    float r = length(p.xz - uSplash[i].xy);
    float spot = (1.0 - smoothstep(R * 0.55, R, r)) * exp(-age * 0.45) * min(uSplash[i].w, 1.0);
    float holes = smoothstep(0.42, 0.62, lace2 + (lace - 0.5) * 0.6 + age * 0.03);
    foam = max(foam, spot * (1.0 - holes * 0.9));
  }
  if (dist < 400.0) {
    float shore = 1e9;
    for (int i = 0; i < uWallCount; i++) {
      vec2 lo = uWalls[i].xy, hi = uWalls[i].zw;
      vec2 d = max(lo - p.xz, p.xz - hi);
      shore = min(shore, length(max(d, 0.0)) + min(max(d.x, d.y), 0.0));
    }
    float surge = 0.5 + 0.5 * sin(TIME * 1.3 + p.x * 0.11 + p.z * 0.07);
    float fine = texture(uNoise, p.xz / 0.8 + t * vec2(0.02, -0.015)).r;
    float edge = shore + 1.6 * (lace - 0.5) + 0.8 * (lace2 - 0.5);
    float band = 1.0 - smoothstep(-0.6, 0.6 + 2.0 * surge, edge);
    float bubbles = smoothstep(0.46, 0.62, fine * 0.6 + lace2 * 0.4 + 0.1 * band);
    foam = max(foam, band * bubbles * 0.85 * (1.0 - smoothstep(150.0, 400.0, dist)));
  }
  foam = clamp(foam, 0.0, 1.0);

  if (uWater.w < 0.5) {
    if (dot(N, V) < 0.0) N = normalize(N - V * dot(N, V) * 1.05);
    float NdotV = max(dot(N, V), 1e-3);
    float F = 0.02 + 0.98 * pow(1.0 - NdotV, 5.0);
    vec2 suv = gl_FragCoord.xy * uScreen.zw;
    suv += (N.xz - vNormal.xz * 0.5) * vec2(0.06, 0.1) / (1.0 + dist * 0.02);
    vec3 refl = texture(uReflection, clamp(suv, vec2(0.001), vec2(0.999))).rgb;
    float rough = mix(0.035, 0.16, smoothstep(20.0, 800.0, dist));
    vec3 H = normalize(L + V);
    float spec = ggx(max(dot(N, H), 0.0), rough) * F * max(dot(N, L), 0.0) / (4.0 * NdotV);
    vec3 sunLit = uSunColor.rgb * shadow;
    vec3 body = vec3(0.003, 0.026, 0.042) * (0.4 + 0.6 * uSunColor.w);
    float sss = pow(max(dot(V, -L) * 0.5 + 0.5, 0.0), 3.0) * clamp(p.y * 0.8 + 0.4, 0.0, 1.0);
    body += vec3(0.02, 0.16, 0.13) * sss * (0.3 + 0.7 * shadow) * max(dot(N, L) * 0.5 + 0.5, 0.0);
    vec3 foamCol = sunLit * max(dot(N, L), 0.0) * 0.6 + vec3(0.5, 0.56, 0.62) * uSunColor.w;
    vec3 col = refl * F + spec * sunLit * 0.5;
    float through = (1.0 - F) * 0.42;
    col += body * (1.0 - F) * (1.0 - through * 0.6);
    col = mix(col, foamCol, foam);
    float alpha = 1.0 - through * (1.0 - foam);
    col = applyFog(col, p);
    fragColor = vec4(col * 1.0, alpha);
  } else {
    vec3 Nd = -N;
    if (dot(Nd, V) < 0.0) Nd = normalize(Nd - V * dot(Nd, V) * 1.05);
    vec3 I = -V;
    vec3 T = refract(I, -Nd, 1.33);
    vec3 col;
    float tilt = clamp(dot(Nd, normalize(vec3(-V.x, 0.0, -V.z) + vec3(0.0, -2.0, 0.0))) * 4.0 - 3.6, -1.0, 1.0);
    vec3 inside = waterScatter(1.0) * (1.4 + 0.9 * tilt + 0.6 * foam);
    if (dot(T, T) < 1e-4) {
      col = inside;
    } else {
      float cosi = max(dot(-I, Nd), 0.0);
      float F = 0.02 + 0.98 * pow(1.0 - cosi, 5.0);
      vec3 sky = skyBase(normalize(T)) * 1.3 + vec3(1.0, 0.85, 0.65) * 30.0 * pow(max(dot(normalize(T), uSunDir.xyz), 0.0), 900.0);
      col = mix(sky * vec3(0.55, 0.85, 0.9), inside, clamp(F * 1.5, 0.0, 1.0));
      col = mix(col, inside * 2.0, smoothstep(0.35, 0.0, dot(T, vec3(0, 1, 0))) * 0.5);
    }
    col += foam * vec3(0.25, 0.35, 0.38) * uSunColor.w;
    fragColor = vec4(applyFog(col, p), 1.0);
  }
}
)GLSL";

}

void Ocean::init() {
  const float spec[kWaveCount][3] = {{112, 0.55f, 15},  {71, 0.38f, -22}, {44, 0.24f, 38}, {27, 0.13f, -48},
                                     {17, 0.075f, 72}, {11, 0.045f, -6}, {7, 0.028f, 118}};
  for (int i = 0; i < kWaveCount; ++i) {
    Wave& w = waves_[i];
    float a = glm::radians(spec[i][2] - 90.0f);
    w.dir = glm::vec2(std::cos(a), std::sin(a));
    w.k = 6.2831853f / spec[i][0];
    w.amp = spec[i][1];
    w.steep = 0.11f / (w.k * w.amp);
    w.omega = std::sqrt(9.81f * w.k);
    w.phase = i * 1.7f;
  }

  const int rings = 150, segments = 192;
  const float b = 0.0445f, a = 0.34f / b;
  std::vector<glm::vec2> verts;
  verts.push_back({0, 0});
  for (int i = 1; i <= rings; ++i) {
    float r = a * (std::exp(b * i) - 1.0f);
    if (i == rings) r = 20000.0f;
    for (int s = 0; s < segments; ++s) {
      float ang = 6.2831853f * (s + 0.5f * (i & 1)) / segments;
      verts.push_back({r * std::cos(ang), r * std::sin(ang)});
    }
  }
  std::vector<uint32_t> idx;
  for (int s = 0; s < segments; ++s) idx.insert(idx.end(), {0u, (uint32_t)(1 + (s + 1) % segments), (uint32_t)(1 + s)});
  for (int i = 1; i < rings; ++i) {
    uint32_t r0 = 1 + (i - 1) * segments, r1 = 1 + i * segments;
    for (int s = 0; s < segments; ++s) {
      uint32_t s1 = (s + 1) % segments;
      idx.insert(idx.end(), {r0 + s, r0 + s1, r1 + s, r0 + s1, r1 + s1, r1 + s});
    }
  }
  indexCount_ = (int)idx.size();
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec2), verts.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &ibo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);
  glBindVertexArray(0);
  program_ = compileProgram("ocean", kOceanVS, kOceanFS);
}

void Ocean::fillWaves(FrameData& f) const {
  for (int i = 0; i < kWaveCount; ++i) {
    const Wave& w = waves_[i];
    f.waveA[i] = glm::vec4(w.dir, w.k, w.amp);
    f.waveB[i] = glm::vec4(w.steep, w.omega, w.phase, 0.0f);
  }
}

glm::vec3 Ocean::displace(glm::vec2 xz, float t) const {
  glm::vec3 d(0.0f);
  for (const Wave& w : waves_) {
    float th = w.k * glm::dot(w.dir, xz) - w.omega * t + w.phase;
    float c = std::cos(th), s = std::sin(th);
    d.x += w.steep * w.amp * w.dir.x * c;
    d.z += w.steep * w.amp * w.dir.y * c;
    d.y += w.amp * s;
  }
  return d;
}

float Ocean::heightAt(float x, float z, float t) const {
  glm::vec2 target(x, z), p = target;
  glm::vec3 d(0.0f);
  for (int i = 0; i < 4; ++i) {
    d = displace(p, t);
    p = target - glm::vec2(d.x, d.z);
  }
  return d.y;
}

void Ocean::addSplash(glm::vec2 xz, float time, float strength) {
  splashes_[nextSplash_] = glm::vec4(xz, time, strength);
  nextSplash_ = (nextSplash_ + 1) % 4;
}

void Ocean::setWalls(const std::vector<glm::vec4>& footprints) {
  walls_.assign(footprints.begin(), footprints.begin() + std::min<size_t>(footprints.size(), 48));
}

void Ocean::draw(const glm::vec3& cam) const {
  glUseProgram(program_);
  glUniform1i(glGetUniformLocation(program_, "uWallCount"), (int)walls_.size());
  if (!walls_.empty())
    glUniform4fv(glGetUniformLocation(program_, "uWalls"), (GLsizei)walls_.size(), glm::value_ptr(walls_[0]));
  glUniform2f(glGetUniformLocation(program_, "uCenter"), cam.x, cam.z);
  glUniform4fv(glGetUniformLocation(program_, "uSplash"), 4, glm::value_ptr(splashes_[0]));
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}
