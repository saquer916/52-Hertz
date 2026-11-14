#include "whale_model.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "cgltf.h"
#include "render_util.h"

#ifdef __EMSCRIPTEN__
static constexpr std::launch kDecodeLaunch = std::launch::deferred;  // no threads in the browser
#else
static constexpr std::launch kDecodeLaunch = std::launch::async;
#endif

namespace {

const char* kWhaleVS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aTangent;
layout(location = 3) in vec2 aUV;
layout(location = 4) in uvec4 aJoints;
layout(location = 5) in vec4 aWeights;
uniform mat4 uModel;
uniform mat4 uJoints[16];
uniform mat4 uLightViewProj;  // only for the shadow pass
out float gl_ClipDistance[1];
out vec3 vWorld;
out vec3 vNormal;
out vec4 vTangent;
out vec2 vUV;
void main() {
  mat4 skin = aWeights.x * uJoints[aJoints.x] + aWeights.y * uJoints[aJoints.y] +
              aWeights.z * uJoints[aJoints.z] + aWeights.w * uJoints[aJoints.w];
  vec4 world = uModel * (skin * vec4(aPos, 1.0));
  mat3 m = mat3(uModel) * mat3(skin);
  vWorld = world.xyz;
  vNormal = normalize(m * aNormal);
  vTangent = vec4(normalize(m * aTangent.xyz), aTangent.w);
  vUV = aUV;
#ifdef SHADOW
  gl_Position = uLightViewProj * world;
#else
  gl_Position = uViewProj * world;
  gl_ClipDistance[0] = dot(uClipPlane, world);
#endif
}
)GLSL";

const char* kWhaleFS = R"GLSL(
in vec3 vWorld;
in vec3 vNormal;
in vec4 vTangent;
in vec2 vUV;
layout(binding = 0) uniform sampler2D uBase;
layout(binding = 1) uniform sampler2D uNormalMap;
layout(binding = 2) uniform sampler2D uORM;
uniform float uFogAmount;
uniform vec3 uGlow;
uniform vec4 uLight;  // xyz position, w strength
out vec4 fragColor;

float ggx(float NdotH, float a) {
  float a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d);
}

void main() {
#ifdef SHADOW
  fragColor = vec4(1.0);
  return;
#endif
  vec3 base = texture(uBase, vUV).rgb;
  vec3 orm = texture(uORM, vUV).rgb;
  float rough = clamp(orm.g * 0.5, 0.2, 1.0);  // the material's roughness factor is 0.5
  vec3 nm = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
  vec3 N = normalize(vNormal);
  vec3 T = normalize(vTangent.xyz - N * dot(N, vTangent.xyz));
  vec3 B = cross(N, T) * vTangent.w;
  N = normalize(mat3(T, B, N) * nm);
  if (!gl_FrontFacing) N = -N;
  vec3 V = normalize(uCamPos.xyz - vWorld);
  float NdotV = max(dot(N, V), 0.0);
  bool camUnder = uWater.w > 0.5;
  float depth = max(-vWorld.y, 0.0);

  // Sun (dimmed and tinted by the water above this point, shadowed by cliffs).
  vec3 L = uSunDir.xyz;
  float reach = sunReach(vWorld.y);
  vec3 sun = uSunColor.rgb * reach * mix(vec3(1.0), vec3(0.35, 0.8, 1.0), clamp(depth / 6.0, 0.0, 1.0));
  sun *= sunShadow(vWorld, N);
  float NdotL = max(dot(N, L), 0.0);
  vec3 H = normalize(L + V);
  float spec = ggx(max(dot(N, H), 0.0), rough * rough) * 0.04 * NdotL;
  vec3 col = base * sun * NdotL / PI * 2.4 + sun * spec;

  // Ambient: sky light from above, water glow from below.
  float up = N.y * 0.5 + 0.5;
  vec3 skyAmb = skyAmbient(N);
  vec3 waterAmb = waterScatter(depth) * 3.0;
  vec3 amb = depth > 0.0 ? mix(waterAmb, skyAmb * reach, up * 0.7) : skyAmb;
  col += base * amb * orm.r;

  // Caustics on the back near the surface.
  if (vWorld.y < 0.0 && vWorld.y > -20.0)
    col += base * sun * caustics(vWorld) * max(N.y, 0.0) * 0.6;

  // The whale's own soft light, so it never disappears in the deep.
  vec3 toLight = uLight.xyz - vWorld;
  float dl = length(toLight);
  float att = pow(clamp(1.0 - dl / 30.0, 0.0, 1.0), 2.0) * uLight.w;
  float wrap = max(dot(N, toLight / dl) * 0.7 + 0.3, 0.0);
  col += base * vec3(0.75, 0.85, 1.0) * wrap * att * 1.25;
  vec3 Hl = normalize(toLight / dl + V);
  col += vec3(0.75, 0.85, 1.0) * ggx(max(dot(N, Hl), 0.0), rough * rough) * 0.03 * att * wrap;

  // A faint blue rim keeps the outline readable against black water.
  float rim = pow(1.0 - NdotV, 3.0);
  col += rim * (camUnder ? vec3(0.03, 0.06, 0.08) * (0.4 + uLight.w) : vec3(0.02, 0.025, 0.03));
  col += rim * uGlow + uGlow * 0.08;

  vec3 fogged = applyFog(col, vWorld);
  col = camUnder ? mix(col, fogged, uFogAmount) : fogged;
  fragColor = vec4(col, 1.0);
}
)GLSL";

struct Vertex {
  float pos[3];
  float normal[3];
  float tangent[4];
  float uv[2];
  uint16_t joints[4];
  float weights[4];
};

glm::mat4 trs(glm::vec3 t, glm::quat r, glm::vec3 s) {
  return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
}

// Samples one animation channel at time `t` (linear keys, as in this file).
void sampleChannel(const cgltf_animation_channel& ch, float t, float out[4]) {
  const cgltf_animation_sampler* s = ch.sampler;
  cgltf_size n = s->input->count;
  int comps = ch.target_path == cgltf_animation_path_type_rotation ? 4 : 3;
  float t0 = 0, t1 = 0;
  cgltf_size k = 0;
  for (; k + 1 < n; ++k) {
    cgltf_accessor_read_float(s->input, k + 1, &t1, 1);
    if (t1 >= t) break;
  }
  cgltf_accessor_read_float(s->input, k, &t0, 1);
  float a[4] = {0, 0, 0, 1}, b[4] = {0, 0, 0, 1};
  cgltf_accessor_read_float(s->output, k, a, comps);
  cgltf_accessor_read_float(s->output, k + 1 < n ? k + 1 : k, b, comps);
  float f = (t1 > t0) ? glm::clamp((t - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;
  if (comps == 4) {
    glm::quat qa(a[3], a[0], a[1], a[2]), qb(b[3], b[0], b[1], b[2]);
    glm::quat q = glm::slerp(qa, qb, f);
    out[0] = q.x, out[1] = q.y, out[2] = q.z, out[3] = q.w;
  } else {
    for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * f;
  }
}

}  // namespace

glm::mat4 WhaleModel::fileToWhale() {
  // Same placement as the Godot scene: scale 0.50578 (about 14 m long), file -X
  // (the head) becomes -Z (forward), lowered so the body is centred.
  const float s = 0.50578f;
  glm::mat4 m(0.0f);
  m[0] = glm::vec4(0, 0, s, 0);
  m[1] = glm::vec4(0, s, 0, 0);
  m[2] = glm::vec4(-s, 0, 0, 0);
  m[3] = glm::vec4(0, -1.77f, 1.0116f, 1);
  return m;
}

bool WhaleModel::load(const std::string& dir) {
  // Decode the three 4K textures on other threads while we parse the mesh.
  auto baseF = std::async(kDecodeLaunch, decodePNG, dir + "/textures/StingrayPBS1_baseColor.png", 2048);
  auto normF = std::async(kDecodeLaunch, decodePNG, dir + "/textures/StingrayPBS1_normal.png", 2048);
  auto ormF = std::async(kDecodeLaunch, decodePNG, dir + "/textures/StingrayPBS1_metallicRoughness.png", 2048);

  std::string path = dir + "/scene.gltf";
  cgltf_options opt{};
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&opt, path.c_str(), &data) != cgltf_result_success ||
      cgltf_load_buffers(&opt, data, path.c_str()) != cgltf_result_success) {
    std::fprintf(stderr, "Could not load %s\n", path.c_str());
    return false;
  }

  // ---- skeleton
  const cgltf_skin& skin = data->skins[0];
  joints_.resize(skin.joints_count);
  for (cgltf_size j = 0; j < skin.joints_count; ++j) {
    const cgltf_node* n = skin.joints[j];
    Joint& J = joints_[j];
    J.name = n->name ? n->name : "";
    if (n->has_translation) J.t = glm::make_vec3(n->translation);
    if (n->has_rotation) J.r = glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]);
    if (n->has_scale) J.s = glm::make_vec3(n->scale);
    cgltf_accessor_read_float(skin.inverse_bind_matrices, j, glm::value_ptr(J.inverseBind), 16);
    for (cgltf_size p = 0; p < skin.joints_count; ++p)
      if (skin.joints[p] == n->parent) J.parent = (int)p;
    if (J.parent < 0 && n->parent) cgltf_node_transform_world(n->parent, glm::value_ptr(rootParent_));
  }
  // Rest pose in skeleton space (used to find each bone's own axes).
  for (size_t j = 0; j < joints_.size(); ++j) {
    Joint& J = joints_[j];
    glm::mat4 parent = J.parent >= 0 ? joints_[J.parent].restGlobal : rootParent_;
    J.restGlobal = parent * trs(J.t, J.r, J.s);
  }

  // The model's own idle clip is stiff, so we drive the bones ourselves, but its
  // average pose makes a better neutral body than the bind pose (as in Godot).
  if (data->animations_count > 0) {
    const cgltf_animation& anim = data->animations[0];
    float length = 0;
    for (cgltf_size c = 0; c < anim.channels_count; ++c) {
      float mx = 0;
      cgltf_accessor_read_float(anim.channels[c].sampler->input, anim.channels[c].sampler->input->count - 1, &mx, 1);
      length = std::fmax(length, mx);
    }
    const int samples = 16;
    for (cgltf_size c = 0; c < anim.channels_count; ++c) {
      const cgltf_animation_channel& ch = anim.channels[c];
      int j = -1;
      for (cgltf_size k = 0; k < skin.joints_count; ++k)
        if (skin.joints[k] == ch.target_node) j = (int)k;
      if (j < 0) continue;
      glm::vec4 sum(0.0f);
      for (int i = 0; i < samples; ++i) {
        float v[4] = {0, 0, 0, 1};
        sampleChannel(ch, length * i / samples, v);
        glm::vec4 q(v[0], v[1], v[2], v[3]);
        if (ch.target_path == cgltf_animation_path_type_rotation && glm::dot(sum, q) < 0.0f) q = -q;
        sum += q;
      }
      if (ch.target_path == cgltf_animation_path_type_rotation) {
        glm::vec4 q = glm::normalize(sum);
        joints_[j].r = glm::quat(q.w, q.x, q.y, q.z);
      } else if (ch.target_path == cgltf_animation_path_type_translation) {
        joints_[j].t = glm::vec3(sum) / (float)samples;
      }
    }
  }

  for (const char* name : {"Tail_J1_01", "Tail_J2_02", "Tail_J3_03", "Tail_J4_04", "Tail_J5_05"})
    spine_.push_back(find(name));
  head_ = find("Head_J1_06");
  flippers_[0] = find("R_Root_Fin_08");
  flippers_[1] = find("L_Root_Fin_011");

  // ---- mesh
  const cgltf_primitive& prim = data->meshes[0].primitives[0];
  std::vector<Vertex> verts;
  for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
    const cgltf_attribute& at = prim.attributes[a];
    const cgltf_accessor* acc = at.data;
    if (verts.empty()) verts.resize(acc->count);
    for (cgltf_size i = 0; i < acc->count; ++i) {
      Vertex& v = verts[i];
      switch (at.type) {
        case cgltf_attribute_type_position: cgltf_accessor_read_float(acc, i, v.pos, 3); break;
        case cgltf_attribute_type_normal: cgltf_accessor_read_float(acc, i, v.normal, 3); break;
        case cgltf_attribute_type_tangent: cgltf_accessor_read_float(acc, i, v.tangent, 4); break;
        case cgltf_attribute_type_texcoord:
          if (at.index == 0) cgltf_accessor_read_float(acc, i, v.uv, 2);
          break;
        case cgltf_attribute_type_joints: {
          cgltf_uint j[4] = {0, 0, 0, 0};
          cgltf_accessor_read_uint(acc, i, j, 4);
          for (int k = 0; k < 4; ++k) v.joints[k] = (uint16_t)j[k];
          break;
        }
        case cgltf_attribute_type_weights: cgltf_accessor_read_float(acc, i, v.weights, 4); break;
        default: break;
      }
    }
  }
  std::vector<uint32_t> indices(prim.indices->count);
  for (cgltf_size i = 0; i < prim.indices->count; ++i) indices[i] = (uint32_t)cgltf_accessor_read_index(prim.indices, i);
  indexCount_ = (int)indices.size();
  cgltf_free(data);

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &ibo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * 4, indices.data(), GL_STATIC_DRAW);
  auto attr = [](int loc, int n, size_t off) {
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, n, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)off);
  };
  attr(0, 3, offsetof(Vertex, pos));
  attr(1, 3, offsetof(Vertex, normal));
  attr(2, 4, offsetof(Vertex, tangent));
  attr(3, 2, offsetof(Vertex, uv));
  glEnableVertexAttribArray(4);
  glVertexAttribIPointer(4, 4, GL_UNSIGNED_SHORT, sizeof(Vertex), (void*)offsetof(Vertex, joints));
  attr(5, 4, offsetof(Vertex, weights));
  glBindVertexArray(0);

  program_ = compileProgram("whale", kWhaleVS, kWhaleFS);
  shadowProgram_ = compileProgram("whale shadow", std::string("#define SHADOW\n") + kWhaleVS,
                                  std::string("#define SHADOW\n") + kWhaleFS);

  texBase_ = uploadTexture(baseF.get(), true);
  texNormal_ = uploadTexture(normF.get(), false);
  texORM_ = uploadTexture(ormF.get(), false);
  return true;
}

int WhaleModel::find(const char* name) const {
  for (size_t j = 0; j < joints_.size(); ++j)
    if (joints_[j].name == name) return (int)j;
  std::fprintf(stderr, "whale: no bone named %s\n", name);
  return -1;
}

glm::vec3 WhaleModel::localAxis(int j, glm::vec3 axis) const {
  return glm::normalize(glm::inverse(glm::mat3(joints_[j].restGlobal)) * axis);
}

void SwimPose::update(float dt, float speed, float boostSpeed, float turnRate) {
  float targetPower = glm::clamp(speed / boostSpeed, 0.25f, 1.0f);
  power += (targetPower - power) * (1.0f - std::exp(-2.0f * dt));
  float targetTurn = glm::clamp(turnRate / 0.9f, -1.0f, 1.0f);
  turn += (targetTurn - turn) * (1.0f - std::exp(-4.0f * dt));
  float beats = 0.3f + 0.07f * speed;
  phase = std::fmod(phase + 6.2831853f * beats * dt, 6.2831853f);
}

float SwimPose::heave() const { return 0.06f * power * std::sin(phase + 1.5707963f); }

void WhaleModel::pose(const SwimPose& swim, glm::mat4 out[kMaxJoints]) const {
  const glm::vec3 kLength(1, 0, 0), kUp(0, 1, 0), kSide(0, 0, 1);
  const float spineDeg[5] = {3, 5, 9, 13, 16};
  const float turnDeg[5] = {5, 6, 7, 8, 8};
  const float lag = 0.55f;
  std::vector<glm::quat> rot(joints_.size());
  for (size_t j = 0; j < joints_.size(); ++j) rot[j] = joints_[j].r;

  auto bend = [&](int j, float upDown, float sideways) {
    if (j < 0) return;
    rot[j] = rot[j] * glm::angleAxis(upDown, localAxis(j, kSide)) * glm::angleAxis(sideways, localAxis(j, kUp));
  };
  for (size_t i = 0; i < spine_.size(); ++i) {
    float stroke = glm::radians(spineDeg[i]) * swim.power * std::sin(swim.phase - lag * i);
    // Spine bones point towards the tail, so curving into a turn means rotating against it.
    float curve = -glm::radians(turnDeg[i]) * swim.turn;
    bend(spine_[i], stroke, curve);
  }
  // The head counters the stroke a little and leads into turns.
  bend(head_, -glm::radians(2.0f) * swim.power * std::sin(swim.phase + 0.6f), glm::radians(10.0f) * swim.turn);
  // Flippers: a mirrored flap with the stroke, plus opposite tilts to steer.
  float flap = glm::radians(5.0f) * swim.power * std::sin(swim.phase - 0.8f);
  float steer = glm::radians(20.0f) * swim.turn;
  for (int i = 0; i < 2; ++i) {
    int j = flippers_[i];
    if (j < 0) continue;
    float mirror = i == 0 ? 1.0f : -1.0f;
    rot[j] = rot[j] * glm::angleAxis(flap * mirror + steer, localAxis(j, kLength));
  }

  std::vector<glm::mat4> global(joints_.size());
  for (size_t j = 0; j < joints_.size(); ++j) {
    const Joint& J = joints_[j];
    glm::mat4 parent = J.parent >= 0 ? global[J.parent] : rootParent_;
    global[j] = parent * trs(J.t, rot[j], J.s);
    out[j] = global[j] * J.inverseBind;
  }
}

void WhaleModel::draw(const WhaleDraw& d) const {
  glUseProgram(program_);
  glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE, glm::value_ptr(d.world));
  glUniformMatrix4fv(glGetUniformLocation(program_, "uJoints"), (GLsizei)joints_.size(), GL_FALSE,
                     glm::value_ptr(d.joints[0]));
  glUniform1f(glGetUniformLocation(program_, "uFogAmount"), d.fogAmount);
  glUniform3f(glGetUniformLocation(program_, "uGlow"), d.glow.x, d.glow.y, d.glow.z);
  glUniform4f(glGetUniformLocation(program_, "uLight"), d.lightPos.x, d.lightPos.y, d.lightPos.z, d.lightStrength);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texBase_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texNormal_);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, texORM_);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(vao_);
  glDisable(GL_CULL_FACE);  // the material is double-sided
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
  glEnable(GL_CULL_FACE);
}

void WhaleModel::drawShadow(const WhaleDraw& d, const glm::mat4& lightViewProj) const {
  glUseProgram(shadowProgram_);
  glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uModel"), 1, GL_FALSE, glm::value_ptr(d.world));
  glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uJoints"), (GLsizei)joints_.size(), GL_FALSE,
                     glm::value_ptr(d.joints[0]));
  glUniformMatrix4fv(glGetUniformLocation(shadowProgram_, "uLightViewProj"), 1, GL_FALSE,
                     glm::value_ptr(lightViewProj));
  glBindVertexArray(vao_);
  glDisable(GL_CULL_FACE);
  glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
  glEnable(GL_CULL_FACE);
}
