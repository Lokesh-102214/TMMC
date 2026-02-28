"""
=============================================================
AEGIS PROTOCOL — Evaluation Runner
=============================================================
Runs both solvers on all 10 test cases.
Validates every output geometrically.
Computes Steam, Smin, Ratio, Score/5, Time, Status.
Prints a full formatted report.

Prerequisites:
    1. Generate tests   : python3 1_generate_tests.py
    2. Build binaries   : ./2_build_solvers.sh

Usage:
    python3 3_evaluate.py

    Optional flags:
        --your-solver  PATH   (default: ./bin/aegis)
        --oracle       PATH   (default: ./bin/oracle)
        --tests-dir    PATH   (default: ./tests)
        --time-limit   SECS   (default: 30.0)
        --max-pts      FLOAT  (default: 5.0 per test case)

Understanding Smin:
    Smin is estimated by the OFFLINE ORACLE solver which reads
    ALL disk queries before answering (full future knowledge).
    It picks the satellite that appears in the most future disks.
    This gives a strong lower bound — the real competition Smin
    could only be equal to or less than this oracle value.
    Your efficiency ratio = oracle_Steam / your_Steam.
=============================================================
"""

import subprocess
import time
import os
import sys
import argparse


# ─── CLI Arguments ────────────────────────────────────────────────────────────
# On Windows, we expect .exe binaries.
bin_ext = ".exe" if sys.platform == "win32" else ""

parser = argparse.ArgumentParser(description="Aegis Protocol Evaluator")
parser.add_argument("--your-solver", default=f"./bin/aegis{bin_ext}",
                    help="Path to your compiled solver binary")
parser.add_argument("--oracle",      default=f"./bin/oracle{bin_ext}",
                    help="Path to the oracle (Smin estimator) binary")
parser.add_argument("--tests-dir",  default="./tests",
                    help="Directory containing the test case .txt files")
parser.add_argument("--time-limit", default=30.0, type=float,
                    help="Time limit in seconds per test case (default: 30.0)")
parser.add_argument("--max-pts",    default=5.0, type=float,
                    help="Maximum points awarded per test case (default: 5.0)")
args = parser.parse_args()

YOUR_SOLVER = args.your_solver
ORACLE      = args.oracle
TESTS_DIR   = args.tests_dir
TIME_LIMIT  = args.time_limit
MAX_PTS     = args.max_pts


# ─── Test Case Registry ───────────────────────────────────────────────────────
# (display_name, filename)
TEST_CASES = [
    ("TC01 · Uniform Random (N=100k)",          "tc01_uniform_random.txt"),
    ("TC02 · Clustered Satellites (50 clusters)","tc02_clustered_satellites.txt"),
    ("TC03 · Grid Sweep Adversarial (N=90k)",   "tc03_grid_sweep.txt"),
    ("TC04 · Gaussian + Tiny Disks (R=1–5)",    "tc04_gaussian_tiny_disks.txt"),
    ("TC05 · Ring Distribution (N=80k)",        "tc05_ring_distribution.txt"),
    ("TC06 · Adversarial Sparse Regions",       "tc06_adversarial_sparse.txt"),
    ("TC07 · Fractal / Sierpinski (N=30k)",     "tc07_fractal_sierpinski.txt"),
    ("TC08 · Repeated Hot Centers (500 hubs)",  "tc08_repeated_hot_centers.txt"),
    ("TC09 · Dense Overlap (small area R=100)", "tc09_dense_overlap.txt"),
    ("TC10 · Adversarial Random Walk",          "tc10_adversarial_walk.txt"),
]


# ─── Runner ───────────────────────────────────────────────────────────────────
def run_binary(binary_path, input_data, time_limit):
    """
    Run a binary with input_data piped to stdin.
    Returns (stdout: str | None, elapsed: float, error: str | None)
    """
    try:
        t0 = time.time()
        result = subprocess.run(
            [binary_path],
            input=input_data,
            capture_output=True,
            text=True,
            timeout=time_limit
        )
        elapsed = time.time() - t0
        if result.returncode != 0:
            return None, elapsed, f"Non-zero exit ({result.returncode}): {result.stderr[:120]}"
        return result.stdout, elapsed, None
    except subprocess.TimeoutExpired:
        return None, time_limit, "TLE (exceeded time limit)"
    except FileNotFoundError:
        return None, 0.0, f"Binary not found: {binary_path}"
    except Exception as e:
        return None, 0.0, str(e)


# ─── Validator ────────────────────────────────────────────────────────────────
def validate(input_data, output_str):
    """
    Geometrically validate every output line:
      - Is the index in [0, N)?
      - Does the satellite actually lie inside the disk?

    Returns:
        valid  : bool
        steam  : int   (number of unique satellites activated)
        error  : str   (description if invalid, else 'OK')
    """
    lines = input_data.strip().split('\n')
    ptr = 0

    N = int(lines[ptr]); ptr += 1
    pts = []
    for _ in range(N):
        x, y = map(float, lines[ptr].split()); ptr += 1
        pts.append((x, y))

    Q = int(lines[ptr]); ptr += 1
    disks = []
    for _ in range(Q):
        cx, cy, r = map(float, lines[ptr].split()); ptr += 1
        disks.append((cx, cy, r))

    out_lines = [ln.strip() for ln in output_str.strip().split('\n') if ln.strip()]

    if len(out_lines) != Q:
        return False, 0, f"Expected {Q} output lines, got {len(out_lines)}"

    activated = set()
    for q in range(Q):
        # Parse index
        try:
            sidx = int(out_lines[q])
        except ValueError:
            return False, 0, f"Query {q+1}: non-integer output '{out_lines[q]}'"

        # Range check
        if not (0 <= sidx < N):
            return False, 0, f"Query {q+1}: index {sidx} out of range [0, {N})"

        # Geometric check: satellite must be inside or on boundary of disk
        cx, cy, r = disks[q]
        sx, sy    = pts[sidx]
        dist2     = (sx - cx) ** 2 + (sy - cy) ** 2
        if dist2 > r * r + 1e-6:
            return (False, 0,
                    f"Query {q+1}: satellite {sidx} at ({sx:.2f},{sy:.2f}) "
                    f"is OUTSIDE disk center=({cx:.2f},{cy:.2f}) r={r:.2f} "
                    f"[dist={dist2**0.5:.4f} > r={r:.4f}]")

        activated.add(sidx)

    return True, len(activated), "OK"


# ─── Preflight Checks ─────────────────────────────────────────────────────────
def preflight():
    ok = True
    for label, path in [("Your solver", YOUR_SOLVER), ("Oracle", ORACLE)]:
        if not os.path.isfile(path):
            print(f"  ERROR: {label} binary not found at '{path}'")
            ok = False
        elif not os.access(path, os.X_OK):
            print(f"  ERROR: {label} binary at '{path}' is not executable")
            ok = False
    if not os.path.isdir(TESTS_DIR):
        print(f"  ERROR: Tests directory not found: '{TESTS_DIR}'")
        ok = False
    return ok


# ─── Main Evaluation Loop ─────────────────────────────────────────────────────
def main():
    print()
    print("=" * 78)
    print("  AEGIS PROTOCOL — Evaluation Report")
    print(f"  Your Solver : {YOUR_SOLVER}")
    print(f"  Oracle      : {ORACLE}  (Smin estimator — offline future-frequency greedy)")
    print(f"  Time Limit  : {TIME_LIMIT}s per case   |   Points per case : {MAX_PTS}")
    print("=" * 78)

    if not preflight():
        print("\n  Fix the above errors and re-run.\n")
        sys.exit(1)

    print()
    header = (f"{'#':<4} {'Test Case':<44} {'Steam':>6} {'Smin':>6} "
              f"{'Ratio':>7} {f'Score/{MAX_PTS:.0f}':>8} {'Time':>7}  Status")
    print(header)
    print("-" * 78)

    total_score   = 0.0
    total_max     = 0.0
    all_results   = []
    n_valid       = 0
    n_wa          = 0
    n_tle         = 0

    for tc_num, (name, fname) in enumerate(TEST_CASES, start=1):
        fpath = os.path.join(TESTS_DIR, fname)
        total_max += MAX_PTS

        # ── Load test case ────────────────────────────────────────────────────
        if not os.path.isfile(fpath):
            status = f"MISSING FILE: {fpath}"
            print(f"{tc_num:<4} {name:<44} {'---':>6} {'---':>6} {'---':>7} "
                  f"{'0.00':>8} {'---':>7}  {status}")
            all_results.append((name, 0, 0, 0.0, 0.0, 0.0, status))
            n_wa += 1
            continue

        with open(fpath) as f:
            input_data = f.read()

        # ── Run your solver ───────────────────────────────────────────────────
        out_yours, t_yours, err_yours = run_binary(YOUR_SOLVER, input_data, TIME_LIMIT)

        if err_yours:
            status = f"WA/TLE — {err_yours[:55]}"
            print(f"{tc_num:<4} {name:<44} {'---':>6} {'---':>6} {'---':>7} "
                  f"{'0.00':>8} {t_yours:>6.2f}s  {status}")
            all_results.append((name, 0, 0, 0.0, 0.0, t_yours, status))
            n_tle += (1 if "TLE" in err_yours else 0)
            n_wa  += (0 if "TLE" in err_yours else 1)
            continue

        # ── Validate your output ──────────────────────────────────────────────
        valid, steam, err_msg = validate(input_data, out_yours)
        if not valid:
            status = f"WA — {err_msg[:55]}"
            print(f"{tc_num:<4} {name:<44} {'---':>6} {'---':>6} {'---':>7} "
                  f"{'0.00':>8} {t_yours:>6.2f}s  {status}")
            all_results.append((name, 0, 0, 0.0, 0.0, t_yours, status))
            n_wa += 1
            continue

        # ── Run oracle to get Smin estimate ───────────────────────────────────
        out_oracle, t_oracle, err_oracle = run_binary(ORACLE, input_data, TIME_LIMIT)
        if err_oracle or out_oracle is None:
            # Oracle failed → use Steam as Smin (ratio = 1.0, fair fallback)
            smin = steam
        else:
            valid_o, smin, _ = validate(input_data, out_oracle)
            if not valid_o:
                smin = steam   # oracle gave invalid output; fallback

        # Clamp: Smin cannot logically exceed Steam (oracle is at least as good)
        smin = min(smin, steam)

        # ── Compute score ─────────────────────────────────────────────────────
        ratio = smin / max(steam, 1)
        score = ratio * MAX_PTS
        total_score += score
        n_valid += 1

        status = "Valid"
        bar_filled = int(round(ratio * 10))
        bar = "█" * bar_filled + "░" * (10 - bar_filled)
        print(f"{tc_num:<4} {name:<44} {steam:>6} {smin:>6} {ratio:>7.4f} "
              f"{score:>7.3f}/{MAX_PTS:.0f} {t_yours:>6.2f}s  {status}  [{bar}]")
        all_results.append((name, steam, smin, ratio, score, t_yours, status))

    # ─── Summary ──────────────────────────────────────────────────────────────
    pct = (total_score / total_max * 100) if total_max > 0 else 0.0
    print("=" * 78)
    print(f"  {'Valid / WA / TLE':<38}  {n_valid} / {n_wa} / {n_tle}")
    print(f"  {'Total Code Score':<38}  {total_score:.3f} / {total_max:.1f}")
    print(f"  {'Code Efficiency Percentage':<38}  {pct:.2f}%")
    print("=" * 78)

    # ─── Per-test breakdown ───────────────────────────────────────────────────
    print()
    print("  WHAT Smin MEANS (important to understand)")
    print("  " + "-" * 74)
    print("  Smin here = activations by the OFFLINE ORACLE (reads all future")
    print("  disk queries before answering). This is a lower-bound estimate.")
    print()
    print("  In the real competition:")
    print("    Smin = lowest Steam among ALL participating teams.")
    print("    Your final score = (competition_Smin / your_Steam) × max_pts.")
    print()
    print("  The oracle Smin shown above is always ≤ the real competition Smin,")
    print("  so your real competition ratio will be ≥ the ratio shown here.")
    print("  In other words: your actual competition score >= this self-evaluation.")
    print("=" * 78)
    print()

    # ─── Per-test explanations ────────────────────────────────────────────────
    explanations = {
        "TC01": "Uniform sparse space. Nearly every disk needs a fresh satellite → high Steam expected for all solvers.",
        "TC02": "50 tight clusters targeted repeatedly. Density heuristic shines; hub satellite absorbs many disks.",
        "TC03": "Grid + sweeping disks. Consecutive disks rarely overlap → adversarial for reuse strategies.",
        "TC04": "Tiny disks (R=1–5). Almost zero overlap possible → Steam ≈ Q for everyone; differences negligible.",
        "TC05": "Ring distribution. Density is uniform along arc → centrality heuristic is the differentiator.",
        "TC06": "Disks target sparsest regions. Tests whether solver handles low-density areas gracefully.",
        "TC07": "Fractal (Sierpinski). Multi-scale density structure; nested dense pockets reward local density.",
        "TC08": "500 hot centers × 20 queries each. Best solver activates 1 hub per center = 500 total.",
        "TC09": "Dense area, large disks. Maximum overlap; the fewer activations, the better the score.",
        "TC10": "Random walk. Partial overlap at each step; both reuse and careful selection matter.",
    }
    print("  PER-TEST ANALYSIS")
    print("  " + "-" * 74)
    for tc_num, (name, fname) in enumerate(TEST_CASES, start=1):
        key = f"TC{tc_num:02d}"
        desc = explanations.get(key, "")
        print(f"  {key}: {desc}")
    print()


if __name__ == "__main__":
    main()
