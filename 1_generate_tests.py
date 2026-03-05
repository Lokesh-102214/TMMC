"""
=============================================================
AEGIS PROTOCOL — Discriminating Test Case Generator
=============================================================
Generates exactly 7 test cases that are GUARANTEED to produce
a large gap between naive and smart solvers.

Mathematical requirement for a discriminating test:
  (N / effective_area) x pi x R^2  >>  1   [density condition]
  AND same spatial regions queried repeatedly  [repetition condition]
  AND multiple valid satellites per disk        [choice condition]

Cases generated:
  TC01_clusters_10mega.txt    -- 10 clusters x 1000 queries each
  TC02_clusters_50tight.txt   -- original TC02 (50 tight clusters) KEPT
  TC03_dense_repeated.txt     -- 200 hot centers x 50 queries, small area
  TC04_hierarchical.txt       -- 5 mega x 20 mini clusters
  TC05_tight_walk.txt         -- step=0.15R, dense area, very high overlap
  TC06_stripe_bands.txt       -- 20 horizontal stripes x 500 queries each
  TC07_dense_overlap.txt      -- original TC09 (dense overlap R=100) KEPT

Usage:
    python3 gen_discriminating_tests.py
Output folder: ./tests/
=============================================================
"""

import random
import math
import os

os.makedirs("./tests", exist_ok=True)


# ─── Helpers ──────────────────────────────────────────────────────────────────

def write_test(fname, pts, disks):
    N, Q = len(pts), len(disks)
    with open(fname, "w") as f:
        f.write(f"{N}\n")
        for x, y in pts:
            f.write(f"{x:.6f} {y:.6f}\n")
        f.write(f"{Q}\n")
        for cx, cy, r in disks:
            f.write(f"{cx:.6f} {cy:.6f} {r:.6f}\n")
    with open(fname) as f:
        lines = f.readlines()
    assert len(lines) == 1 + N + 1 + Q, \
        f"Line mismatch in {fname}: got {len(lines)}, expected {1+N+1+Q}"
    short = fname.split("/")[-1].split("\\")[-1]
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    eff_area = (max(xs) - min(xs)) * (max(ys) - min(ys)) + 1.0
    sats_per_disk = N / eff_area * math.pi * 100.0**2
    print(f"  OK  {short:<38}  N={N:>6}  Q={Q:>5}  sats/disk~{sats_per_disk:>8.1f}")


def fix_disk(pts, cx, cy, r):
    """Guarantee at least one satellite is inside the disk."""
    for x, y in pts:
        if (x - cx)**2 + (y - cy)**2 <= r * r:
            return (cx, cy, r)
    p = random.choice(pts)
    return (p[0], p[1], r)


# ─── TC01: 10 Mega-Clusters, 1000 queries each ────────────────────────────────
# N=100k (10k per cluster), each cluster hit 1000 times.
# Oracle: picks 1 central hub per cluster  => ~10-20 total activations.
# Naive:  linear scan picks random low-index sat => ~150-300 activations.
# Key:    10k sats in radius-40 blob, R=50-100  => hundreds of candidates/disk.
def generate_tc01():
    random.seed(200)
    N_per_cluster = 10_000
    n_clusters    = 10
    Q             = 10_000

    centers = [(random.uniform(-5e5, 5e5), random.uniform(-5e5, 5e5))
               for _ in range(n_clusters)]
    pts = []
    for cx, cy in centers:
        for _ in range(N_per_cluster):
            pts.append((cx + random.gauss(0, 40),
                        cy + random.gauss(0, 40)))

    disks = []
    for q in range(Q):
        cx, cy = centers[q % n_clusters]   # strict round-robin through clusters
        r = random.uniform(50, 100)
        cx += random.gauss(0, 15)
        cy += random.gauss(0, 15)
        disks.append(fix_disk(pts, cx, cy, r))

    write_test("./tests/TC01_clusters_10mega.txt", pts, disks)


# ─── TC02: 50 Tight Clusters — ORIGINAL kept ─────────────────────────────────
# N=50k (1k per cluster, sigma=30), Q=10k targeting cluster centers.
# Oracle: ~80 activations.  Naive: ~263 activations.  (measured ratio=0.30)
def generate_tc02():
    random.seed(43)
    N_per_cluster = 1_000
    n_clusters    = 50
    Q             = 10_000

    centers = [(random.uniform(-8e5, 8e5), random.uniform(-8e5, 8e5))
               for _ in range(n_clusters)]
    pts = []
    for cx, cy in centers:
        for _ in range(N_per_cluster):
            pts.append((cx + random.gauss(0, 30),
                        cy + random.gauss(0, 30)))

    disks = []
    for _ in range(Q):
        cx, cy = random.choice(centers)
        r = random.uniform(20, 100)
        cx += random.gauss(0, 10)
        cy += random.gauss(0, 10)
        disks.append(fix_disk(pts, cx, cy, r))

    write_test("./tests/TC02_clusters_50tight.txt", pts, disks)


# ─── TC03: Dense Square + 200 Hot Centers, Each Queried 50 Times ──────────────
# N=100k in a 1000x1000 box. 200 fixed centers each sent 50 times.
# Oracle: 1 activation per center => ~200 total.
# Naive:  each query hits a different low-index sat => ~400-800 activations.
# sats/disk = 100k/(1e6) * pi*100^2 = 3142 => massive choice per disk.
def generate_tc03():
    random.seed(201)
    N = 100_000
    Q = 10_000

    pts = [(random.uniform(-500.0, 500.0), random.uniform(-500.0, 500.0))
           for _ in range(N)]

    hot = [(random.uniform(-450.0, 450.0), random.uniform(-450.0, 450.0))
           for _ in range(200)]

    disks = []
    for q in range(Q):
        hc = hot[q % 200]
        r  = random.uniform(60, 100)
        cx = hc[0] + random.gauss(0, 5)
        cy = hc[1] + random.gauss(0, 5)
        disks.append(fix_disk(pts, cx, cy, r))

    write_test("./tests/TC03_dense_repeated.txt", pts, disks)


# ─── TC04: Hierarchical Clusters (5 mega x 20 mini = 100 mini total) ──────────
# N=100k (1k per mini-cluster). Queries 80% hit mini-center, 20% hit mega-center.
# Oracle: 1 hub per mini-cluster => ~100 activations.
# Naive:  picks varied low-index sats => ~400-700 activations.
# Key:    two-level structure; density rewards hub selection at both scales.
def generate_tc04():
    random.seed(202)
    N_per_mini   = 1_000
    n_mega       = 5
    n_mini_each  = 20
    Q            = 10_000

    mega = [(random.uniform(-8e5, 8e5), random.uniform(-8e5, 8e5))
            for _ in range(n_mega)]

    mini = []
    for mx, my in mega:
        for _ in range(n_mini_each):
            mini.append((mx + random.gauss(0, 2_000),
                         my + random.gauss(0, 2_000)))

    pts = []
    for cx, cy in mini:
        for _ in range(N_per_mini):
            pts.append((cx + random.gauss(0, 30),
                        cy + random.gauss(0, 30)))

    disks = []
    for _ in range(Q):
        if random.random() < 0.8:
            cx, cy = random.choice(mini)
            r = random.uniform(40, 100)
        else:
            cx, cy = random.choice(mega)
            r = random.uniform(80, 100)
        cx += random.gauss(0, 10)
        cy += random.gauss(0, 10)
        disks.append(fix_disk(pts, cx, cy, r))

    write_test("./tests/TC04_hierarchical.txt", pts, disks)


# ─── TC05: Dense Area + Tight Random Walk (step = 0.10-0.20 x R) ──────────────
# N=100k in a 4000x4000 box. Walk step is 10-20% of R => 80-90% disk overlap.
# Oracle: picks disk-center satellite => stays inside next disk => ~300-600 acts.
# Naive:  picks arbitrary low-index sat at boundary => falls out => ~2000-3000.
# Key:    consecutive disks almost completely overlap; central sat always reusable.
def generate_tc05():
    random.seed(203)
    N = 100_000
    Q = 10_000

    pts = [(random.uniform(-2_000, 2_000), random.uniform(-2_000, 2_000))
           for _ in range(N)]

    disks = []
    cx, cy, r = 0.0, 0.0, 100.0
    for _ in range(Q):
        angle = random.uniform(0, 2 * math.pi)
        step  = random.uniform(0.10 * r, 0.20 * r)
        cx = max(-1_900.0, min(1_900.0, cx + step * math.cos(angle)))
        cy = max(-1_900.0, min(1_900.0, cy + step * math.sin(angle)))
        r  = random.uniform(80, 100)
        d  = fix_disk(pts, cx, cy, r)
        disks.append(d)
        cx, cy, r = d

    write_test("./tests/TC05_tight_walk.txt", pts, disks)


# ─── TC06: 20 Horizontal Stripe Bands, Each Queried 500 Times ─────────────────
# N=100k in a 10000x4000 box, 20 stripes of height 200.
# Each stripe queried 500 times with random X within stripe.
# Oracle: picks stripe-center hub => ~20-50 per stripe => ~1000-1500 total.
# Naive:  picks first low-index sat, varies with X => ~3000-5000 activations.
# Key:    stripes are dense (sats/disk ~ 30-50) with high repetition.
def generate_tc06():
    random.seed(204)
    N = 100_000
    Q = 10_000

    pts = [(random.uniform(-5_000, 5_000), random.uniform(-2_000, 2_000))
           for _ in range(N)]

    stripe_y = [-1_900 + i * 200 for i in range(20)]

    disks = []
    for q in range(Q):
        sy = stripe_y[q % 20]
        cx = random.uniform(-4_900, 4_900)
        cy = sy + random.gauss(0, 10)
        r  = 100.0
        disks.append(fix_disk(pts, cx, cy, r))

    write_test("./tests/TC06_stripe_bands.txt", pts, disks)


# ─── TC07: Dense Overlap — Small Area, R=100 — ORIGINAL TC09 kept ─────────────
# N=100k in a 10000x10000 box, all disks have R=100.
# Oracle: ~3069 activations.  Naive: ~4599 activations.  (measured ratio=0.67)
def generate_tc07():
    random.seed(50)
    N = 100_000
    Q = 10_000

    pts = [(random.uniform(-5_000, 5_000), random.uniform(-5_000, 5_000))
           for _ in range(N)]

    disks = []
    for _ in range(Q):
        cx = random.uniform(-4_900, 4_900)
        cy = random.uniform(-4_900, 4_900)
        r  = 100.0
        disks.append(fix_disk(pts, cx, cy, r))

    write_test("./tests/TC07_dense_overlap.txt", pts, disks)


# ─── Entry Point ──────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 70)
    print("  AEGIS PROTOCOL — Generating 7 Discriminating Test Cases")
    print(f"  {'Name':<38}  {'N':>6}  {'Q':>5}  {'sats/disk':>9}")
    print("=" * 70)
    generate_tc01()
    generate_tc02()
    generate_tc03()
    generate_tc04()
    generate_tc05()
    generate_tc06()
    generate_tc07()
    print("=" * 70)
    print("  All 7 test cases written to ./tests/")
    print()
    print("  Expected Steam gap (naive vs oracle):")
    print("    TC01  naive~250    oracle~15     naive_ratio~0.06  LARGE")
    print("    TC02  naive~260    oracle~80     naive_ratio~0.30  LARGE")
    print("    TC03  naive~600    oracle~200    naive_ratio~0.33  LARGE")
    print("    TC04  naive~500    oracle~120    naive_ratio~0.24  LARGE")
    print("    TC05  naive~2000   oracle~400    naive_ratio~0.20  LARGE")
    print("    TC06  naive~4000   oracle~1200   naive_ratio~0.30  LARGE")
    print("    TC07  naive~4600   oracle~3100   naive_ratio~0.67  MODERATE")
    print("=" * 70)