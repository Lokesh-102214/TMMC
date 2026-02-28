"""
=============================================================
AEGIS PROTOCOL — Test Suite Generator
=============================================================
Generates 10 canonical test cases with diverse distributions.

Usage:
    python3 1_generate_tests.py

Output folder: ./tests/
    tc01_uniform_random.txt
    tc02_clustered_satellites.txt
    tc03_grid_sweep.txt
    tc04_gaussian_tiny_disks.txt
    tc05_ring_distribution.txt
    tc06_adversarial_sparse.txt
    tc07_fractal_sierpinski.txt
    tc08_repeated_hot_centers.txt
    tc09_dense_overlap.txt
    tc10_adversarial_walk.txt

File format (matches judge protocol exactly):
    Line 1        : N
    Lines 2..N+1  : x y   (satellite coordinates)
    Line N+2      : Q
    Lines N+3..end: cx cy r  (disk queries)
=============================================================
"""

import random
import math
import os
from collections import defaultdict

random.seed(42)
os.makedirs("./tests", exist_ok=True)


# ─── Helpers ──────────────────────────────────────────────────────────────────

def write_test(fname, pts, disks):
    """Write test case to file and verify line count integrity."""
    N, Q = len(pts), len(disks)
    with open(fname, 'w') as f:
        f.write(f"{N}\n")
        for x, y in pts:
            f.write(f"{x:.6f} {y:.6f}\n")
        f.write(f"{Q}\n")
        for cx, cy, r in disks:
            f.write(f"{cx:.6f} {cy:.6f} {r:.6f}\n")
    with open(fname) as f:
        lines = f.readlines()
    expected = 1 + N + 1 + Q
    assert len(lines) == expected, f"Line count mismatch: {len(lines)} vs {expected}"
    short = fname.split('/')[-1]
    print(f"  OK  {short:<45}  N={N:>6}, Q={Q:>5}")


def is_covered(pts, cx, cy, r):
    """True if any satellite lies inside or on the disk boundary."""
    for x, y in pts:
        if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
            return True
    return False


def fix_disk(pts, cx, cy, r):
    """
    Guarantee coverage: if no satellite is inside the disk,
    snap the center to a random satellite (keeping radius unchanged).
    The judge guarantees at least one point is always inside; this
    helper enforces that invariant during test generation.
    """
    if is_covered(pts, cx, cy, r):
        return (cx, cy, r)
    p = random.choice(pts)
    return (p[0], p[1], r)


# ─── TC01: Pure Uniform Random ────────────────────────────────────────────────
# Satellites and disk centers both drawn uniformly from [-1e6, 1e6].
# Represents the generic baseline case; Steam will be high.
def generate_tc01():
    random.seed(42)
    N, Q = 100_000, 10_000
    pts = [(random.uniform(-1e6, 1e6), random.uniform(-1e6, 1e6))
           for _ in range(N)]
    disks = []
    for _ in range(Q):
        p = random.choice(pts)
        r = random.uniform(1, 100)
        cx = p[0] + random.uniform(-r * 0.5, r * 0.5)
        cy = p[1] + random.uniform(-r * 0.5, r * 0.5)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc01_uniform_random.txt", pts, disks)


# ─── TC02: Tight Gaussian Clusters ────────────────────────────────────────────
# 50 cluster centers; satellites are Gaussian-distributed around each (σ=30).
# Disks primarily target cluster centers.
# Density heuristic is heavily rewarded here.
def generate_tc02():
    random.seed(43)
    N_per_cluster, n_clusters = 1_000, 50
    centers = [(random.uniform(-8e5, 8e5), random.uniform(-8e5, 8e5))
               for _ in range(n_clusters)]
    pts = []
    for cx, cy in centers:
        for _ in range(N_per_cluster):
            pts.append((cx + random.gauss(0, 30), cy + random.gauss(0, 30)))
    Q = 10_000
    disks = []
    for _ in range(Q):
        cx, cy = random.choice(centers)
        r = random.uniform(20, 100)
        cx += random.gauss(0, 10)
        cy += random.gauss(0, 10)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc02_clustered_satellites.txt", pts, disks)


# ─── TC03: Regular Grid + Left-to-Right Sweeping Disks ────────────────────────
# Satellites on a 300x300 uniform grid (spacing=2000 units).
# Disks sweep deterministically left-to-right, rarely overlapping.
# Adversarial for pure reuse strategies.
def generate_tc03():
    random.seed(44)
    side = 300
    spacing = 2_000.0
    pts = []
    for i in range(side):
        for j in range(side):
            if len(pts) < 90_000:
                pts.append((-300_000 + i * spacing, -300_000 + j * spacing))
    Q = 10_000
    disks = []
    for q in range(Q):
        xi = q % 200
        yi = q // 200
        bx = -300_000 + xi * (side // 200) * spacing
        by = -300_000 + yi * 20 * spacing
        r = random.uniform(30, 100)
        # Find nearest grid point to sweep location
        best = min(pts, key=lambda p: (p[0] - bx) ** 2 + (p[1] - by) ** 2)
        cx = best[0] + random.uniform(-r * 0.3, r * 0.3)
        cy = best[1] + random.uniform(-r * 0.3, r * 0.3)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc03_grid_sweep.txt", pts, disks)


# ─── TC04: Gaussian Satellites + Tiny Disks (R = 1..5) ───────────────────────
# Very small radius means very few satellites per disk.
# Tests correctness under maximum activation pressure.
# Almost every query forces a new satellite activation.
def generate_tc04():
    random.seed(45)
    N, Q = 100_000, 10_000
    pts = [(random.gauss(0, 200_000), random.gauss(0, 200_000))
           for _ in range(N)]
    disks = []
    for _ in range(Q):
        p = random.choice(pts)
        r = random.uniform(1, 5)
        cx = p[0] + random.uniform(-r * 0.5, r * 0.5)
        cy = p[1] + random.uniform(-r * 0.5, r * 0.5)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc04_gaussian_tiny_disks.txt", pts, disks)


# ─── TC05: Ring / Annular Satellite Distribution ──────────────────────────────
# All satellites placed on a thick ring (radius 400k..600k from origin).
# Disks also target ring points. Tests density on curved manifolds.
def generate_tc05():
    random.seed(46)
    N, Q = 80_000, 10_000
    pts = []
    for _ in range(N):
        a = random.uniform(0, 2 * math.pi)
        rv = random.uniform(4e5, 6e5)
        pts.append((rv * math.cos(a), rv * math.sin(a)))
    disks = []
    for _ in range(Q):
        p = random.choice(pts)
        r = random.uniform(50, 100)
        cx = p[0] + random.uniform(-r * 0.4, r * 0.4)
        cy = p[1] + random.uniform(-r * 0.4, r * 0.4)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc05_ring_distribution.txt", pts, disks)


# ─── TC06: Adversarial Sparse — Disks Target Least-Dense Regions ──────────────
# Coarse 50k×50k grid identifies sparsest cells.
# Disks are exclusively sent to these isolated regions.
# Punishes density heuristics that only favour dense zones.
def generate_tc06():
    random.seed(47)
    N, Q = 100_000, 10_000
    pts = [(random.uniform(-1e6, 1e6), random.uniform(-1e6, 1e6))
           for _ in range(N)]
    cell = 50_000
    density_map = defaultdict(list)
    for i, (x, y) in enumerate(pts):
        density_map[(int(x // cell), int(y // cell))].append(i)
    # Sort cells by ascending occupancy (least dense first)
    cells_sorted = sorted(density_map.items(), key=lambda kv: len(kv[1]))
    disks = []
    for q in range(Q):
        cell_entry = cells_sorted[q % max(1, len(cells_sorted) // 4)]
        pidx = random.choice(cell_entry[1])
        p = pts[pidx]
        r = random.uniform(50, 100)
        cx = p[0] + random.uniform(-r * 0.3, r * 0.3)
        cy = p[1] + random.uniform(-r * 0.3, r * 0.3)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc06_adversarial_sparse.txt", pts, disks)


# ─── TC07: Fractal / Sierpinski Triangle (IFS) ────────────────────────────────
# Iterated Function System generates multi-scale fractal distribution.
# Dense pockets exist at every zoom level — challenges grid hashing.
def generate_tc07():
    random.seed(48)
    N, Q = 30_000, 10_000
    pts = []
    x = y = 0.0
    scale = 8e5
    ox, oy = -4e5, -4e5
    # IFS for Sierpinski triangle: 3 contractions
    for i in range(N * 25):
        rv = random.random()
        if rv < 1 / 3:
            x, y = x / 2, y / 2
        elif rv < 2 / 3:
            x, y = x / 2 + 0.5, y / 2
        else:
            x, y = x / 2 + 0.25, y / 2 + math.sqrt(3) / 4
        if i > 100:  # skip initial transient before attractor is reached
            pts.append((
                ox + x * scale + random.gauss(0, 300),
                oy + y * scale + random.gauss(0, 300)
            ))
            if len(pts) >= N:
                break
    disks = []
    for _ in range(Q):
        p = random.choice(pts)
        r = random.uniform(10, 100)
        cx = p[0] + random.uniform(-r * 0.4, r * 0.4)
        cy = p[1] + random.uniform(-r * 0.4, r * 0.4)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc07_fractal_sierpinski.txt", pts, disks)


# ─── TC08: 500 Repeated Hot Centers ───────────────────────────────────────────
# 500 "hot" satellite locations each queried ~20 times.
# Solver that picks a single good hub per hot center dominates.
# Tests active-satellite reuse logic.
def generate_tc08():
    random.seed(49)
    N, Q = 100_000, 10_000
    pts = [(random.uniform(-1e6, 1e6), random.uniform(-1e6, 1e6))
           for _ in range(N)]
    hot = random.choices(pts, k=500)
    disks = []
    for q in range(Q):
        hc = hot[q % 500]
        r = random.uniform(20, 100)
        cx = hc[0] + random.gauss(0, 5)
        cy = hc[1] + random.gauss(0, 5)
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc08_repeated_hot_centers.txt", pts, disks)


# ─── TC09: Dense Packing — Small Area + Large Disks ──────────────────────────
# 100k satellites in a 10k×10k region; all disks have R=100 (max).
# Massive disk overlap; tests how well solver minimises total activations.
def generate_tc09():
    random.seed(50)
    N, Q = 100_000, 10_000
    pts = [(random.uniform(-5_000, 5_000), random.uniform(-5_000, 5_000))
           for _ in range(N)]
    disks = []
    for _ in range(Q):
        cx = random.uniform(-4_900, 4_900)
        cy = random.uniform(-4_900, 4_900)
        r = 100.0
        disks.append(fix_disk(pts, cx, cy, r))
    write_test("./tests/tc09_dense_overlap.txt", pts, disks)


# ─── TC10: Adversarial Random Walk ────────────────────────────────────────────
# Each disk center is a random-walk step from the previous center.
# Step size in [0.5R, 1.8R] → partial overlaps at every step.
# Consecutive disks sometimes share coverage, sometimes do not.
def generate_tc10():
    random.seed(51)
    N, Q = 100_000, 10_000
    pts = [(random.uniform(-1e6, 1e6), random.uniform(-1e6, 1e6))
           for _ in range(N)]
    disks = []
    cx = cy = 0.0
    r = 100.0
    for _ in range(Q):
        angle = random.uniform(0, 2 * math.pi)
        step = random.uniform(r * 0.5, r * 1.8)
        cx = max(-9e5, min(9e5, cx + step * math.cos(angle)))
        cy = max(-9e5, min(9e5, cy + step * math.sin(angle)))
        r = random.uniform(50, 100)
        d = fix_disk(pts, cx, cy, r)
        disks.append(d)
        cx, cy, r = d   # walk continues from confirmed covered center
    write_test("./tests/tc10_adversarial_walk.txt", pts, disks)


# ─── Entry Point ──────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 65)
    print("  AEGIS PROTOCOL — Generating 10 Test Cases")
    print("=" * 65)
    generate_tc01()
    generate_tc02()
    generate_tc03()
    generate_tc04()
    generate_tc05()
    generate_tc06()
    generate_tc07()
    generate_tc08()
    generate_tc09()
    generate_tc10()
    print("=" * 65)
    print("  All 10 test cases written to ./tests/")
    print("  Next step: run  python3 2_build_solvers.sh")
    print("=" * 65)
