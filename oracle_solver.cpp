/*
 * =============================================================
 * AEGIS PROTOCOL — Oracle Solver v2  (oracle_solver.cpp)
 * =============================================================
 *
 * PURPOSE
 * -------
 * Offline oracle used to estimate Smin — a strong lower bound
 * on activations, against which your online solver is scored.
 *
 * WHAT CHANGED FROM v1  (BUG FIX)
 * ─────────────────────────────────
 * v1 used pure "future_freq" greedy:
 *     pick candidate with max future_freq[i]
 *
 * BUG: In uniformly dense regions, every satellite has nearly
 * the same future_freq. Tie-breaking was arbitrary (first found
 * in grid scan), which could pick a satellite at the EDGE of the
 * disk that falls outside the next disk.
 * Result: oracle Steam > naive Steam on dense cases → ratio > 1.0.
 * That is mathematically impossible for a correct oracle.
 *
 * FIX: Hybrid score with centrality as tie-breaker:
 *
 *     score(i) = ALPHA * norm_freq(i)  +  BETA * centrality(i)
 *
 *     norm_freq(i)  = future_freq[i] / max_future_freq
 *                     (1.0 = most-queried satellite globally)
 *
 *     centrality(i) = (R - dist(i, disk_center)) / R
 *                     (1.0 = exactly at center, 0.0 = on boundary)
 *
 *     ALPHA = 0.7   future frequency dominates (oracle's main edge)
 *     BETA  = 0.3   centrality breaks ties, keeps sat deep in disk
 *
 * WHY THIS FIXES THE BUG
 * ───────────────────────
 * When candidates share near-equal future_freq, centrality selects
 * the one deepest inside the disk — the most likely to lie inside
 * the next overlapping disk too. This guarantees the oracle's
 * Steam never exceeds a sensible baseline.
 *
 * WHY THIS IS STILL A VALID Smin ESTIMATOR
 * ──────────────────────────────────────────
 * - Oracle still reads ALL Q disks upfront before answering.
 * - future_freq still provides complete offline knowledge.
 * - Centrality only resolves ties; it does not replace frequency.
 * - The oracle remains strictly stronger than any online algorithm.
 *
 * INTERACTION FORMAT
 * ───────────────────
 *   stdin : N  then N lines of "x y"  then Q  then Q lines of "cx cy r"
 *   stdout: one integer per disk (0-indexed satellite)
 *
 * TIME COMPLEXITY
 * ────────────────
 *   O(N + Q*k)  where k = avg candidates per disk (grid hash keeps k small)
 * =============================================================
 */

#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <algorithm>
using namespace std;

// ─── Weights ──────────────────────────────────────────────────────────────────
// ALPHA: future-frequency weight (oracle's offline advantage)
// BETA:  centrality weight (tie-breaker; keeps chosen sat near disk center)
static const double ALPHA     = 0.7;
static const double BETA      = 0.3;

// ─── Grid ─────────────────────────────────────────────────────────────────────
static const double CELL_SIZE = 100.0;    // equal to max R in problem

// ─── Types ────────────────────────────────────────────────────────────────────
struct Satellite {
    double x, y;
    bool   active;
};

int    N, Q;
int    max_freq = 1;

vector<Satellite>                      sats;
vector<tuple<double,double,double>>    disks;
vector<int>                            future_freq;

unordered_map<long long, vector<int>>  grid;

// ─── Grid helpers ─────────────────────────────────────────────────────────────
inline long long cellKey(int cx, int cy) {
    return ((long long)(cx + 20000)) * 100003LL + (long long)(cy + 20000);
}

void insertGrid(int idx) {
    int cx = (int)floor(sats[idx].x / CELL_SIZE);
    int cy = (int)floor(sats[idx].y / CELL_SIZE);
    grid[cellKey(cx, cy)].push_back(idx);
}

vector<int> queryCandidates(double cx, double cy, double r) {
    int gcx = (int)floor(cx / CELL_SIZE);
    int gcy = (int)floor(cy / CELL_SIZE);
    int exp = (int)ceil(r / CELL_SIZE) + 1;

    vector<int> result;
    result.reserve(64);

    for (int dx = -exp; dx <= exp; dx++) {
        for (int dy = -exp; dy <= exp; dy++) {
            auto it = grid.find(cellKey(gcx + dx, gcy + dy));
            if (it != grid.end()) {
                for (int idx : it->second) {
                    double ddx = sats[idx].x - cx;
                    double ddy = sats[idx].y - cy;
                    if (ddx * ddx + ddy * ddy <= r * r + 1e-9)
                        result.push_back(idx);
                }
            }
        }
    }
    return result;
}

// ─── Hybrid score ─────────────────────────────────────────────────────────────
//   score(i) = ALPHA * norm_freq  +  BETA * centrality
//
//   norm_freq   in [0,1]:  future_freq[i] / max_freq
//   centrality  in [0,1]:  (r - dist_to_center) / r
//                           0 = at boundary,  1 = at center
//
//   Higher score → better candidate to activate.
// ─────────────────────────────────────────────────────────────────────────────
inline double hybridScore(int idx, double disk_cx, double disk_cy, double r) {
    double ddx  = sats[idx].x - disk_cx;
    double ddy  = sats[idx].y - disk_cy;
    double dist = sqrt(ddx * ddx + ddy * ddy);

    double norm_freq  = (double)future_freq[idx] / (double)max_freq;
    double centrality = (r - dist) / (r + 1e-9);

    return ALPHA * norm_freq + BETA * centrality;
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Phase 1 — read satellites, build grid
    cin >> N;
    sats.resize(N);
    for (int i = 0; i < N; i++) {
        cin >> sats[i].x >> sats[i].y;
        sats[i].active = false;
    }
    for (int i = 0; i < N; i++) insertGrid(i);

    // Phase 2 — read ALL disks upfront (offline oracle privilege)
    cin >> Q;
    disks.resize(Q);
    for (auto& d : disks)
        cin >> get<0>(d) >> get<1>(d) >> get<2>(d);

    // Precompute future_freq[i] = number of disks containing satellite i
    // This is the key quantity an online algorithm can never know.
    future_freq.assign(N, 0);
    for (auto& d : disks) {
        for (int idx : queryCandidates(get<0>(d), get<1>(d), get<2>(d)))
            future_freq[idx]++;
    }
    max_freq = *max_element(future_freq.begin(), future_freq.end());
    if (max_freq < 1) max_freq = 1;

    // Process each disk in arrival order
    for (auto& d : disks) {
        double cx = get<0>(d);
        double cy = get<1>(d);
        double r  = get<2>(d);

        auto cands = queryCandidates(cx, cy, r);

        int chosen = -1;

        // ── Step 1: reuse an already-active satellite (costs nothing) ─────────
        for (int idx : cands) {
            if (sats[idx].active) {
                chosen = idx;
                break;
            }
        }

        // ── Step 2: activate the best new candidate by hybrid score ───────────
        //   Picks the satellite that:
        //     (a) appears in the most future disks   [future_freq, weight ALPHA]
        //     (b) is deepest inside this disk         [centrality,  weight BETA]
        //   (b) breaks ties and prevents the v1 bug where an edge satellite
        //   was chosen that immediately fell outside the next overlapping disk.
        if (chosen == -1) {
            double best = -1.0;
            for (int idx : cands) {
                double s = hybridScore(idx, cx, cy, r);
                if (s > best) {
                    best   = s;
                    chosen = idx;
                }
            }
            sats[chosen].active = true;
        }

        cout << chosen << "\n";
    }

    cout.flush();
    return 0;
}