#include "echo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "level.h"
#include "render_util.h"

namespace {

const char* kDotVS = R"GLSL(
layout(location = 0) in vec4 aDot;   // xyz, time it starts to appear
layout(location = 1) in float aKind;
out vec2 vUV;
out vec4 vColor;
void main() {
  const vec2 corners[4] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
  vec2 corner = corners[gl_VertexID];
  float age = TIME - aDot.w;
  // Fade in over 0.8 s once the echo is back, then out over 5 s (as in Godot).
  float a = clamp(age / 0.8, 0.0, 1.0) * (1.0 - clamp((age - 0.8) / 5.0, 0.0, 1.0));
  vec4 clip = uViewProj * vec4(aDot.xyz, 1.0);
  float radius = 0.16;
  float px = uProj[1][1] * radius / max(clip.w, 1e-3) * uScreen.y * 0.5;
  // Keep far dots at least ~1.3 px wide, but dimmer, so they don't flicker.
  float grow = max(px, 1.3) / max(px, 1e-4);
  a *= sqrt(clamp(px / 1.3, 0.25, 1.0));
  if (a <= 0.001 || age < 0.0) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    return;
  }
  clip.xy += corner * radius * grow * vec2(uProj[0][0], uProj[1][1]);
  gl_Position = clip;
  vUV = corner;
  // Lighter gray the closer the whale is.
  float gray = mix(0.92, 0.2, smoothstep(8.0, 90.0, distance(aDot.xyz, uWhaleHead.xyz)));
  vec3 col = aKind > 0.5 ? vec3(1.0, 0.78, 0.42) * (0.6 + gray) * 1.6 : vec3(gray) * 1.25;
  vColor = vec4(col, a);
}
)GLSL";

const char* kDotFS = R"GLSL(
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
void main() {
  float d = length(vUV);
  float a = (1.0 - smoothstep(0.55, 1.0, d)) * vColor.a;
  if (a <= 0.002) discard;
  fragColor = vec4(vColor.rgb * a, a);
}
)GLSL";

}  // namespace

void Echo::init() {
  program_ = compileProgram("echo dots", kDotVS, kDotFS);
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, kCapacity * sizeof(Dot), nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Dot), (void*)offsetof(Dot, posAppear));
  glVertexAttribDivisor(0, 1);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Dot), (void*)offsetof(Dot, kind));
  glVertexAttribDivisor(1, 1);
  glBindVertexArray(0);
  clear();
}

void Echo::clear() {
  // Mark every slot as long gone.
  std::vector<Dot> empty(kCapacity, Dot{glm::vec4(0, 0, 0, -1e6f), 0.0f});
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, kCapacity * sizeof(Dot), empty.data());
  head_ = 0;
  count_ = 0;
}

void Echo::addDots(const std::vector<Dot>& dots) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  size_t done = 0;
  while (done < dots.size()) {
    size_t n = std::min(dots.size() - done, (size_t)(kCapacity - head_));
    glBufferSubData(GL_ARRAY_BUFFER, head_ * sizeof(Dot), n * sizeof(Dot), dots.data() + done);
    head_ = (head_ + (int)n) % kCapacity;
    done += n;
  }
  count_ = std::min(kCapacity, count_ + (int)dots.size());
}

void Echo::draw() const {
  if (count_ == 0) return;
  glUseProgram(program_);
  glBindVertexArray(vao_);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count_);
}
