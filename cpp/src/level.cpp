#include "level.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <numeric>
#include <random>
#include <set>

#include <glm/gtc/type_ptr.hpp>

#include "render_util.h"

namespace {

// ---------------------------------------------------------------- rock noise
struct Perlin3 {
  int perm[512];
  explicit Perlin3(unsigned seed) {
    int p[256];
    std::iota(p, p + 256, 0);
    std::mt19937 rng(seed);
    std::shuffle(p, p + 256, rng);
    for (int i = 0; i < 512; ++i) perm[i] = p[i & 255];
  }
  static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
  static float grad(int h, float x, float y, float z) {
    h &= 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
  }
  float operator()(float x, float y, float z) const {
    int X = (int)std::floor(x) & 255, Y = (int)std::floor(y) & 255, Z = (int)std::floor(z) & 255;
    x -= std::floor(x), y -= std::floor(y), z -= std::floor(z);
    float u = fade(x), v = fade(y), w = fade(z);
    int A = perm[X] + Y, AA = perm[A] + Z, AB = perm[A + 1] + Z;
    int B = perm[X + 1] + Y, BA = perm[B] + Z, BB = perm[B + 1] + Z;
    auto lerp = [](float t, float a, float b) { return a + t * (b - a); };
    return lerp(w,
                lerp(v, lerp(u, grad(perm[AA], x, y, z), grad(perm[BA], x - 1, y, z)),
                     lerp(u, grad(perm[AB], x, y - 1, z), grad(perm[BB], x - 1, y - 1, z))),
                lerp(v, lerp(u, grad(perm[AA + 1], x, y, z - 1), grad(perm[BA + 1], x - 1, y, z - 1)),
                     lerp(u, grad(perm[AB + 1], x, y - 1, z - 1), grad(perm[BB + 1], x - 1, y - 1, z - 1))));
  }
};

const Perlin3& noise() {
  static Perlin3 n(52u);
  return n;
}

// ---------------------------------------------------------------- shaders
const char* kRockVS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uLightViewProj;
out float gl_ClipDistance[1];
out vec3 vWorld;
out vec3 vNormal;
void main() {
  vWorld = aPos;
  vNormal = aNormal;
#ifdef SHADOW
  gl_Position = uLightViewProj * vec4(aPos, 1.0);
#else
  gl_Position = uViewProj * vec4(aPos, 1.0);
  gl_ClipDistance[0] = dot(uClipPlane, vec4(aPos, 1.0));
#endif
}
)GLSL";

const char* kRockFS = R"GLSL(
in vec3 vWorld;
in vec3 vNormal;
out vec4 fragColor;
void main() {
#ifdef SHADOW
  fragColor = vec4(1.0);
#else
  vec3 p = vWorld;
  vec3 N = normalize(vNormal);
  vec3 w = pow(abs(N), vec3(4.0));
  w /= dot(w, vec3(1.0));
  // Triplanar rock detail at two scales.
  vec4 ax = texture(uNoise, p.zy * vec2(0.06, 0.03));
  vec4 ay = texture(uNoise, p.xz * 0.06);
  vec4 az = texture(uNoise, p.xy * vec2(0.06, 0.03));
  vec4 bx = texture(uNoise, p.zy * 0.27);
  vec4 by = texture(uNoise, p.xz * 0.27);
  vec4 bz = texture(uNoise, p.xy * 0.27);
  vec4 n1 = ax * w.x + ay * w.y + az * w.z;
  vec4 n2 = bx * w.x + by * w.y + bz * w.z;
  vec2 gx = (ax.gb - 0.5) + (bx.gb - 0.5) * 0.8;
  vec2 gy = (ay.gb - 0.5) + (by.gb - 0.5) * 0.8;
  vec2 gz = (az.gb - 0.5) + (bz.gb - 0.5) * 0.8;
  N = normalize(N - 1.4 * (w.x * vec3(0.0, gx.y, gx.x) + w.y * vec3(gy.x, 0.0, gy.y) + w.z * vec3(gz.x, gz.y, 0.0)));

  float h = n1.r * 0.55 + n2.r * 0.45;
  vec3 rock = vec3(0.32, 0.30, 0.28) * (0.55 + 0.9 * h);
  float band = texture(uNoise, vec2(p.y * 0.021, 0.37 + (p.x + p.z) * 0.0009)).a;
  rock *= mix(vec3(0.82, 0.86, 0.92), vec3(1.18, 1.02, 0.86), smoothstep(0.3, 0.7, band));
  // Pale, weathered tops and ledges above the water.
  rock = mix(rock, vec3(0.52, 0.50, 0.46) * (0.7 + 0.5 * h), smoothstep(0.55, 0.9, N.y) * step(0.0, p.y) * 0.7);
  // Wet and darker below the splash line, with a band of weed at the waterline.
  float wet = 1.0 - smoothstep(-0.5, 2.5, p.y + (h - 0.5) * 2.0);
  rock = mix(rock, rock * vec3(0.5, 0.56, 0.52), wet);
  float weed = smoothstep(-7.0, -1.5, p.y) * (1.0 - smoothstep(-0.8, 1.2, p.y)) * smoothstep(0.42, 0.62, n2.a);
  rock = mix(rock, vec3(0.07, 0.12, 0.05), weed * 0.8);

  float depth = max(-p.y, 0.0);
  float reach = sunReach(p.y);
  float shadow = sunShadow(p, normalize(vNormal));
  vec3 sun = uSunColor.rgb * reach * shadow * mix(vec3(1.0), vec3(0.3, 0.8, 1.0), clamp(depth / 6.0, 0.0, 1.0));
  float NdotL = max(dot(N, uSunDir.xyz), 0.0);
  float ao = 0.55 + 0.45 * h;
  vec3 col = rock * sun * NdotL * 0.9;
  vec3 sky = skyAmbient(N);
  if (p.y >= 0.0) {
    col += rock * sky * ao * 1.1;
    // Warm bounce light off the sea.
    col += rock * vec3(0.10, 0.09, 0.08) * max(-N.y, 0.0);
  } else {
    col += rock * (sky * 0.5 * reach + waterScatter(depth) * 2.0) * ao;
    if (depth < 24.0) col += rock * uSunColor.rgb * reach * shadow * caustics(p) * (0.25 + 0.5 * max(N.y, 0.0)) * 0.9;
  }
  fragColor = vec4(applyFog(col, p), 1.0);
#endif
}
)GLSL";

struct RockVertex {
  float pos[3];
  float normal[3];
};

constexpr int DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

using Cell = std::pair<int, int>;
using Edge = std::pair<Cell, Cell>;

Edge edge(Cell a, Cell b) { return a < b ? Edge{a, b} : Edge{b, a}; }

std::set<Edge> carve(unsigned seed, Cell start) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> uni(0.0f, 1.0f);
  std::set<Edge> open;
  std::set<Cell> seen{start};
  struct Item {
    Cell cell;
    int last;
  };
  std::vector<Item> stack{{start, -1}};
  while (!stack.empty()) {
    auto [cell, last] = stack.back();
    std::vector<int> options;
    for (int d = 0; d < 4; ++d) {
      Cell n{cell.first + DIRS[d][0], cell.second + DIRS[d][1]};
      if (n.first >= 0 && n.first < Level::kCols && n.second >= 0 && n.second < Level::kRows && !seen.count(n))
        options.push_back(d);
    }
    if (options.empty()) {
      stack.pop_back();
      continue;
    }
    bool canContinue = std::find(options.begin(), options.end(), last) != options.end();
    // Bias towards going straight, so the maze has long corridors.
    int d = (canContinue && uni(rng) < 0.65f) ? last : options[rng() % options.size()];
    Cell n{cell.first + DIRS[d][0], cell.second + DIRS[d][1]};
    open.insert(edge(cell, n));
    seen.insert(n);
    stack.push_back({n, d});
  }
  return open;
}

std::map<Cell, int> distances(const std::set<Edge>& open, Cell start) {
  std::map<Cell, int> dist{{start, 0}};
  std::deque<Cell> q{start};
  while (!q.empty()) {
    Cell c = q.front();
    q.pop_front();
    for (auto& d : DIRS) {
      Cell n{c.first + d[0], c.second + d[1]};
      if (!dist.count(n) && open.count(edge(c, n))) {
        dist[n] = dist[c] + 1;
        q.push_back(n);
      }
    }
  }
  return dist;
}

}  // namespace

float Level::rockOffset(glm::vec3 p) {
  const Perlin3& n = noise();
  float d = 1.5f * n(p.x * 0.035f, p.y * 0.011f, p.z * 0.035f);
  d += 0.8f * n(p.x * 0.09f + 17.0f, p.y * 0.03f, p.z * 0.09f);
  d += 0.3f * n(p.x * 0.25f, p.y * 0.25f + 5.0f, p.z * 0.25f);
  float strata = std::sin(p.y * 0.45f + 2.5f * n(p.x * 0.02f, p.y * 0.05f, p.z * 0.02f));
  d += 0.45f * strata * strata * strata;
  // Mostly carved inwards, so the rock rarely pokes out past the collision box.
  return d * 0.9f - 0.7f;
}

float Level::floorHeight(float x, float z) {
  return kFloorY + 1.2f * noise()(x * 0.02f, 0.5f, z * 0.02f) - 0.6f;
}

void Level::init() {
  program_ = compileProgram("rock", kRockVS, kRockFS);
  shadowProgram_ = compileProgram("rock shadow", std::string("#define SHADOW\n") + kRockVS,
                                  std::string("#define SHADOW\n") + kRockFS);
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glGenBuffers(1, &ibo_);
}

void Level::generate(unsigned seed) {
  const Cell startCell{2, 0};
  auto cx = [](int x) { return (x - 2) * kCell; };
  auto cz = [](int z) { return -(z - 0) * kCell; };

  std::set<Edge> open;
  int exitX = 0, steps = 0;
  unsigned s = seed;
  for (;; ++s) {
    open = carve(s, startCell);
    auto dist = distances(open, startCell);
    exitX = 0;
    steps = -1;
    for (int x = 0; x < kCols; ++x)
      if (dist[{x, kRows - 1}] > steps) steps = dist[{x, kRows - 1}], exitX = x;
    // The whale starts facing +X, so the first cell must open that way.
    bool opensEast = open.count(edge(startCell, {startCell.first + 1, startCell.second})) > 0;
    if (steps >= 8 && steps <= 12 && opensEast && exitX != startCell.first) break;
    if (s - seed > 5000) break;
  }
  seedUsed = s;
  shortestPath = steps;

  // Walk back from the exit cell along decreasing distance to get the route.
  {
    auto dist = distances(open, startCell);
    std::vector<Cell> cells{{exitX, kRows - 1}};
    while (cells.back() != startCell) {
      Cell c = cells.back();
      for (auto& d : DIRS) {
        Cell n{c.first + d[0], c.second + d[1]};
        if (dist.count(n) && dist[n] == dist[c] - 1 && open.count(edge(c, n))) {
          cells.push_back(n);
          break;
        }
      }
    }
    route.clear();
    for (auto it = cells.rbegin(); it != cells.rend(); ++it)
      route.push_back(glm::vec3((it->first - 2) * kCell, -10.0f, -it->second * kCell));
  }

  const float H = 90.0f, wallY = -15.0f, L = kCell + kThick;
  walls_.clear();
  auto addWall = [&](float x, float z, bool alongZ) {
    glm::vec3 half = alongZ ? glm::vec3(kThick, H, L) * 0.5f : glm::vec3(L, H, kThick) * 0.5f;
    glm::vec3 c(x, wallY, z);
    walls_.push_back({c - half, c + half});
  };
  for (int z = 0; z <= kRows; ++z)
    for (int x = 0; x < kCols; ++x) {
      if (z == kRows && x == exitX) continue;  // the way out
      if (z > 0 && z < kRows && open.count(edge({x, z - 1}, {x, z}))) continue;
      addWall(cx(x), cz(z) + kCell / 2, false);
    }
  for (int x = 0; x <= kCols; ++x)
    for (int z = 0; z < kRows; ++z) {
      if (x > 0 && x < kCols && open.count(edge({x - 1, z}, {x, z}))) continue;
      addWall(cx(x) - kCell / 2, cz(z), true);
    }

  start = glm::vec3(0.0f, -10.0f, 0.0f);
  startYaw = -1.5707963f;
  exitGap = glm::vec3(cx(exitX), -10.0f, cz(kRows - 1) - kCell / 2);
  goal = glm::vec3(cx(exitX), -10.0f, cz(kRows - 1) - kCell / 2 - 30.0f);
  route.push_back(exitGap);
  route.push_back(goal);
  bounds = glm::vec4(cx(0) - kCell / 2, cz(kRows - 1) - kCell / 2, cx(kCols - 1) + kCell / 2, cz(0) + kCell / 2);
  if (std::getenv("HERTZ_DEBUG_WALLS"))
    for (const Box& b : walls_)
      std::printf("wall x %.1f..%.1f z %.1f..%.1f\n", b.lo.x, b.hi.x, b.lo.z, b.hi.z);
  std::printf("maze: seed %u, %zu walls, exit in column %d, shortest route %d cells\n", seedUsed, walls_.size(),
              exitX, steps);
  buildMesh();
}

void Level::buildMesh() {
  std::vector<RockVertex> verts;
  std::vector<uint32_t> idx;
  const Perlin3& n = noise();

  for (const Box& b : walls_) {
    glm::vec3 size = b.hi - b.lo;
    int nx = std::max(2, (int)std::lround(size.x / 1.6f));
    int ny = std::max(2, (int)std::lround(size.y / 1.8f));
    int nz = std::max(2, (int)std::lround(size.z / 1.6f));
    // Displace along the face normal (or the average normal on edges, so faces stay joined).
    auto displace = [&](glm::vec3 p) {
      const float e = 1e-3f;
      glm::vec3 dir(0.0f);
      if (p.x < b.lo.x + e) dir.x -= 1;
      if (p.x > b.hi.x - e) dir.x += 1;
      if (p.z < b.lo.z + e) dir.z -= 1;
      if (p.z > b.hi.z - e) dir.z += 1;
      bool top = p.y > b.hi.y - e;
      if (top) dir.y += 1;
      if (glm::dot(dir, dir) == 0.0f) return p;
      // Fade the rock out at the seafloor so the base meets the sand.
      float fadeBottom = glm::clamp((p.y - b.lo.y) / 4.0f, 0.0f, 1.0f);
      glm::vec3 q = p + glm::normalize(dir) * rockOffset(p) * fadeBottom;
      if (top) q.y += 4.0f * n(p.x * 0.06f, 3.3f, p.z * 0.06f) + 2.0f * n(p.x * 0.17f, 7.1f, p.z * 0.17f) - 1.0f;
      return q;
    };
    auto face = [&](glm::vec3 origin, glm::vec3 du, glm::vec3 dv, int nu, int nv) {
      uint32_t base = (uint32_t)verts.size();
      std::vector<glm::vec3> grid((nu + 1) * (nv + 1));
      for (int j = 0; j <= nv; ++j)
        for (int i = 0; i <= nu; ++i)
          grid[j * (nu + 1) + i] = displace(origin + du * ((float)i / nu) + dv * ((float)j / nv));
      glm::vec3 faceN = glm::normalize(glm::cross(du, dv));
      for (int j = 0; j <= nv; ++j)
        for (int i = 0; i <= nu; ++i) {
          glm::vec3 a = grid[j * (nu + 1) + std::min(i + 1, nu)] - grid[j * (nu + 1) + std::max(i - 1, 0)];
          glm::vec3 c = grid[std::min(j + 1, nv) * (nu + 1) + i] - grid[std::max(j - 1, 0) * (nu + 1) + i];
          glm::vec3 nn = glm::cross(a, c);
          nn = glm::dot(nn, nn) > 1e-12f ? glm::normalize(nn) : faceN;
          if (glm::dot(nn, faceN) < 0.0f) nn = -nn;
          glm::vec3 p = grid[j * (nu + 1) + i];
          verts.push_back({{p.x, p.y, p.z}, {nn.x, nn.y, nn.z}});
        }
      for (int j = 0; j < nv; ++j)
        for (int i = 0; i < nu; ++i) {
          uint32_t a = base + j * (nu + 1) + i, b2 = a + 1, c = a + (nu + 1), d = c + 1;
          idx.insert(idx.end(), {a, b2, d, a, d, c});
        }
    };
    glm::vec3 lo = b.lo, hi = b.hi;
    face({hi.x, lo.y, hi.z}, {0, 0, -size.z}, {0, size.y, 0}, nz, ny);  // +X
    face({lo.x, lo.y, lo.z}, {0, 0, size.z}, {0, size.y, 0}, nz, ny);   // -X
    face({lo.x, lo.y, hi.z}, {size.x, 0, 0}, {0, size.y, 0}, nx, ny);   // +Z
    face({hi.x, lo.y, lo.z}, {-size.x, 0, 0}, {0, size.y, 0}, nx, ny);  // -Z
    face({lo.x, hi.y, hi.z}, {size.x, 0, 0}, {0, 0, -size.z}, nx, nz);  // top
  }

  // The seafloor: a big gently rolling plane.
  {
    const int N = 96;
    const float extent = 900.0f;
    uint32_t base = (uint32_t)verts.size();
    auto floorY = [](float x, float z) { return floorHeight(x, z); };
    for (int j = 0; j <= N; ++j)
      for (int i = 0; i <= N; ++i) {
        float x = -extent + 2 * extent * i / N, z = -75.0f - extent + 2 * extent * j / N;
        float h = floorY(x, z);
        glm::vec3 nn = glm::normalize(
            glm::vec3(floorY(x - 1, z) - floorY(x + 1, z), 2.0f, floorY(x, z - 1) - floorY(x, z + 1)));
        verts.push_back({{x, h, z}, {nn.x, nn.y, nn.z}});
      }
    for (int j = 0; j < N; ++j)
      for (int i = 0; i < N; ++i) {
        uint32_t a = base + j * (N + 1) + i, b2 = a + 1, c = a + (N + 1), d = c + 1;
        idx.insert(idx.end(), {a, c, d, a, d, b2});
      }
  }

  indexCount_ = (int)idx.size();
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(RockVertex), verts.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RockVertex), (void*)offsetof(RockVertex, pos));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RockVertex), (void*)offsetof(RockVertex, normal));
  glBindVertexArray(0);
  std::printf("maze mesh: %zu vertices, %d triangles\n", verts.size(), indexCount_ / 3);
}

std::vector<glm::vec4> Level::footprints() const {
  std::vector<glm::vec4> out;
  for (const Box& b : walls_) out.push_back(glm::vec4(b.lo.x, b.lo.z, b.hi.x, b.hi.z));
  return out;
}

void Level::draw() const {
  glUseProgram(program_);
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

void Level::drawShadow(const glm::mat4& lightViewProj) const {
  glUseProgram(shadowProgram_);
  glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uLightViewProj"), 1, GL_FALSE,
                     glm::value_ptr(lightViewProj));
  glBindVertexArray(vao_);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

bool Level::raycast(glm::vec3 from, glm::vec3 dir, float maxDist, RayHit& hit) const {
  float best = maxDist;
  bool found = false;
  auto test = [&](const Box& b) {
    float tmin = 0.0f, tmax = best;
    int axis = -1;
    float sign = 0.0f;
    for (int a = 0; a < 3; ++a) {
      if (std::fabs(dir[a]) < 1e-8f) {
        if (from[a] < b.lo[a] || from[a] > b.hi[a]) return;
        continue;
      }
      float inv = 1.0f / dir[a];
      float t0 = (b.lo[a] - from[a]) * inv, t1 = (b.hi[a] - from[a]) * inv;
      float s = -1.0f;
      if (t0 > t1) std::swap(t0, t1), s = 1.0f;
      if (t0 > tmin) tmin = t0, axis = a, sign = s;
      tmax = std::min(tmax, t1);
      if (tmin > tmax) return;
    }
    if (axis < 0) return;  // started inside
    best = tmin;
    found = true;
    hit.t = tmin;
    hit.pos = from + dir * tmin;
    hit.normal = glm::vec3(0.0f);
    hit.normal[axis] = sign;
  };
  for (const Box& b : walls_) test(b);
  // Seafloor plane.
  if (dir.y < -1e-6f) {
    float t = (kFloorY - from.y) / dir.y;
    if (t > 0.0f && t < best) {
      best = t;
      found = true;
      hit.t = t;
      hit.pos = from + dir * t;
      hit.normal = glm::vec3(0, 1, 0);
    }
  }
  return found;
}

bool Level::resolveSphere(glm::vec3& c, float r, glm::vec3& normal) const {
  bool touched = false;
  normal = glm::vec3(0.0f);
  for (const Box& b : walls_) {
    glm::vec3 q = glm::clamp(c, b.lo, b.hi);
    glm::vec3 d = c - q;
    float dist2 = glm::dot(d, d);
    if (dist2 >= r * r) continue;
    glm::vec3 n;
    float push;
    if (dist2 > 1e-8f) {
      float dist = std::sqrt(dist2);
      n = d / dist;
      push = r - dist;
    } else {
      // Centre is inside the box: leave along the shortest way out.
      glm::vec3 toLo = c - b.lo, toHi = b.hi - c;
      float m = 1e9f;
      for (int a = 0; a < 3; ++a) {
        if (toLo[a] < m) m = toLo[a], n = glm::vec3(0.0f), n[a] = -1.0f;
        if (toHi[a] < m) m = toHi[a], n = glm::vec3(0.0f), n[a] = 1.0f;
      }
      push = m + r;
    }
    c += n * push;
    normal += n;
    touched = true;
  }
  if (c.y < kFloorY + r) {
    c.y = kFloorY + r;
    normal += glm::vec3(0, 1, 0);
    touched = true;
  }
  return touched;
}
