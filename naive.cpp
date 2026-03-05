/*
 * =============================================================
 * AEGIS PROTOCOL — Naive Solver  (naive_solver.cpp)
 * =============================================================
 *
 * PURPOSE
 * -------
 * This is the WORST-CASE BASELINE solver.
 * It is intentionally dumb to show how much worse a trivial
 * approach performs vs the smart hybrid solver.
 *
 * WHAT IT DOES (deliberately bad)
 * ─────────────────────────────────
 *   For every disk query:
 *     1. Scan satellites in raw index order (0, 1, 2, ...).
 *     2. Pick the FIRST satellite that geometrically fits.
 *     3. Output it — whether or not it was already active.
 *
 * WHY THIS IS TERRIBLE
 * ─────────────────────
 *   1. NO reuse check — even if satellite #7 is already active
 *      and inside the disk, it will be skipped if satellite #3
 *      also fits and comes earlier in the array.
 *      → But satellite #3 might be far from all future disks.
 *
 *   2. NO density awareness — "first found" has nothing to do
 *      with how many future disks will visit that region.
 *
 *   3. NO centrality — the first-found satellite is typically
 *      near the disk boundary, not the center, so it is least
 *      likely to be re-covered by nearby future disks.
 *
 *   4. Index-order bias — satellites indexed 0..N/10 will be
 *      activated far more often than satellites indexed 9N/10..N,
 *      creating systematic waste.
 *
 * NET EFFECT
 * ───────────
 *   Steam ≈ Q (nearly every query activates a brand-new satellite)
 *   because the solver never deliberately reuses anything.
 *   On repeated-center tests (TC08) Steam can approach 10,000
 *   even though the optimal is only ~500.
 *
 * USE THIS FILE TO
 * ─────────────────
 *   - Provide a "floor" score in the evaluation table.
 *   - Demonstrate to judges how much the heuristic matters.
 *   - Verify that even a dumb solver produces VALID output
 *     (correct geometry, no crashes) — validity != efficiency.
 *
 * INTERACTION FORMAT
 * ───────────────────
 *   Identical to judge format.
 *   Read: N, N×(x y), Q, Q×(cx cy r)
 *   Write: one integer per disk (0-indexed satellite)
 *
 * TIME COMPLEXITY
 * ────────────────
 *   O(N) per query in the worst case — brute-force linear scan.
 *   No spatial index, no grid, no hash map.
 *   Will be slow for large N but still correct.
 *
 * =============================================================
 */

#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ── Phase 1: Read satellites ──────────────────────────────────────────────
    int N;
    cin >> N;

    vector<double> sx(N), sy(N);
    for (int i = 0; i < N; i++) {
        cin >> sx[i] >> sy[i];
    }

    // ── Phase 2: Answer queries ───────────────────────────────────────────────
    int Q;
    cin >> Q;

    while (Q--) {
        double cx, cy, r;
        cin >> cx >> cy >> r;

        double r2 = r * r;
        int chosen = -1;

        // Dumb strategy: linear scan from index 0, take the very first hit.
        // No reuse check. No density. No centrality.
        // The satellite chosen will be whatever happens to come first in the
        // original input order — completely unrelated to future usefulness.
        for (int i = 0; i < N; i++) {
            double dx = sx[i] - cx;
            double dy = sy[i] - cy;
            if (dx * dx + dy * dy <= r2 + 1e-9) {
                chosen = i;
                break;   // take FIRST found, never look for better
            }
        }

        // (The judge guarantees at least one point exists, so chosen != -1)
        cout << chosen << "\n";
        cout.flush();
    }

    return 0;
}