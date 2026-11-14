// 52 Hertz: a C++/OpenGL rebuild of the Godot game.
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "game.h"
#include "gl.h"
#include "stb_image_write.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

struct Options {
  std::string screenshot;      // write a PNG here and quit
  int frames = 240;            // ... after this many frames
  std::vector<int> shots;      // or at each of these frames (out_NNNN.png)
  std::string scenario = "tour";
  int width = 1280, height = 720;
  long seed = -1;
  int benchmark = 0;           // frames to time with vsync off
  bool hideHud = false;
};

void usage() {
  std::printf(
      "52hertz [options]\n"
      "  --screenshot FILE.png   run a scripted scenario and save the last frame\n"
      "  --frames N              how many frames to run (default 240, at 60 fps)\n"
      "  --shots A,B,C           save several frames: FILE_A.png, FILE_B.png, ...\n"
      "  --scenario NAME         tour | echo | click | breach | surface | deep | pod | breath | autopilot\n"
      "  --seed N                maze seed (default: random; scripted runs use 7)\n"
      "  --size WxH              window / image size (default 1280x720)\n"
      "  --benchmark N           time N frames of the scenario with vsync off\n"
      "  --no-hud                hide the HUD\n");
}

// Scripted input: what the player "does" on each frame of a scenario.
struct Scenario {
  std::function<void(Game&)> setup;
  std::function<void(int frame, InputState&, Game&)> input;
};

Scenario makeScenario(const std::string& name) {
  auto place = [](Game& g, glm::vec3 pos, float yaw, float pitch) {
    Whale& w = g.whale();
    w.pos = pos;
    w.yaw = w.targetYaw = yaw;
    w.pitch = w.targetPitch = pitch;
    w.vel = w.forward() * w.speed;
    w.underwater = pos.y < 0.0f;
    g.snapCamera();
  };
  auto hold = [](int frame, int from, int to) { return frame >= from && frame < to; };
  const float east = -1.5707963f;

  if (name == "echo")  // a long call down a corridor at 14 m
    return {[=](Game& g) { place(g, {0, -14, 0}, east, 0.0f); },
            [=](int f, InputState& in, Game&) {
              in.echo = hold(f, 20, 95);
              in.slow = f > 95;
              in.mouseDx = f > 100 && f < 160 ? 1.5f : 0.0f;
            }};
  if (name == "click")  // a few quick clicks all around
    return {[=](Game& g) { place(g, {0, -12, 0}, east, 0.0f); },
            [=](int f, InputState& in, Game&) {
              in.echo = hold(f, 10, 13) || hold(f, 70, 73) || hold(f, 130, 133);
              in.slow = true;
            }};
  if (name == "breach")  // swim up hard and leap out
    return {[=](Game& g) { place(g, {0, -14, 0}, east, glm::radians(50.0f)); },
            [=](int f, InputState& in, Game&) {
              in.boost = true;
              in.mouseDy = f < 30 ? -2.0f : 0.0f;
            }};
  if (name == "surface")  // cruising along the top, looking at the cliffs
    return {[=](Game& g) { place(g, {-5, -1.2f, 0}, east, 0.0f); },
            [=](int f, InputState& in, Game&) {
              in.slow = true;
              in.mouseDy = f < 20 ? 1.0f : 0.0f;
            }};
  if (name == "deep")  // dive to the bottom and call
    return {[=](Game& g) { place(g, {0, -20, 0}, east, glm::radians(-45.0f)); },
            [=](int f, InputState& in, Game&) {
              in.mouseDy = f < 60 ? 0.0f : (f < 120 ? -1.2f : 0.0f);
              in.echo = hold(f, 150, 225);
              in.slow = f > 150;
            }};
  if (name == "pod")  // just past the exit, about to meet the pod
    return {[=](Game& g) {
              glm::vec3 p = g.level().exitGap + glm::vec3(0, -2, 20);
              place(g, p, 0.0f, 0.0f);
            },
            [=](int f, InputState& in, Game&) { in.echo = hold(f, 10, 50); }};
  if (name == "breath")  // run out of air and bounce back
    return {[=](Game& g) {
              place(g, {0, -14, 0}, east, 0.0f);
              g.setBreath(0.02f);
            },
            [=](int f, InputState& in, Game&) { in.slow = true; }};
  if (name == "skim")  // camera right at the waterline
    return {[=](Game& g) { place(g, {0, -3.4f, 0}, east, 0.0f); },
            [=](int f, InputState& in, Game&) { in.slow = true; }};
  if (name == "wall")  // ram the end wall of the first corridor at full speed
    return {[=](Game& g) { place(g, {40, -12, 0}, east, 0.0f); },
            [=](int f, InputState& in, Game&) {
              in.boost = true;
              in.echo = hold(f, 250, 254);
            }};
  if (name == "autopilot")  // swim the shortest route through the maze to the pod
    return {[=](Game& g) {},
            [=](int f, InputState& in, Game& g) {
              static size_t next = 1;
              const auto& route = g.level().route;
              Whale& w = g.whale();
              if (f == 1) next = 1;
              while (next + 1 < route.size() &&
                     glm::length(glm::vec2(route[next].x - w.pos.x, route[next].z - w.pos.z)) < 14.0f)
                ++next;
              glm::vec3 to = route[std::min(next, route.size() - 1)] - w.pos;
              float want = std::atan2(-to.x, -to.z);
              float diff = std::remainder(want - w.targetYaw, 6.2831853f);
              in.mouseDx = -std::clamp(diff, -0.04f, 0.04f) / 0.0025f;
              // Hold a cruising depth of about 10 m.
              float pitchWant = std::clamp((-10.0f - w.pos.y) * 0.05f, -0.4f, 0.4f);
              in.mouseDy = -std::clamp(pitchWant - w.targetPitch, -0.02f, 0.02f) / 0.0025f;
              in.boost = std::fabs(diff) < 0.3f;
              in.echo = f % 300 >= 200 && f % 300 < 272;  // a long call every 5 s
              // Come up for air now and then.
              if (g.breath() < 0.35f) in.mouseDy = -std::clamp(0.6f - w.targetPitch, -0.03f, 0.03f) / 0.0025f;
            }};
  // "tour": swim the first corridor, click, then a long call ahead.
  return {[=](Game& g) { place(g, {0, -8, 0}, east, 0.0f); },
          [=](int f, InputState& in, Game&) {
            in.echo = hold(f, 30, 34) || hold(f, 120, 195);
            in.boost = hold(f, 40, 110);
            in.mouseDy = hold(f, 40, 70) ? 0.6f : 0.0f;
          }};
}

void savePNG(const RenderTarget& rt, const std::string& path) {
  std::vector<unsigned char> px((size_t)rt.width * rt.height * 4);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, rt.fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, rt.width, rt.height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  // OpenGL's rows go bottom-up; PNG's go top-down. Also drop alpha.
  std::vector<unsigned char> rgb((size_t)rt.width * rt.height * 3);
  for (int y = 0; y < rt.height; ++y)
    for (int x = 0; x < rt.width; ++x)
      for (int c = 0; c < 3; ++c)
        rgb[((size_t)y * rt.width + x) * 3 + c] = px[((size_t)(rt.height - 1 - y) * rt.width + x) * 4 + c];
  if (stbi_write_png(path.c_str(), rt.width, rt.height, 3, rgb.data(), rt.width * 3))
    std::printf("saved %s\n", path.c_str());
  else
    std::fprintf(stderr, "could not write %s\n", path.c_str());
}

std::string shotName(const std::string& base, int frame) {
  std::string stem = base, ext = ".png";
  if (base.size() > 4 && base.substr(base.size() - 4) == ".png") stem = base.substr(0, base.size() - 4);
  char buf[16];
  std::snprintf(buf, sizeof buf, "_%04d", frame);
  return stem + buf + ext;
}

// ---- interactive input
struct WindowState {
  bool captured = true;
  bool restart = false;
  bool fullscreen = false;
  int windowedX = 100, windowedY = 100, windowedW = 1280, windowedH = 720;
  double lastX = 0, lastY = 0;
  bool haveLast = false;
};

void keyCallback(GLFWwindow* win, int key, int, int action, int) {
  auto* st = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
  if (action != GLFW_PRESS) return;
  if (key == GLFW_KEY_ESCAPE) {
    st->captured = false;
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  } else if (key == GLFW_KEY_R) {
    st->restart = true;
  } else if (key == GLFW_KEY_F11) {
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    if (!st->fullscreen && mon) {
      glfwGetWindowPos(win, &st->windowedX, &st->windowedY);
      glfwGetWindowSize(win, &st->windowedW, &st->windowedH);
      const GLFWvidmode* mode = glfwGetVideoMode(mon);
      glfwSetWindowMonitor(win, mon, 0, 0, mode->width, mode->height, mode->refreshRate);
      st->fullscreen = true;
    } else {
      glfwSetWindowMonitor(win, nullptr, st->windowedX, st->windowedY, st->windowedW, st->windowedH, 0);
      st->fullscreen = false;
    }
  }
}

void mouseButtonCallback(GLFWwindow* win, int, int action, int) {
  auto* st = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
  if (action == GLFW_PRESS && !st->captured) {
    st->captured = true;
    st->haveLast = false;
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }
}

[[maybe_unused]] void APIENTRY glDebug(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* msg, const void*) {
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
  if (type == GL_DEBUG_TYPE_ERROR) std::fprintf(stderr, "GL error: %s\n", msg);
}

}  // namespace

// One frame of the game in a window: input, update, draw. The desktop calls it in a
// loop; a browser calls it once per animation frame (see main()).
struct FrameContext {
  GLFWwindow* win;
  Game* game;
  WindowState* st;
  std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
  double fpsTimer = 0.0;
  int fpsFrames = 0;
};

static void runFrame(void* arg) {
  FrameContext& c = *static_cast<FrameContext*>(arg);
  GLFWwindow* win = c.win;
  Game& game = *c.game;
  WindowState& st = *c.st;
  int fbw = 0, fbh = 0;
  glfwPollEvents();
  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - c.last).count();
  c.last = now;
  dt = std::min(dt, 1.0f / 20.0f);

  InputState in;
  double mx, my;
  glfwGetCursorPos(win, &mx, &my);
  if (st.captured && st.haveLast) {
    in.mouseDx = (float)(mx - st.lastX);
    in.mouseDy = (float)(my - st.lastY);
  }
  st.lastX = mx;
  st.lastY = my;
  st.haveLast = true;
  in.mouseCaptured = st.captured;
  in.boost = glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
             glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS;
  in.slow = glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS;
  in.echo = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
  in.restart = st.restart;
  st.restart = false;

  glfwGetFramebufferSize(win, &fbw, &fbh);
  if (fbw > 0 && fbh > 0) {
    game.resize(fbw, fbh);
    game.update(dt, in);
    game.render();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, game.output().fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, game.output().width, game.output().height, 0, 0, fbw, fbh, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
  }
  glfwSwapBuffers(win);

  c.fpsTimer += dt;
  c.fpsFrames++;
  if (c.fpsTimer >= 1.0) {
    char title[128];
    std::snprintf(title, sizeof title, "52 Hertz - %d fps", c.fpsFrames);
    glfwSetWindowTitle(c.win, title);
    c.fpsTimer = 0.0;
    c.fpsFrames = 0;
  }
}

// Shared builds keep their assets next to the program, so start there.
static void startInProgramFolder(const char* argv0) {
#ifdef PORTABLE_ASSETS
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path exe = fs::read_symlink("/proc/self/exe", ec);  // Linux
  if (ec) exe = fs::absolute(argv0, ec);                 // Windows and others
  if (!ec) fs::current_path(exe.parent_path(), ec);
#else
  (void)argv0;
#endif
}

int main(int argc, char** argv) {
  startInProgramFolder(argv[0]);
  Options opt;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        usage();
        std::exit(1);
      }
      return argv[++i];
    };
    if (a == "--screenshot") opt.screenshot = next();
    else if (a == "--frames") opt.frames = std::atoi(next().c_str());
    else if (a == "--scenario") opt.scenario = next();
    else if (a == "--seed") opt.seed = std::atol(next().c_str());
    else if (a == "--benchmark") opt.benchmark = std::atoi(next().c_str());
    else if (a == "--no-hud") opt.hideHud = true;
    else if (a == "--shots") {
      std::string list = next();
      for (size_t p = 0; p < list.size();) {
        size_t q = list.find(',', p);
        opt.shots.push_back(std::atoi(list.substr(p, q - p).c_str()));
        if (q == std::string::npos) break;
        p = q + 1;
      }
    } else if (a == "--size") {
      std::string s = next();
      std::sscanf(s.c_str(), "%dx%d", &opt.width, &opt.height);
    } else {
      usage();
      return a == "--help" || a == "-h" ? 0 : 1;
    }
  }
  bool scripted = !opt.screenshot.empty() || opt.benchmark > 0;
  if (!opt.shots.empty()) {
    int last = 0;
    for (int f : opt.shots) last = std::max(last, f);
    opt.frames = last;
  }
  if (opt.benchmark > 0) opt.frames = opt.benchmark;

  if (!glfwInit()) {
    std::fprintf(stderr, "Could not start GLFW\n");
    return 1;
  }
#ifdef __EMSCRIPTEN__
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);  // WebGL 2
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
#ifdef HERTZ_GL41
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);  // the newest OpenGL macOS has
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#endif
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
  if (scripted) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // render offscreen, don't pop up a window
  GLFWwindow* win = glfwCreateWindow(opt.width, opt.height, "52 Hertz", nullptr, nullptr);
  if (!win) {
    std::fprintf(stderr, "Could not open an OpenGL window (this game needs OpenGL 4.5, or 4.1 on macOS)\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(win);
  if (const char* missing = loadGL()) {
    std::fprintf(stderr, "OpenGL function %s is missing\n", missing);
    return 1;
  }
#ifndef HERTZ_GL41
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(glDebug, nullptr);
#endif
  std::printf("OpenGL %s on %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
  glfwSwapInterval(opt.benchmark > 0 ? 0 : 1);

  unsigned seed = opt.seed >= 0 ? (unsigned)opt.seed : (scripted ? 7u : std::random_device{}());
#ifdef __EMSCRIPTEN__
  static Game game;  // main() never returns in the browser
#else
  Game game;
#endif
  int fbw, fbh;
  glfwGetFramebufferSize(win, &fbw, &fbh);
  if (scripted) fbw = opt.width, fbh = opt.height;  // render offscreen at the requested size
  if (!game.init(fbw, fbh, seed)) return 1;
  game.setHudVisible(!opt.hideHud);

  if (scripted) {
    Scenario sc = makeScenario(opt.scenario);
    sc.setup(game);
    const float dt = 1.0f / 60.0f;
    auto t0 = std::chrono::steady_clock::now();
    for (int f = 1; f <= opt.frames; ++f) {
      InputState in;
      sc.input(f, in, game);
      game.update(dt, in);
      game.render();
      bool shoot = opt.shots.empty() ? (f == opt.frames && !opt.screenshot.empty())
                                     : std::find(opt.shots.begin(), opt.shots.end(), f) != opt.shots.end();
      if (shoot) savePNG(game.output(), opt.shots.empty() ? opt.screenshot : shotName(opt.screenshot, f));
      if (shoot && std::getenv("HERTZ_DUMP_REFLECTION")) savePNG(game.reflection(), shotName(opt.screenshot + ".refl", f));
      if (opt.benchmark > 0 && f % 50 == 0) glFinish();  // keep the GPU from queueing too far ahead
    }
    glFinish();
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (opt.benchmark > 0)
      std::printf("benchmark: %d frames in %.2f s = %.2f ms/frame (%.0f fps) at %dx%d\n", opt.frames, secs,
                  secs * 1000.0 / opt.frames, opt.frames / secs, fbw, fbh);
    std::printf("%s\n", game.status().c_str());
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
  }

#ifdef __EMSCRIPTEN__
  static WindowState st;
#else
  WindowState st;
#endif
  glfwSetWindowUserPointer(win, &st);
  glfwSetKeyCallback(win, keyCallback);
  glfwSetMouseButtonCallback(win, mouseButtonCallback);
#ifdef __EMSCRIPTEN__
  st.captured = false;  // browsers only lock the mouse after a click ("Click to swim")
#else
  glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
#endif

#ifdef __EMSCRIPTEN__
  static FrameContext ctx{win, &game, &st};  // must outlive main() in the browser
#else
  FrameContext ctx{win, &game, &st};
#endif
#ifdef __EMSCRIPTEN__
  // The browser drives the frames; main() never returns, so the game stays alive.
  emscripten_set_main_loop_arg(runFrame, &ctx, 0, true);
#else
  while (!glfwWindowShouldClose(win)) runFrame(&ctx);
#endif
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
