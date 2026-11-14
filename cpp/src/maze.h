#pragma once
#include <map>
#include <set>
#include <utility>

// The maze is a grid of cells. An Edge between two neighbouring cells means
// "there is no wall here, you can swim through".
using Cell = std::pair<int, int>;  // (column x, row z); the grid is Level::kCols x Level::kRows
using Edge = std::pair<Cell, Cell>;

// The four neighbours of a cell: +x, -x, +z, -z.
constexpr int DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

// The same Edge whichever order the two cells are given in.
inline Edge edge(Cell a, Cell b) { return a < b ? Edge{a, b} : Edge{b, a}; }

// Written in my_core.cpp.
std::set<Edge> carve(unsigned seed, Cell start);
std::map<Cell, int> distances(const std::set<Edge>& open, Cell start);
