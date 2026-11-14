#pragma once
#include "gl.h"
#include "render_util.h"

struct PostSettings {
  float exposure = 1.0f;
  float fade = 0.0f;        // 0 = clear, 1 = black
  float underwater = 0.0f;  // 0..1, adds a gentle wobble and tint
};

// HDR scene -> bloom -> filmic tone mapping -> 8-bit image.
class Post {
 public:
  void init();
  void resize(int w, int h);
  // Renders the final image of `scene` into `out` (which must be RGBA8, same size).
  void run(const RenderTarget& scene, const RenderTarget& out, const PostSettings& s);

 private:
  static constexpr int kLevels = 5;
  RenderTarget bloom_[kLevels];
  GLuint downProg_ = 0, upProg_ = 0, compositeProg_ = 0;
};
