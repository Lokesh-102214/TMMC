
# Competition Metric
<img width="920" height="360" alt="image" src="https://github.com/user-attachments/assets/49e4eaa6-8d41-45c3-9209-df4aae292de0" />

# Dummy Meteric
# AEGIS PROTOCOL — Evaluation Suite

Complete self-evaluation toolkit for the Aegis Protocol problem.

---

## Files in This Package

| File | Purpose |
|------|---------|
| `1_generate_tests.py` | Generates all 10 test cases into `./tests/` |
| `oracle_solver.cpp` | Offline oracle — estimates Smin (compile first) |
| `2_build_solvers.sh` | Compiles both `aegis_protocol.cpp` and `oracle_solver.cpp` |
| `3_evaluate.py` | Runs both solvers, validates, computes scores |
| `aegis_protocol.cpp` | **YOUR SOLVER** — place in the same folder |

---

## Step-by-Step Workflow

```bash
# 1. Place your solution here
cp /path/to/your/aegis_protocol.cpp .

# 2. Generate the 10 test cases (~2 minutes, creates ./tests/)
python3 1_generate_tests.py

# 3. Compile both solvers
chmod +x 2_build_solvers.sh
./2_build_solvers.sh

# 4. Run evaluation
python3 3_evaluate.py
```

Optional flags for `3_evaluate.py`:
```bash
python3 3_evaluate.py \
    --your-solver ./bin/aegis \   # path to your binary
    --oracle      ./bin/oracle \  # path to oracle binary
    --tests-dir   ./tests \       # path to test cases
    --time-limit  30.0 \          # seconds per test case
    --max-pts     5.0             # points per test case
```

---

## Understanding Smin — The Critical Concept

### In the Real Competition
```
Smin = minimum Steam achieved by ANY team
Score = (Smin / your_Steam) × max_points_per_case
```
You only know Smin **after** all teams submit. It's unknowable in advance.

### What This Suite Uses Instead
We use an **Offline Oracle** — a solver that "cheats" by reading
all Q disk queries **before** answering any of them, then uses a
**Future Frequency Greedy** strategy:

```
For each satellite i:
    future_freq[i] = number of disks that contain satellite i

For each disk (in order):
    if any active satellite is inside → reuse it (free)
    else → activate the candidate with highest future_freq
```

This gives a **theoretical lower bound** on activations — impossible
for any online algorithm to beat.

### The Relationship

```
oracle_Steam  ≤  best_online_Steam  ≤  your_Steam
     ↑                                      ↑
  = Smin_estimate              = Steam shown in report
```

So your self-evaluated ratio `oracle_Steam / your_Steam` is
**pessimistic** — your actual competition score will be **higher**
because real Smin ≥ oracle_Steam (other online teams also can't do
better than the oracle).

### Practical Interpretation

| Ratio | Meaning |
|-------|---------|
| 1.00 | Your solver == oracle (you matched the theoretical best) |
| 0.95–0.99 | Excellent — very close to optimal |
| 0.85–0.95 | Good — density heuristic working well |
| < 0.85 | Room for improvement in selection strategy |

---

## The 10 Test Cases

| # | Name | Distribution | Challenge |
|---|------|-------------|-----------|
| TC01 | Uniform Random | Uniform `[-1e6, 1e6]` | Baseline; high Steam |
| TC02 | Clustered Satellites | 50 Gaussian clusters (σ=30) | Density reward |
| TC03 | Grid Sweep | 300×300 regular grid | Adversarial sweep |
| TC04 | Gaussian Tiny Disks | Gaussian pts, R=1–5 | Correctness pressure |
| TC05 | Ring Distribution | Annular ring r∈[4e5,6e5] | Curved manifold |
| TC06 | Adversarial Sparse | Uniform; disks target voids | Anti-density test |
| TC07 | Fractal Sierpinski | IFS attractor + noise | Multi-scale density |
| TC08 | Repeated Hot Centers | 500 hubs × 20 queries | Reuse test |
| TC09 | Dense Overlap | 100k pts in 10k×10k, R=100 | Maximum overlap |
| TC10 | Adversarial Walk | Random walk, step∈[0.5R,1.8R] | Partial overlaps |

---

## System Requirements

- **Python 3.8+**
- **g++ with C++17 support** (`g++ --version` to check)
- ~500 MB disk space for test cases
- ~2–5 minutes to generate test cases

---

## Expected Output (sample)

```
======================================================================
  AEGIS PROTOCOL — Evaluation Report
  Your Solver : ./bin/aegis
  Oracle      : ./bin/oracle  (Smin estimator — offline future-frequency greedy)
  Time Limit  : 30.0s per case   |   Points per case : 5.0
======================================================================

#    Test Case                                     Steam   Smin   Ratio  Score/5    Time  Status
----------------------------------------------------------------------
1    TC01 · Uniform Random (N=100k)                 9502   9501  0.9999   4.999/5   0.33s Valid [██████████]
2    TC02 · Clustered Satellites (50 clusters)       570    527  0.9246   4.623/5   0.31s Valid [█████████░]
...
======================================================================
  Total Code Score    48.14 / 50.0
  Code Efficiency     96.28%
======================================================================
```
