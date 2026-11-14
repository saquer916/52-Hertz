#include "post.h"

#include <algorithm>

namespace {

const char* kFullscreenVS = R"GLSL(
out vec2 vUV;
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  vUV = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* kDownFS = R"GLSL(
in vec2 vUV;
layout(binding = 0) uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uPrefilter;
out vec4 fragColor;
vec3 s(vec2 o) { return texture(uSrc, vUV + o * uTexel).rgb; }
void main() {
  vec3 a = s(vec2(-2, -2)), b = s(vec2(0, -2)), c = s(vec2(2, -2));
  vec3 d = s(vec2(-1, -1)), e = s(vec2(1, -1));
  vec3 f = s(vec2(-2, 0)), g = s(vec2(0, 0)), h = s(vec2(2, 0));
  vec3 i = s(vec2(-1, 1)), j = s(vec2(1, 1));
  vec3 k = s(vec2(-2, 2)), l = s(vec2(0, 2)), m = s(vec2(2, 2));
  vec3 col = (d + e + i + j) * 0.125 + (a + b + g + f) * 0.03125 + (b + c + h + g) * 0.03125 +
             (f + g + l + k) * 0.03125 + (g + h + m + l) * 0.03125;
  if (uPrefilter > 0.5) {
    col = min(col, vec3(60.0));
    float br = max(col.r, max(col.g, col.b));
    float knee = 0.6;
    float soft = clamp(br - 0.9 + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    col *= max(soft, br - 0.9) / max(br, 1e-4);
  }
  fragColor = vec4(col, 1.0);
}
)GLSL";

const char* kUpFS = R"GLSL(
in vec2 vUV;
layout(binding = 0) uniform sampler2D uSrc;
uniform vec2 uTexel;
out vec4 fragColor;
void main() {
  vec3 c = vec3(0.0);
  c += texture(uSrc, vUV + vec2(-1, -1) * uTexel).rgb;
  c += texture(uSrc, vUV + vec2(0, -1) * uTexel).rgb * 2.0;
  c += texture(uSrc, vUV + vec2(1, -1) * uTexel).rgb;
  c += texture(uSrc, vUV + vec2(-1, 0) * uTexel).rgb * 2.0;
  c += texture(uSrc, vUV).rgb * 4.0;
  c += texture(uSrc, vUV + vec2(1, 0) * uTexel).rgb * 2.0;
  c += texture(uSrc, vUV + vec2(-1, 1) * uTexel).rgb;
  c += texture(uSrc, vUV + vec2(0, 1) * uTexel).rgb * 2.0;
  c += texture(uSrc, vUV + vec2(1, 1) * uTexel).rgb;
  fragColor = vec4(c / 16.0, 1.0);
}
)GLSL";

const char* kCompositeFS = R"GLSL(
in vec2 vUV;
layout(binding = 0) uniform sampler2D uScene;
layout(binding = 1) uniform sampler2D uBloom;
uniform float uExposure;
uniform float uFade;
uniform float uUnderwater;
out vec4 fragColor;

vec3 aces(vec3 x) {
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
  vec2 uv = vUV;
  uv += uUnderwater * 0.0018 * vec2(sin(uv.y * 23.0 + TIME * 1.7), cos(uv.x * 19.0 + TIME * 1.3));
  vec3 col = texture(uScene, uv).rgb;
  vec3 bloom = texture(uBloom, uv).rgb;
  col += bloom * 0.07;
  col *= uExposure;
  col = aces(col);
  float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
  col = max(mix(vec3(lum), col, 1.22), 0.0);
  col = pow(col, vec3(1.0 / 2.2));
  col = mix(col, col * col * (3.0 - 2.0 * col), 0.15);
  vec2 q = vUV - 0.5;
  col *= 1.0 - dot(q, q) * (0.55 + 0.35 * uUnderwater);
  col *= 1.0 - uFade;
  col += (hash12(gl_FragCoord.xy + fract(TIME) * 61.0) - 0.5) / 255.0;
  fragColor = vec4(col, 1.0);
}
)GLSL";

}

void Post::init() {
  downProg_ = compileProgram("bloom down", kFullscreenVS, kDownFS);
  upProg_ = compileProgram("bloom up", kFullscreenVS, kUpFS);
  compositeProg_ = compileProgram("composite", kFullscreenVS, kCompositeFS);
}

void Post::resize(int w, int h) {
  for (int i = 0; i < kLevels; ++i) {
    w = std::max(1, w / 2);
    h = std::max(1, h / 2);
    bloom_[i].create(w, h, GL_RGBA16F, false);
  }
}

void Post::run(const RenderTarget& scene, const RenderTarget& out, const PostSettings& s) {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  glUseProgram(downProg_);
  GLuint src = scene.color;
  int sw = scene.width, sh = scene.height;
  for (int i = 0; i < kLevels; ++i) {
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_[i].fbo);
    glViewport(0, 0, bloom_[i].width, bloom_[i].height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src);
    glUniform2f(glGetUniformLocation(downProg_, "uTexel"), 1.0f / sw, 1.0f / sh);
    glUniform1f(glGetUniformLocation(downProg_, "uPrefilter"), i == 0 ? 1.0f : 0.0f);
    drawFullscreenTriangle();
    src = bloom_[i].color;
    sw = bloom_[i].width;
    sh = bloom_[i].height;
  }
  glUseProgram(upProg_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  for (int i = kLevels - 1; i > 0; --i) {
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_[i - 1].fbo);
    glViewport(0, 0, bloom_[i - 1].width, bloom_[i - 1].height);
    glBindTexture(GL_TEXTURE_2D, bloom_[i].color);
    glUniform2f(glGetUniformLocation(upProg_, "uTexel"), 1.0f / bloom_[i].width, 1.0f / bloom_[i].height);
    drawFullscreenTriangle();
  }
  glDisable(GL_BLEND);

  glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
  glViewport(0, 0, out.width, out.height);
  glUseProgram(compositeProg_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene.color);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, bloom_[0].color);
  glActiveTexture(GL_TEXTURE0);
  glUniform1f(glGetUniformLocation(compositeProg_, "uExposure"), s.exposure);
  glUniform1f(glGetUniformLocation(compositeProg_, "uFade"), s.fade);
  glUniform1f(glGetUniformLocation(compositeProg_, "uUnderwater"), s.underwater);
  drawFullscreenTriangle();
}
