#include "render_util.h"

#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "stb_image.h"

const char* kCommonGLSL = R"GLSL(
#version 450 core
layout(std140, binding = 0) uniform Frame {
  mat4 uView;
  mat4 uProj;
  mat4 uViewProj;
  mat4 uInvViewProj;
  mat4 uShadowMat;
  vec4 uCamPos;
  vec4 uSunDir;
  vec4 uSunColor;
  vec4 uFogColor;
  vec4 uWater;
  vec4 uWhaleLight;
  vec4 uWhaleHead;
  vec4 uScreen;
  vec4 uClipPlane;
  vec4 uWaveA[7];
  vec4 uWaveB[7];
};
layout(binding = 7) uniform sampler2D uNoise;
layout(binding = 6) uniform sampler2DShadow uShadowMap;
#define TIME uCamPos.w
#define PI 3.14159265

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
vec2 hash22(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.xx + p3.yz) * p3.zy);
}

vec3 skyBase(vec3 d) {
  vec3 s = uSunDir.xyz;
  float h = max(d.y, 0.0);
  vec3 zenith = vec3(0.075, 0.18, 0.46);
  vec3 horizon = vec3(0.62, 0.66, 0.74);
  vec3 col = mix(horizon, zenith, pow(h, 0.45));
  vec2 dh = normalize(d.xz + vec2(1e-5));
  vec2 sh = normalize(s.xz + vec2(1e-5));
  float az = 0.5 + 0.5 * dot(dh, sh);
  vec3 warm = vec3(1.0, 0.52, 0.26);
  col = mix(col, warm * 1.2, pow(1.0 - h, 5.0) * (0.2 + 0.8 * az * az * az));
  float sd = max(dot(d, s), 0.0);
  col += warm * (0.35 * pow(sd, 8.0) + 1.4 * pow(sd, 120.0));
  if (d.y < 0.0) col = mix(col, vec3(0.28, 0.32, 0.38), clamp(-d.y * 4.0, 0.0, 1.0));
  return col;
}

vec3 skyColor(vec3 d, bool withSunDisc) {
  vec3 col = skyBase(d);
  vec3 s = uSunDir.xyz;
  float sd = dot(d, s);
  if (d.y > 0.0) {
    vec2 uv = d.xz / (d.y + 0.1);
    vec2 drift = vec2(TIME * 0.004, TIME * 0.0015);
    float c = texture(uNoise, uv * 0.11 + drift).r * 0.6 + texture(uNoise, uv * 0.37 - drift * 1.7).a * 0.4;
    float cover = smoothstep(0.5, 0.74, c) * smoothstep(0.0, 0.18, d.y);
    float lit = pow(max(sd, 0.0), 4.0);
    vec3 cloud = mix(vec3(0.55, 0.55, 0.64), vec3(1.6, 1.05, 0.72), lit);
    cloud = mix(cloud, cloud * vec3(1.15, 0.85, 0.75), pow(1.0 - d.y, 6.0));
    cloud *= mix(1.0, 0.72, smoothstep(0.62, 0.85, c));
    col = mix(col, cloud, cover * 0.85);
  }
  if (withSunDisc) col += vec3(1.0, 0.82, 0.6) * 60.0 * smoothstep(0.99985, 0.99994, sd);
  return col;
}

float sunShadow(vec3 p, vec3 n) {
  vec4 s = uShadowMat * vec4(p + n * 0.25, 1.0);
  if (s.x < 0.0 || s.x > 1.0 || s.y < 0.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
  vec2 texel = vec2(1.0 / 2048.0);
  float sum = texture(uShadowMap, vec3(s.xy, s.z - 0.0008));
  sum += texture(uShadowMap, vec3(s.xy + vec2(1.5, 0.5) * texel, s.z - 0.0008));
  sum += texture(uShadowMap, vec3(s.xy + vec2(-0.5, 1.5) * texel, s.z - 0.0008));
  sum += texture(uShadowMap, vec3(s.xy + vec2(-1.5, -0.5) * texel, s.z - 0.0008));
  sum += texture(uShadowMap, vec3(s.xy + vec2(0.5, -1.5) * texel, s.z - 0.0008));
  return sum * 0.2;
}

vec3 skyAmbient(vec3 n) {
  vec3 skyAvg = vec3(0.34, 0.42, 0.58);
  vec3 ground = vec3(0.09, 0.11, 0.12);
  return mix(ground, skyAvg, n.y * 0.5 + 0.5) * uSunColor.w;
}

float sunReach(float y) { return exp(min(y, 0.0) * 0.11); }

vec3 waterScatter(float depth) {
  float k = clamp(depth / 25.0, 0.0, 1.0);
  return vec3(0.022, 0.105, 0.14) * pow(1.0 - k, 2.6);
}

vec3 applyFog(vec3 col, vec3 p) {
  vec3 c = uCamPos.xyz;
  vec3 v = p - c;
  float d = length(v);
  float yc = c.y, yp = p.y;
  float wet;
  if (uWater.w < -0.5) wet = 0.0;
  else if (yc < 0.0 && yp < 0.0) wet = 1.0;
  else if (yc >= 0.0 && yp >= 0.0) wet = 0.0;
  else if (yc < 0.0) wet = yc / (yc - yp);
  else wet = -yp / (yc - yp);
  float wLen = d * wet;
  if (uWater.w > 0.5) {
    float vis = uFogColor.w;
    float begin = uWater.z;
    float f = clamp((wLen - begin) / max(vis - begin, 0.5), 0.0, 1.0);
    float lookDepth = max(uWater.x - (v.y / max(d, 1e-3)) * 5.0, 0.0);
    vec3 fogCol = mix(uFogColor.rgb, waterScatter(lookDepth), 0.5);
    return mix(col, fogCol, f);
  }
  if (wLen > 0.0) {
    float f = 1.0 - exp(-wLen / 6.0);
    col = mix(col, waterScatter(3.0) * (0.25 + 0.75 * uSunColor.w), f);
  }
  float airLen = d - wLen;
  float fa = 1.0 - exp(-max(airLen - 120.0, 0.0) * 0.0022);
  return mix(col, skyBase(normalize(v)), fa * 0.85);
}

float voronoiEdge(vec2 p, float t) {
  vec2 n = floor(p), f = fract(p);
  float d1 = 8.0, d2 = 8.0;
  for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
      vec2 g = vec2(i, j);
      vec2 o = hash22(n + g);
      o = 0.5 + 0.42 * sin(t + 6.2831 * o);
      vec2 r = g + o - f;
      float dd = dot(r, r);
      if (dd < d1) { d2 = d1; d1 = dd; } else if (dd < d2) { d2 = dd; }
    }
  return sqrt(d2) - sqrt(d1);
}

float caustics(vec3 p) {
  vec2 uv = (p.xz - uSunDir.xz * 0.35 * p.y) * 0.21;
  float t = TIME * 0.9;
  float a = voronoiEdge(uv + vec2(0.0, p.y * 0.02), t);
  float b = voronoiEdge(uv * 1.37 + vec2(3.1, 1.7), -t * 1.13);
  float c = (1.0 - smoothstep(0.0, 0.16, a)) + (1.0 - smoothstep(0.0, 0.16, b));
  return c * c * 0.5;
}
)GLSL";

#ifdef HERTZ_GL41
static std::vector<std::pair<std::string, int>> gSamplerUnits, gBlockBindings;
static bool gVertexClips = false;

static std::string toPortableGLSL(std::string src, GLenum type, const std::string& name) {
  static const std::regex sampler(R"(layout\s*\(\s*binding\s*=\s*(\d+)\s*\)\s*uniform\s+(\w+)\s+(\w+))");
  static const std::regex block(R"(layout\s*\(\s*std140\s*,\s*binding\s*=\s*(\d+)\s*\)\s*uniform\s+(\w+))");
  for (std::sregex_iterator it(src.begin(), src.end(), sampler), end; it != end; ++it)
    gSamplerUnits.emplace_back((*it)[3], std::stoi((*it)[1]));
  for (std::sregex_iterator it(src.begin(), src.end(), block), end; it != end; ++it)
    gBlockBindings.emplace_back((*it)[2], std::stoi((*it)[1]));
  src = std::regex_replace(src, sampler, "uniform $2 $3");
  src = std::regex_replace(src, block, "layout(std140) uniform $2");
  if (type == GL_VERTEX_SHADER && name.find("shadow") != std::string::npos) {
    src = std::regex_replace(src, std::regex(R"(void\s+main\s*\(\s*\))"), "void hertzVertexMain()");
    src += "\nvoid main() {\n  hertzVertexMain();\n  gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n}\n";
  }
#ifdef HERTZ_ES
  src.erase(0, src.find("#version"));
  src = std::regex_replace(src, std::regex("#version 450 core"),
                           "#version 300 es\n"
                           "precision highp float;\nprecision highp int;\n"
                           "precision highp sampler2D;\nprecision highp sampler2DShadow;\n"
                           "#define HERTZ_GL41\n#define HERTZ_ES");
  if (type == GL_VERTEX_SHADER) {
    gVertexClips = src.find("gl_ClipDistance") != std::string::npos;
    src = std::regex_replace(src, std::regex(R"(out\s+float\s+gl_ClipDistance\s*\[\s*1\s*\]\s*;)"),
                             "out float vClipDistance;");
    src = std::regex_replace(src, std::regex(R"(gl_ClipDistance\s*\[\s*0\s*\])"), "vClipDistance");
  } else if (gVertexClips) {
    src = std::regex_replace(src, std::regex(R"(void\s+main\s*\(\s*\))"), "void hertzMain()");
    src += "\nin float vClipDistance;\nvoid main() {\n  if (vClipDistance < 0.0) discard;\n  hertzMain();\n}\n";
  }
#else
  (void)type;
  src = std::regex_replace(src, std::regex("#version 450 core"), "#version 410 core\n#define HERTZ_GL41");
#endif
  return src;
}
#endif

static GLuint compileStage(const char* name, GLenum type, const std::string& src) {
  GLuint s = glCreateShader(type);
  std::string full = std::string(kCommonGLSL) +
                     (type == GL_VERTEX_SHADER ? "#define VERTEX_SHADER\n" : "#define FRAGMENT_SHADER\n") + src;
#ifdef HERTZ_GL41
  full = toPortableGLSL(full, type, name);
#endif
  if (const char* dump = std::getenv("HERTZ_DUMP_SHADERS")) {
    std::string path = std::string(dump) + "/" + name + (type == GL_VERTEX_SHADER ? ".vert" : ".frag");
    if (FILE* f = std::fopen(path.c_str(), "w")) {
      std::fputs(full.c_str(), f);
      std::fclose(f);
    }
  }
  const char* p = full.c_str();
  glShaderSource(s, 1, &p, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetShaderInfoLog(s, sizeof log, nullptr, log);
    std::fprintf(stderr, "Shader '%s' (%s) failed to compile:\n%s\n", name,
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
    std::exit(1);
  }
  return s;
}

GLuint compileProgram(const char* name, const std::string& vertex, const std::string& fragment) {
  GLuint vs = compileStage(name, GL_VERTEX_SHADER, vertex);
  GLuint fs = compileStage(name, GL_FRAGMENT_SHADER, fragment);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[8192];
    glGetProgramInfoLog(prog, sizeof log, nullptr, log);
    std::fprintf(stderr, "Program '%s' failed to link:\n%s\n", name, log);
    std::exit(1);
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
#ifdef HERTZ_GL41
  glUseProgram(prog);
  for (auto& [uniform, unit] : gSamplerUnits) {
    GLint loc = glGetUniformLocation(prog, uniform.c_str());
    if (loc >= 0) glUniform1i(loc, unit);
  }
  for (auto& [blockName, binding] : gBlockBindings) {
    GLuint index = glGetUniformBlockIndex(prog, blockName.c_str());
    if (index != GL_INVALID_INDEX) glUniformBlockBinding(prog, index, binding);
  }
  gSamplerUnits.clear();
  gBlockBindings.clear();
  gVertexClips = false;
  glUseProgram(0);
#endif
  return prog;
}

static GLuint gFrameUBO = 0;

void initFrameBlock() {
  glGenBuffers(1, &gFrameUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, gFrameUBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameData), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, gFrameUBO);
}

void uploadFrameBlock(const FrameData& data) {
  glBindBuffer(GL_UNIFORM_BUFFER, gFrameUBO);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameData), &data);
}

namespace {
constexpr int kNoiseSize = 256;

struct PeriodicNoise {
  unsigned seed;
  float grad(int ix, int iy, int period, float dx, float dy) const {
    ix = ((ix % period) + period) % period;
    iy = ((iy % period) + period) % period;
    unsigned h = (unsigned)ix * 73856093u ^ (unsigned)iy * 19349663u ^ seed * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    float a = (h & 0xffff) / 65535.0f * 6.2831853f;
    return std::cos(a) * dx + std::sin(a) * dy;
  }
  float at(float x, float y, int period) const {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    float u = fade(fx), v = fade(fy);
    float n00 = grad(ix, iy, period, fx, fy);
    float n10 = grad(ix + 1, iy, period, fx - 1, fy);
    float n01 = grad(ix, iy + 1, period, fx, fy - 1);
    float n11 = grad(ix + 1, iy + 1, period, fx - 1, fy - 1);
    float a = n00 + (n10 - n00) * u;
    float b = n01 + (n11 - n01) * u;
    return a + (b - a) * v;
  }
  float fbm(float x, float y, int baseCells) const {
    float sum = 0, amp = 0.5f, norm = 0;
    int cells = baseCells;
    for (int o = 0; o < 6 && cells <= kNoiseSize / 2; ++o) {
      float s = (float)cells / kNoiseSize;
      sum += amp * at(x * s, y * s, cells);
      norm += amp;
      amp *= 0.5f;
      cells *= 2;
    }
    return 0.5f + 0.75f * sum / norm;
  }
};
}

void initNoiseTexture() {
  PeriodicNoise a{17u}, b{4711u};
  std::vector<float> r(kNoiseSize * kNoiseSize), alpha(kNoiseSize * kNoiseSize);
  for (int y = 0; y < kNoiseSize; ++y)
    for (int x = 0; x < kNoiseSize; ++x) {
      r[y * kNoiseSize + x] = a.fbm((float)x, (float)y, 4);
      alpha[y * kNoiseSize + x] = b.fbm((float)x, (float)y, 8);
    }
  std::vector<unsigned char> px(kNoiseSize * kNoiseSize * 4);
  auto at = [&](int x, int y) {
    return r[((y + kNoiseSize) % kNoiseSize) * kNoiseSize + (x + kNoiseSize) % kNoiseSize];
  };
  auto u8 = [](float v) { return (unsigned char)std::lround(std::fmin(std::fmax(v, 0.f), 1.f) * 255.f); };
  for (int y = 0; y < kNoiseSize; ++y)
    for (int x = 0; x < kNoiseSize; ++x) {
      float dx = (at(x + 1, y) - at(x - 1, y)) * 0.5f;
      float dy = (at(x, y + 1) - at(x, y - 1)) * 0.5f;
      unsigned char* p = &px[(y * kNoiseSize + x) * 4];
      p[0] = u8(at(x, y));
      p[1] = u8(0.5f + dx * 12.f);
      p[2] = u8(0.5f + dy * 12.f);
      p[3] = u8(alpha[y * kNoiseSize + x]);
    }
  GLuint tex;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kNoiseSize, kNoiseSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
  glActiveTexture(GL_TEXTURE0);
}

void drawFullscreenTriangle() {
  static GLuint vao = 0;
  if (!vao) glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void RenderTarget::create(int w, int h, GLenum colorFormat, bool withDepth) {
  destroy();
  width = w;
  height = h;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &color);
  glBindTexture(GL_TEXTURE_2D, color);
  GLenum type = colorFormat == GL_RGBA8 ? GL_UNSIGNED_BYTE : GL_HALF_FLOAT;
  glTexImage2D(GL_TEXTURE_2D, 0, colorFormat, w, h, 0, GL_RGBA, type, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  if (withDepth) {
    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
  }
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::fprintf(stderr, "Framebuffer %dx%d is incomplete\n", w, h);
    std::exit(1);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::destroy() {
  if (fbo) glDeleteFramebuffers(1, &fbo);
  if (color) glDeleteTextures(1, &color);
  if (depth) glDeleteTextures(1, &depth);
  fbo = color = depth = 0;
}

Image decodePNG(const std::string& path, int maxSize) {
  int w, h, n;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
  if (!data) {
    std::fprintf(stderr, "Could not load %s: %s\n", path.c_str(), stbi_failure_reason());
    std::exit(1);
  }
  std::vector<unsigned char> img(data, data + (size_t)w * h * 4);
  stbi_image_free(data);
  while (w > maxSize || h > maxSize) {
    int nw = w / 2, nh = h / 2;
    std::vector<unsigned char> half((size_t)nw * nh * 4);
    for (int y = 0; y < nh; ++y)
      for (int x = 0; x < nw; ++x)
        for (int c = 0; c < 4; ++c) {
          int s = img[((2 * y) * w + 2 * x) * 4 + c] + img[((2 * y) * w + 2 * x + 1) * 4 + c] +
                  img[((2 * y + 1) * w + 2 * x) * 4 + c] + img[((2 * y + 1) * w + 2 * x + 1) * 4 + c];
          half[(y * nw + x) * 4 + c] = (unsigned char)((s + 2) / 4);
        }
    img.swap(half);
    w = nw;
    h = nh;
  }
  return {w, h, std::move(img)};
}

GLuint uploadTexture(const Image& img, bool srgb) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, img.width, img.height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, img.pixels.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
  return tex;
}
