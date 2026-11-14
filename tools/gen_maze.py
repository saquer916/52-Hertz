#!/usr/bin/env python3
"""Generates a cliff-wall maze level for 52 Hertz.

Usage:  python3 tools/gen_maze.py > scenes/level_1.tscn

Every wall is a duplicate sharing one shape, one mesh and the material in
assets/materials/rock_wall.tres, so retexturing that file retextures the level.
Change the numbers below for a different size or layout (try another SEED_START).
"""
import random
import sys
from collections import deque

W, D = 5, 4              # cells across (x) and deep (-z)
C, T, H = 50.0, 5.0, 90.0  # cell size, wall thickness, wall height (metres)
WALL_Y = -15.0           # walls span -60 (seafloor) to +30 (above the water)
START = (2, 0)           # the whale starts at (0, -10, 0), the centre of this cell
STRAIGHT_BIAS = 0.65     # chance to keep carving straight: higher = fewer turns
MIN_PATH, MAX_PATH = 8, 12  # length of the shortest route, in cells
SEED_START = 0

DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def carve(seed):
    rng = random.Random(seed)
    open_edges, seen, stack = set(), {START}, [(START, None)]
    while stack:
        (x, z), last = stack[-1]
        options = [d for d in DIRS if 0 <= x + d[0] < W and 0 <= z + d[1] < D and (x + d[0], z + d[1]) not in seen]
        if not options:
            stack.pop()
            continue
        d = last if last in options and rng.random() < STRAIGHT_BIAS else rng.choice(options)
        n = (x + d[0], z + d[1])
        open_edges.add(frozenset(((x, z), n)))
        seen.add(n)
        stack.append((n, d))
    return open_edges


def distances(open_edges):
    dist, queue = {START: 0}, deque([START])
    while queue:
        c = queue.popleft()
        for dx, dz in DIRS:
            n = (c[0] + dx, c[1] + dz)
            if n not in dist and frozenset((c, n)) in open_edges:
                dist[n] = dist[c] + 1
                queue.append(n)
    return dist


def pick_layout():
    for seed in range(SEED_START, SEED_START + 1000):
        edges = carve(seed)
        dist = distances(edges)
        exit_x, steps = max(((x, dist[(x, D - 1)]) for x in range(W)), key=lambda t: t[1])
        # The main scene's whale faces +X at the start, so the start cell must open that way.
        opens_east = frozenset((START, (START[0] + 1, START[1]))) in edges
        if MIN_PATH <= steps <= MAX_PATH and opens_east and exit_x != START[0]:
            return seed, edges, exit_x, steps
    sys.exit("no layout found; loosen MIN_PATH/MAX_PATH")


def cx(x): return (x - START[0]) * C
def cz(z): return -(z - START[1]) * C
def f(v): return f"{v:g}"


def main():
    seed, edges, exit_x, steps = pick_layout()
    print(f"seed={seed} exit_cell=({exit_x},{D - 1}) shortest_path={steps} cells", file=sys.stderr)

    walls = []  # (x, z, runs_along_z)
    for z in range(D + 1):                     # walls running along X, between rows
        for x in range(W):
            if z == D and x == exit_x:
                continue                       # the exit
            if 0 < z < D and frozenset(((x, z - 1), (x, z))) in edges:
                continue
            walls.append((cx(x), cz(z) + C / 2, False))
    for x in range(W + 1):                     # walls running along Z, between columns
        for z in range(D):
            if 0 < x < W and frozenset(((x - 1, z), (x, z))) in edges:
                continue
            walls.append((cx(x) - C / 2, cz(z), True))
    print(f"walls={len(walls)}", file=sys.stderr)

    out = ['[gd_scene format=3]\n',
           '[ext_resource type="Material" path="res://assets/materials/rock_wall.tres" id="1_rock"]\n',
           f'[sub_resource type="BoxShape3D" id="WallShape"]\nsize = Vector3({f(C + T)}, {f(H)}, {f(T)})\n',
           f'[sub_resource type="BoxMesh" id="WallMesh"]\nmaterial = ExtResource("1_rock")\nsize = Vector3({f(C + T)}, {f(H)}, {f(T)})\n',
           '[sub_resource type="BoxShape3D" id="FloorShape"]\nsize = Vector3(600, 2, 600)\n',
           '[sub_resource type="BoxMesh" id="FloorMesh"]\nmaterial = ExtResource("1_rock")\nsize = Vector3(600, 2, 600)\n',
           '[node name="Level1" type="Node3D"]\n']
    mid_x, mid_z = (cx(0) + cx(W - 1)) / 2, (cz(0) + cz(D - 1)) / 2
    out.append(f'[node name="Seafloor" type="StaticBody3D" parent="."]\ntransform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, {f(mid_x)}, -60, {f(mid_z)})\n')
    out.append('[node name="CollisionShape3D" type="CollisionShape3D" parent="Seafloor"]\nshape = SubResource("FloorShape")\n')
    out.append('[node name="MeshInstance3D" type="MeshInstance3D" parent="Seafloor"]\nmesh = SubResource("FloorMesh")\n')
    out.append('[node name="Walls" type="Node3D" parent="."]\n')
    for i, (x, z, along_z) in enumerate(walls):
        name = "Wall" if i == 0 else f"Wall{i + 1}"
        basis = "0, 0, 1, 0, 1, 0, -1, 0, 0" if along_z else "1, 0, 0, 0, 1, 0, 0, 0, 1"
        out.append(f'[node name="{name}" type="StaticBody3D" parent="Walls"]\ntransform = Transform3D({basis}, {f(x)}, {f(WALL_Y)}, {f(z)})\n')
        out.append(f'[node name="CollisionShape3D" type="CollisionShape3D" parent="Walls/{name}"]\nshape = SubResource("WallShape")\n')
        out.append(f'[node name="MeshInstance3D" type="MeshInstance3D" parent="Walls/{name}"]\nmesh = SubResource("WallMesh")\n')
    # Start faces +X (Transform3D below is a -90 degree turn around Y).
    out.append('[node name="Start" type="Marker3D" parent="."]\ntransform = Transform3D(0, 0, -1, 0, 1, 0, 1, 0, 0, 0, -10, 0)\n')
    out.append(f'[node name="PodSpot" type="Marker3D" parent="."]\ntransform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, {f(cx(exit_x))}, -10, {f(cz(D - 1) - C / 2 - 30)})\n')
    sys.stdout.write("\n".join(out))


if __name__ == "__main__":
    main()
