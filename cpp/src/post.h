#pragma once
#include "gl.h"
#include "render_util.h"

struct PostSettings {
  float exposure = 1.0f;
  float fade = 0.0f;
  float underwater = 0.0f;
};

class Post {
 public:
  void init();
  void resize(int w, int h);
  void run(const RenderTarget& scene, const RenderTarget& out, const PostSettings& s);

 private:
  static constexpr int kLevels = 5;
  RenderTarget bloom_[kLevels];
  GLuint downProg_ = 0, upProg_ = 0, compositeProg_ = 0;
};
