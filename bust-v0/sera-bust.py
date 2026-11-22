#!/usr/bin/env python3
"""Sera-vice bust v0 — low-poly chibi bust of Sera with a face window for the
LOLIN S3 Mini Pro (ESP32-S3, 0.85\" 128x128 TFT).

Pure stdlib: generates a binary STL. No Blender / OpenSCAD needed to build it;
run `python3 sera-bust.py` and slice `sera-bust-v0.stl`.

Design (mm, Z-up, face looks toward +Y):
  * Plinth + shoulders + neck + big chibi head, pearl headband, ears.
  * Face is flattened to a pad; a 17x17 mm window is cut in it so the TFT
    (128x128, ~15.3 mm viewable square) shows through with ~1 mm margin.
  * Behind the window is an open tube (inner 26 x 36 mm) that the board
    (D1-mini footprint ~34.3 long x 25.4 wide) slides into from the back.
    A front frame inside the tube stops the board so the screen sits just
    behind the window; the USB-C cable trails out the rear slot with the
    board (plug the cable in before sliding the board home).
  * Board orientation: screen facing +Y (out the window), USB-C edge down.
    The 128x128 screen is square, so rotating the board 90 deg in its plane
    is also fine if the cable routes better that way.

Tune at the top. Measure the real board/screen with calipers before
printing v1 — WINDOW and POCKET numbers below are from the D1-mini
footprint (34.3 x 25.4 mm) and the 0.85\" diagonal (viewable ~15.3 mm
square), not from a measurement of Marcus's exact board.
"""
import math
import struct

# ---------------------------------------------------------------- parameters
HEAD_R = 32.0          # head sphere radius
HEAD_C = (0.0, 0.0, 62.0)   # head centre (x, y, z)
FACE_Y = 24.0          # front pad plane (head front clamps to this)
WINDOW = 17.0          # face window, square (screen viewable ~15.3 + margin)
BOARD_W = 25.4         # LOLIN S3 Mini Pro width (D1-mini footprint)
BOARD_L = 34.3         # LOLIN S3 Mini Pro length
POCKET_W = 26.0        # tube inner width  (BOARD_W + ~0.6 clearance)
POCKET_H = 36.0        # tube inner height (BOARD_L + ~1.7 clearance)
WALL = 2.0             # tube wall thickness
TUBE_Y0, TUBE_Y1 = -34.0, 22.0   # tube runs back -> just behind face pad

OUT = "sera-bust-v0.stl"

# ---------------------------------------------------------------- mesh kit
tris = []  # list of (v0, v1, v2), verts are (x, y, z) tuples


def add_tri(a, b, c):
    tris.append((a, b, c))


def add_quad(a, b, c, d):
    add_tri(a, b, c)
    add_tri(a, c, d)


def add_box(x0, x1, y0, y1, z0, z1):
    """Axis-aligned box, outward-facing triangles."""
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    # bottom, top, front(y0), back(y1), left(x0), right(x1)
    add_quad(v[3], v[2], v[1], v[0])
    add_quad(v[4], v[5], v[6], v[7])
    add_quad(v[0], v[1], v[5], v[4])
    add_quad(v[2], v[3], v[7], v[6])
    add_quad(v[0], v[4], v[7], v[3])
    add_quad(v[1], v[2], v[6], v[5])


def icosphere(freq):
    """Unit icosphere verts/faces, subdivided `freq` times."""
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
             (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
             (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    verts = [tuple(x / math.sqrt(sum(c * c for c in v)) for x in v) for v in verts]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
             (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
             (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
             (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    cache = {}

    def mid(i, j):
        key = (min(i, j), max(i, j))
        if key in cache:
            return cache[key]
        a, b = verts[i], verts[j]
        m = tuple((a[k] + b[k]) / 2.0 for k in range(3))
        n = math.sqrt(sum(c * c for c in m))
        verts.append(tuple(c / n for c in m))
        cache[key] = len(verts) - 1
        return cache[key]

    for _ in range(freq):
        new = []
        for a, b, c in faces:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            new += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = new
    return verts, faces


def add_sphere(center, r, scale=(1, 1, 1), freq=2, keep=None, squash_y=None):
    """Add a (possibly scaled, face-filtered) sphere.

    keep: fn(cx, cy, cz) -> bool on triangle centroid; False drops the tri.
    squash_y: if set, clamp y to this value (flattens the front into a pad).
    """
    verts, faces = icosphere(freq)
    cx, cy, cz = center
    sx, sy, sz = scale
    for a, b, c in faces:
        pts = []
        for i in (a, b, c):
            x, y, z = verts[i]
            x, y, z = x * r * sx + cx, y * r * sy + cy, z * r * sz + cz
            if squash_y is not None and y > squash_y:
                y = squash_y
            pts.append((x, y, z))
        if keep is not None:
            mx = sum(p[0] for p in pts) / 3
            my = sum(p[1] for p in pts) / 3
            mz = sum(p[2] for p in pts) / 3
            if not keep(mx, my, mz):
                continue
        add_tri(*pts)


def add_cylinder(cx, cy, z0, z1, r, seg=48):
    for i in range(seg):
        a0 = 2 * math.pi * i / seg
        a1 = 2 * math.pi * (i + 1) / seg
        p0 = (cx + r * math.cos(a0), cy + r * math.sin(a0), z0)
        p1 = (cx + r * math.cos(a1), cy + r * math.sin(a1), z0)
        p2 = (cx + r * math.cos(a1), cy + r * math.sin(a1), z1)
        p3 = (cx + r * math.cos(a0), cy + r * math.sin(a0), z1)
        add_quad(p0, p1, p2, p3)
        add_tri((cx, cy, z0), p1, p0)
        add_tri((cx, cy, z1), p3, p2)


def add_torus(center, R, r, tilt_deg=0, seg=28, sides=10):
    cx, cy, cz = center
    tilt = math.radians(tilt_deg)
    ct, st = math.cos(tilt), math.sin(tilt)
    ring = []
    for i in range(seg + 1):
        u = 2 * math.pi * i / seg
        pts = []
        for j in range(sides + 1):
            v = 2 * math.pi * j / sides
            # torus in XY plane, then tilt about X
            x = (R + r * math.cos(v)) * math.cos(u)
            y = (R + r * math.cos(v)) * math.sin(u)
            z = r * math.sin(v)
            y2 = y * ct - z * st
            z2 = y * st + z * ct
            pts.append((cx + x, cy + y2, cz + z2))
        ring.append(pts)
    for i in range(seg):
        for j in range(sides):
            add_quad(ring[i][j], ring[i + 1][j], ring[i + 1][j + 1], ring[i][j + 1])


# ---------------------------------------------------------------- parts
# plinth
add_cylinder(0, 0, 0, 5, 44, seg=48)

# shoulders: flattened sphere
add_sphere((0, 0, 20), 30, scale=(1.45, 1.05, 0.6), freq=2)

# neck
add_cylinder(0, 0, 34, 52, 11, seg=24)

# head: freq 3 for a cleaner window edge; flatten front to a pad; cut the
# face window and a rear loading slot.
hw = WINDOW / 2.0
hc = HEAD_C[2]


def head_keep(x, y, z):
    if y > FACE_Y - 0.5 and abs(x) < hw and abs(z - hc) < hw:
        return False            # face window
    if y < -18 and abs(x) < POCKET_W / 2 and 44 < z < 80:
        return False            # rear slot for the board
    return True


add_sphere(HEAD_C, HEAD_R, freq=3, keep=head_keep, squash_y=FACE_Y)

# ears
add_sphere((-30.5, 0, 60), 5.5, freq=1)
add_sphere((30.5, 0, 60), 5.5, freq=1)

# hair: bigger sphere, shifted up/back; face band cut out, front strands kept
def hair_keep(x, y, z):
    if y > 6 and abs(x) < 25 and 58 < z < 78:
        return False            # face opening (bangs end ~z=78)
    return True


add_sphere((0, -3, 64), 33.5, scale=(1.02, 1.05, 1.0), freq=2, keep=hair_keep)

# pearl headband: torus + pearls along the front arc
add_torus((0, 0, 74), 32.5, 2.4, tilt_deg=-8)
for k in range(9):
    ang = math.radians(-50 + k * 12.5)
    px = 32.5 * math.sin(ang)
    py = 32.5 * math.cos(ang)
    pz = 74 - 8 * 0.0 + (-math.sin(math.radians(-8)) * py * 0 + 0)
    # tilt the pearl ring to match the torus tilt about X
    tilt = math.radians(-8)
    py2 = py * math.cos(tilt)
    pz2 = 74 + py * math.sin(tilt)
    add_sphere((px, py2, pz2), 2.6, freq=1)

# board pocket tube: 4 walls, open front (window side) and back (loading).
# inner 26 x 36, outer +WALL. Front stop frame keeps the board's screen
# just behind the window; rear flange frames the loading slot.
x0, x1 = -POCKET_W / 2 - WALL, POCKET_W / 2 + WALL
z0, z1 = 62 - POCKET_H / 2, 62 + POCKET_H / 2
add_box(x0, x1, TUBE_Y0, TUBE_Y1, z1 - WALL, z1)            # top wall
add_box(x0, x1, TUBE_Y0, TUBE_Y1, z0, z0 + WALL)            # bottom wall
add_box(x0, x0 + WALL, TUBE_Y0, TUBE_Y1, z0, z1)            # left wall
add_box(x1 - WALL, x1, TUBE_Y0, TUBE_Y1, z0, z1)            # right wall

# front stop frame (y 20..22), opening 18x18 so no screen pixels are covered
fw = 9.0
add_box(-POCKET_W / 2 - WALL, POCKET_W / 2 + WALL, 20, 22, 62 + fw, z1)
add_box(-POCKET_W / 2 - WALL, POCKET_W / 2 + WALL, 20, 22, z0, 62 - fw)
add_box(-POCKET_W / 2 - WALL, -fw, 20, 22, 62 - fw, 62 + fw)
add_box(fw, POCKET_W / 2 + WALL, 20, 22, 62 - fw, 62 + fw)

# rear flange frame (y -36..-34), opening = pocket, flange to 34 x 44
add_box(-17, 17, -36, -34, 80, 84)
add_box(-17, 17, -36, -34, 40, 44)
add_box(-17, -13, -36, -34, 44, 80)
add_box(13, 17, -36, -34, 44, 80)

# ---------------------------------------------------------------- write STL
def normal(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1],
         u[2] * v[0] - u[0] * v[2],
         u[0] * v[1] - u[1] * v[0]]
    ln = math.sqrt(sum(x * x for x in n)) or 1.0
    return [x / ln for x in n]


good = []
for a, b, c in tris:
    n = normal(a, b, c)
    # drop degenerate triangles (zero area)
    ux, uy, uz = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    vx, vy, vz = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    cx = uy * vz - uz * vy
    cy = uz * vx - ux * vz
    cz = ux * vy - uy * vx
    if math.sqrt(cx * cx + cy * cy + cz * cz) < 1e-9:
        continue
    good.append((n, a, b, c))

with open(OUT, "wb") as f:
    f.write(b"\0" * 80)
    f.write(struct.pack("<I", len(good)))
    for n, a, b, c in good:
        f.write(struct.pack("<12fH", *n, *a, *b, *c, 0))

xs = [p[0] for _, a, b, c in good for p in (a, b, c)]
ys = [p[1] for _, a, b, c in good for p in (a, b, c)]
zs = [p[2] for _, a, b, c in good for p in (a, b, c)]
print(f"wrote {OUT}: {len(good)} triangles")
print(f"bounds X {min(xs):.1f}..{max(xs):.1f}  "
      f"Y {min(ys):.1f}..{max(ys):.1f}  Z {min(zs):.1f}..{max(zs):.1f} mm")
print(f"size: {max(xs)-min(xs):.1f} x {max(ys)-min(ys):.1f} x "
      f"{max(zs)-min(zs):.1f} mm")
