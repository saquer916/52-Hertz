// Core engine pieces, rebuilt by hand: the chase camera, ray and collision tests
// against the maze, and the maze algorithm itself.
//
// Each function starts as a simple PLACEHOLDER that keeps the game running (badly),
// so you can rebuild them one at a time and test as you go. Delete the placeholder
// line when you write your version.

#include <algorithm>
#include <cmath>
#include <deque>
#include <random>

#include "game.h"
#include "maze.h"

// ===========================================================================
// 1. CHASE CAMERA  (~15 min)   Test: just play.
// ===========================================================================
// Members you use: camPos_ (where the camera is), camAim_ (the point it looks at),
// whale_.pos, whale_.basis() (a 3x3 rotation: basis * (0,0,1) is "behind the whale"),
// whale_.forward(), level_.raycast(...), level_.resolveSphere(...).

// Puts the camera straight into place (used at the start and after bounce-back).
void Game::snapCamera() {
  // camPos_ = 12 m behind and 3.5 m above the whale, in the whale's own directions:
  //   whale_.pos + whale_.basis() * glm::vec3(0, 3.5f, 12.0f)
  // camAim_ = 8 m ahead of the whale: whale_.pos + whale_.forward() * 8.0f
  camPos_ = whale_.pos + glm::vec3(0, 6, 20);  // PLACEHOLDER: fixed offset, doesn't turn with the whale
  camAim_ = whale_.pos;
}

// Follows the whale smoothly every frame.
void Game::updateCamera(float dt) {
  // 1. desired = the same "behind and above" point as snapCamera.
  // 2. If a wall is between the whale and `desired`, pull the camera in front of it:
  //      glm::vec3 d = desired - whale_.pos;  float len = glm::length(d);
  //      RayHit hit;
  //      if (len > 0.001f && level_.raycast(whale_.pos, d / len, len, hit))
  //        desired = hit.pos + hit.normal * 0.5f;
  // 3. Ease toward it so it lags a little (smooth): camPos_ += (desired - camPos_) * (1 - exp(-4 * dt));
  // 4. camAim_ = 8 m ahead of the whale.
  snapCamera();  // PLACEHOLDER: snaps every frame (no smoothing, goes through walls)
}

// ===========================================================================
// 2. RAY vs. THE MAZE  (~30 min)   Test: echo dots, and the camera not going through walls.
// ===========================================================================
// walls_ is a list of boxes. Each Box has `lo` (min corner) and `hi` (max corner), and
// every wall lines up with the x/y/z axes, so this is the classic "slab" test.
// A point on the ray is  from + dir * t  (t = distance along the ray, dir is length 1).
// Fill in `hit` for the CLOSEST thing hit within maxDist: hit.t, hit.pos, and hit.normal
// (which way the face it hit points, e.g. (-1,0,0) for a wall's -x side). Return true if anything was hit.
bool Level::raycast(glm::vec3 from, glm::vec3 dir, float maxDist, RayHit& hit) const {
  // For each box:
  //   tmin = 0, tmax = best-so-far (start at maxDist)
  //   For each axis a = 0,1,2:
  //     - if dir[a] is ~0: the ray is parallel to those faces; miss if from[a] is outside lo[a]..hi[a]
  //     - else t0 = (lo[a] - from[a]) / dir[a],  t1 = (hi[a] - from[a]) / dir[a]; swap so t0 < t1
  //       (remember which face that was: entering through the lo face means normal -1 on axis a)
  //     - tmin = max(tmin, t0), tmax = min(tmax, t1); if tmin > tmax: miss
  //   If it hit and tmin is the best so far: remember it, with the normal of the face from the axis
  //   that set tmin.
  // Then the seafloor: a flat plane at y = kFloorY (only if dir.y < 0):
  //   t = (kFloorY - from.y) / dir.y; if 0 < t < best it's the closest hit, normal (0, 1, 0).
  return false;  // PLACEHOLDER: never hits anything (no echo dots yet!)
}

// ===========================================================================
// 3. SPHERE vs. THE MAZE  (~20 min)   Test: swim into a wall (you should stop), and the floor.
// ===========================================================================
// Push a sphere (centre c, radius r) out of every wall it overlaps, and up off the floor.
// `normal` = the sum of the push directions. Return true if anything was touched.
bool Level::resolveSphere(glm::vec3& c, float r, glm::vec3& normal) const {
  normal = glm::vec3(0.0f);
  // For each box b:
  //   q = the closest point in the box to c:  glm::clamp(c, b.lo, b.hi)
  //   d = c - q;  if length(d) >= r: not touching, skip
  //   Otherwise push c out along d by (r - length(d)):  n = d / length(d);  c += n * (r - length(d));  normal += n;
  //   (Bonus: if c is INSIDE the box, length(d) is 0. Push it out through the nearest face instead.)
  // Floor: if c.y < kFloorY + r, set c.y = kFloorY + r and add (0, 1, 0) to normal.
  return false;  // PLACEHOLDER: no collision (you swim through walls)
}

// ===========================================================================
// 4. THE MAZE ALGORITHM  (~30-40 min)   Test: press R for new mazes.
// ===========================================================================
// carve: a "randomised depth-first search" that knocks down walls to make a perfect maze
// (one route between any two cells), preferring to keep going straight so corridors are long.
// Return the set of open Edges.
std::set<Edge> carve(unsigned seed, Cell start) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> random01(0.0f, 1.0f);
  // - seen = {start}; a stack of (cell, direction we came in from, -1 for none), starting with start
  // - while the stack isn't empty:
  //     look at the top cell; find neighbours (use DIRS) that are inside the grid
  //     (0 <= x < Level::kCols, 0 <= z < Level::kRows) and not seen yet
  //     - none: pop it (dead end, back up)
  //     - else pick one: 65% of the time keep the same direction as last time (if that's an option),
  //       otherwise a random option. Open the edge to it, mark it seen, push it.
  //
  // PLACEHOLDER: every wall between cells is open (no maze at all).
  std::set<Edge> open;
  for (int x = 0; x < Level::kCols; ++x)
    for (int z = 0; z < Level::kRows; ++z) {
      if (x + 1 < Level::kCols) open.insert(edge({x, z}, {x + 1, z}));
      if (z + 1 < Level::kRows) open.insert(edge({x, z}, {x, z + 1}));
    }
  return open;
}

// distances: how many cells away every cell is from `start`, walking only through open edges.
// That's a breadth-first search (use std::deque as the queue).
std::map<Cell, int> distances(const std::set<Edge>& open, Cell start) {
  std::map<Cell, int> dist{{start, 0}};
  std::deque<Cell> queue{start};
  // while the queue isn't empty: take the front cell; for each neighbour (DIRS) that isn't in
  // dist yet and has an open edge to this cell: dist[neighbour] = dist[cell] + 1, add it to the queue.
  while (!queue.empty()) {  // PLACEHOLDER: straight-line distance, ignoring walls
    Cell c = queue.front();
    queue.pop_front();
    for (auto& d : DIRS) {
      Cell n{c.first + d[0], c.second + d[1]};
      if (n.first < 0 || n.second < 0 || n.first >= Level::kCols || n.second >= Level::kRows || dist.count(n)) continue;
      dist[n] = std::abs(n.first - start.first) + std::abs(n.second - start.second);
      queue.push_back(n);
    }
  }
  return dist;
}
