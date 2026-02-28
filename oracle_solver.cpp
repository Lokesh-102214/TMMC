/*
 * =============================================================
 * AEGIS PROTOCOL — Oracle Solver  (oracle_solver.cpp)
 * =============================================================
 *
 * PURPOSE
 * -------
 * This is the OFFLINE ORACLE used to estimate Smin — the
 * theoretical lower bound on activations against which your
 * online solver is scored.
 *
 * WHY THIS IS NOT THE TRUE Smin
 * ──────────────────────────────
 * The true Smin in the competition = the lowest Steam achieved
 * by ANY team.  That is unknown until all submissions are scored.
 *
 * The geometric optimal (minimum hitting set) is NP-Hard to
 * compute exactly, so we use the strongest practical offline
 * greedy heuristic:
 *
 *   STRATEGY: "Future Frequency Greedy"
 *   ─────────────────────────────────────
 *   Before processing any query:
 *     Precompute future_freq[i] = number of disks that contain
 *     satellite i (using ALL Q disk queries, read upfront).
 *
 *   For each disk (in order):
 *     1. If an already-active satellite is inside → output it.
 *     2. Else → activate the candidate with highest future_freq.
 *
 *   Rationale: activating the satellite that appears in the
 *   most future disks maximises expected reuse, provably better
 *   than any online strategy.
 *
 * This oracle gives a STRONG LOWER BOUND on competition Smin.
 * Your online solver's ratio = oracle_Steam / your_Steam.
 * A ratio close to 1.0 means you're near-optimal.
 *
 * IMPORTANT: This is NOT used as your submission. It is purely
 * for self-evaluation purposes.
 *
 * =============================================================
 *
 * INTERACTION FORMAT (same as judge):
 *   Read N, N points, Q, Q disks — all from stdin.
 *   Write one integer per disk to stdout.
 *
 * TIME COMPLEXITY:
 *   O(N + Q*k) where k = average candidates per disk
 *   Grid hashing keeps k small (typically k << N).
 *
 * =============================================================
 */

#include <vector>
#include <tuple>
#include <unordered_map>
#include <cmath>
#include <iostream>
using namespace std;

// ─── Grid Configuration ───────────────────────────────────────────────────────
static const double CELL_SIZE = 100.0;   // equals max R in problem

// ─── Data Structures ─────────────────────────────────────────────────────────
struct Satellite {
    double x, y;
    bool   active;
};

int N, Q;
vector<Satellite> sats;
vector<tuple<double, double, double>> disks;   // all Q disks (read upfront)
vector<int> future_freq;                        // future_freq[i] = # disks containing sat i

unordered_map<long long, vector<int>> grid;

// ─── Grid Helpers ─────────────────────────────────────────────────────────────
inline long long cellKey(int cx, int cy) {
    // Encode two cell coordinates into a single 64-bit key.
    // Offset by 20000 to handle negative coordinates.
    return ((long long)(cx + 20000)) * 100003LL + (long long)(cy + 20000);
}

void insertGrid(int idx) {
    int cx = (int)floor(sats[idx].x / CELL_SIZE);
    int cy = (int)floor(sats[idx].y / CELL_SIZE);
    grid[cellKey(cx, cy)].push_back(idx);
}

// Return all satellite indices that lie inside disk(cx, cy, r).
vector<int> queryCandidates(double cx, double cy, double r) {
    int gcx = (int)floor(cx / CELL_SIZE);
    int gcy = (int)floor(cy / CELL_SIZE);
    int exp = (int)ceil(r / CELL_SIZE) + 1;   // cell expansion factor

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

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Phase 1: Read all satellites
    cin >> N;
    sats.resize(N);
    for (int i = 0; i < N; i++) {
        cin >> sats[i].x >> sats[i].y;
        sats[i].active = false;
    }

    // Build spatial index
    for (int i = 0; i < N; i++) insertGrid(i);

    // Phase 2: Read ALL disk queries upfront (offline privilege)
    cin >> Q;
    disks.resize(Q);
    for (auto& disk : disks) {
        cin >> get<0>(disk) >> get<1>(disk) >> get<2>(disk);
    }

    // ── Precompute future_freq ────────────────────────────────────────────────
    // future_freq[i] = number of disks in which satellite i is a candidate.
    // This is the key offline advantage: an online solver cannot know this.
    future_freq.assign(N, 0);
    for (auto& disk : disks) {
        auto cands = queryCandidates(get<0>(disk), get<1>(disk), get<2>(disk));
        for (int idx : cands)
            future_freq[idx]++;
    }

    // ── Process each disk greedily ────────────────────────────────────────────
    for (auto& disk : disks) {
        auto cands = queryCandidates(get<0>(disk), get<1>(disk), get<2>(disk));

        int chosen = -1;

        // Step 1: Reuse an already-active satellite if possible (free move)
        for (int idx : cands) {
            if (sats[idx].active) {
                chosen = idx;
                break;
            }
        }

        // Step 2: No active satellite → pick candidate with highest future_freq
        if (chosen == -1) {
            int best_freq = -1;
            for (int idx : cands) {
                if (future_freq[idx] > best_freq) {
                    best_freq = future_freq[idx];
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
