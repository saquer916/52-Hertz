#include "hud.h"

#include <cmath>

#include "render_util.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_easy_font.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

const char* kHudVS = R"GLSL(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aLocal;
layout(location = 3) in vec4 aShape;
layout(location = 4) in float aKind;
uniform vec2 uSize;
out vec4 vColor;
out vec2 vLocal;
flat out vec4 vShape;
flat out float vKind;
void main() {
  gl_Position = vec4(aPos / uSize * 2.0 - 1.0, 0.0, 1.0);
  gl_Position.y = -gl_Position.y;
  vColor = aColor;
  vLocal = aLocal;
  vShape = aShape;
  vKind = aKind;
}
)GLSL";

const char* kHudFS = R"GLSL(
in vec4 vColor;
in vec2 vLocal;
flat in vec4 vShape;
flat in float vKind;
out vec4 fragColor;
void main() {
  float d;
  if (vKind < 0.5) {
    vec2 q = abs(vLocal) - vShape.xy + vShape.z;
    d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - vShape.z;
  } else {
    float r = length(vLocal);
    d = abs(r - vShape.x) - vShape.y * 0.5;
    float ang = atan(vLocal.x, -vLocal.y);
    if (ang < 0.0) ang += 6.2831853;
    if (ang < vShape.z || ang > vShape.w) d = max(d, 1.0);
  }
  float a = clamp(0.5 - d, 0.0, 1.0) * vColor.a;
  if (a <= 0.0) discard;
  fragColor = vec4(vColor.rgb * a, a);
}
)GLSL";

}

void Hud::init() {
  program_ = compileProgram("hud", kHudVS, kHudFS);
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  auto attr = [](int loc, int n, size_t off) {
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, n, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)off);
  };
  attr(0, 2, offsetof(Vertex, pos));
  attr(1, 4, offsetof(Vertex, color));
  attr(2, 2, offsetof(Vertex, local));
  attr(3, 4, offsetof(Vertex, shape));
  attr(4, 1, offsetof(Vertex, kind));
  glBindVertexArray(0);
}

void Hud::begin(int w, int h) {
  width_ = w;
  height_ = h;
  verts_.clear();
}

void Hud::quad(glm::vec2 a, glm::vec2 b, glm::vec4 color, glm::vec2 center, glm::vec4 shape, float kind) {
  Vertex v[4] = {{{a.x, a.y}, color, glm::vec2(a.x, a.y) - center, shape, kind},
                 {{b.x, a.y}, color, glm::vec2(b.x, a.y) - center, shape, kind},
                 {{b.x, b.y}, color, glm::vec2(b.x, b.y) - center, shape, kind},
                 {{a.x, b.y}, color, glm::vec2(a.x, b.y) - center, shape, kind}};
  verts_.insert(verts_.end(), {v[0], v[1], v[2], v[0], v[2], v[3]});
}

void Hud::rect(glm::vec2 pos, glm::vec2 size, glm::vec4 color, float radius) {
  glm::vec2 c = pos + size * 0.5f;
  quad(pos - 1.0f, pos + size + 1.0f, color, c, glm::vec4(size * 0.5f, radius, 0.0f), 0.0f);
}

void Hud::ring(glm::vec2 c, float radius, float thickness, float a0, float a1, glm::vec4 color) {
  float r = radius + thickness;
  quad(c - r, c + r, color, c, glm::vec4(radius, thickness, a0, a1), 1.0f);
}

float Hud::textWidth(const std::string& s, float scale) const {
  return stb_easy_font_width(const_cast<char*>(s.c_str())) * scale;
}

void Hud::text(glm::vec2 pos, const std::string& s, float scale, glm::vec4 color, Align align, bool shadow) {
  float w = textWidth(s, scale);
  if (align == Center) pos.x -= w * 0.5f;
  if (align == Right) pos.x -= w;
  static char buffer[200000];
  int quads = stb_easy_font_print(0, 0, const_cast<char*>(s.c_str()), nullptr, buffer, sizeof buffer);
  const float* f = reinterpret_cast<const float*>(buffer);
  for (int pass = shadow ? 0 : 1; pass < 2; ++pass) {
    glm::vec2 off = pass == 0 ? glm::vec2(std::max(1.0f, scale * 0.6f)) : glm::vec2(0.0f);
    glm::vec4 col = pass == 0 ? glm::vec4(0, 0, 0, color.a * 0.6f) : color;
    for (int q = 0; q < quads; ++q) {
      const float* v0 = f + q * 16;
      const float* v2 = f + q * 16 + 8;
      glm::vec2 a = pos + off + glm::vec2(v0[0], v0[1]) * scale;
      glm::vec2 b = pos + off + glm::vec2(v2[0], v2[1]) * scale;
      glm::vec2 c = (a + b) * 0.5f;
      quad(a, b, col, c, glm::vec4((b - a) * 0.5f + 0.5f, 0.0f, 0.0f), 0.0f);
    }
  }
}

void Hud::end() {
  if (verts_.empty()) return;
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(Vertex), verts_.data(), GL_STREAM_DRAW);
  glUseProgram(program_);
  glUniform2f(glGetUniformLocation(program_, "uSize"), (float)width_, (float)height_);
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts_.size());
}
