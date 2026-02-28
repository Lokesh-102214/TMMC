/*
 * ═══════════════════════════════════════════════════════════════════
 *  Aegis Protocol: Online Disk Defense  —  Optimized C++ Solution
 *  Strategy: Multi-Scale Density  +  Adaptive Spread  +  Grid Hash
 * ═══════════════════════════════════════════════════════════════════
 *
 *  APPROACH OVERVIEW:
 *  ─────────────────
 *  Simple density (count neighbors) is suboptimal because it always
 *  selects satellites in the most "crowded" cluster — causing wasted
 *  activations when disks cover multiple cluster regions.
 *
 *  Our improved strategy combines THREE signals:
 *
 *  1. MULTI-SCALE DENSITY  (precomputed, static)
 *     Compute density at radii 75, 150, 300, 600.
 *     A satellite scoring high at ALL scales is truly central,
 *     not just locally clustered.
 *
 *  2. ADAPTIVE SPREAD BONUS  (dynamic, per query)
 *     Penalize candidates that are near already-active satellites.
 *     This forces selections to spread across space, maximizing
 *     the chance each new activation covers unique future disks.
 *     Uses a separate active-set grid for O(1) lookups.
 *
 *  3. PROXIMITY TO DISK CENTER  (tiebreaker)
 *     Among equal-scored candidates, prefer the one closest to the
 *     disk center — it is the most "committed" choice.
 *
 *  SPATIAL DATA STRUCTURES:
 *  ─────────────────────────
 *  • Static grid (cell = 100)  → find all candidates inside a disk
 *  • Active grid (cell = 100)  → find nearest active satellite fast
 *  Both grids use hash maps: (cx,cy) → vector<index>
 *
 *  TIME COMPLEXITY:
 *  ────────────────
 *  • Build:      O(N)
 *  • Density:    O(N × k)  where k = avg satellites per cell region
 *  • Per query:  O(k)      sub-linear, not O(N)
 * ═══════════════════════════════════════════════════════════════════
 */

#include <bits/stdc++.h>
using namespace std;

// ──────────────────────────────────────────────
//  TUNABLE PARAMETERS
// ──────────────────────────────────────────────
static const double CELL      = 100.0;   // grid cell size (= max radius)
static const double SPREAD_R  = 300.0;   // radius to search for nearest active
// Multi-scale density radii
static const double D_RADII[] = {75.0, 150.0, 300.0, 600.0};
// Weights for each density scale (closer scale = more weight)
static const double D_WEIGHTS[]= {1.0,  0.8,   0.5,   0.3 };
static const int    N_SCALES   = 4;
// Weight of spread bonus vs density in final score
static const double SPREAD_W  = 1.5;

// ──────────────────────────────────────────────
//  POINT & GLOBAL STATE
// ──────────────────────────────────────────────
struct Pt { double x, y; };

int          N;
vector<Pt>   pts;
vector<bool> active;
vector<double> density_score;  // precomputed multi-scale density

// ──────────────────────────────────────────────
//  GRID HASH UTILITY
// ──────────────────────────────────────────────
using Grid = unordered_map<long long, vector<int>>;

// Encode (cx,cy) as a unique 64-bit key
inline long long key(int cx, int cy) {
    return (long long)(cx + 30000) * 200003LL + (cy + 30000);
}

// Insert point index into grid
inline void gridInsert(Grid& g, int idx, double x, double y) {
    int cx = (int)floor(x / CELL);
    int cy = (int)floor(y / CELL);
    g[key(cx,cy)].push_back(idx);
}

// Get all indices within radius R of (qx,qy)
void gridQuery(const Grid& g, double qx, double qy, double R,
               vector<int>& out) {
    out.clear();
    double r2 = R * R;
    int x0 = (int)floor((qx-R)/CELL), x1 = (int)floor((qx+R)/CELL);
    int y0 = (int)floor((qy-R)/CELL), y1 = (int)floor((qy+R)/CELL);
    for (int cx = x0; cx <= x1; cx++) {
        for (int cy = y0; cy <= y1; cy++) {
            auto it = g.find(key(cx,cy));
            if (it == g.end()) continue;
            for (int i : it->second) {
                double dx = pts[i].x - qx, dy = pts[i].y - qy;
                if (dx*dx + dy*dy <= r2) out.push_back(i);
            }
        }
    }
}

// Get count (not indices) within radius R — used for density
int gridCount(const Grid& g, double qx, double qy, double R) {
    int cnt = 0;
    double r2 = R * R;
    int x0 = (int)floor((qx-R)/CELL), x1 = (int)floor((qx+R)/CELL);
    int y0 = (int)floor((qy-R)/CELL), y1 = (int)floor((qy+R)/CELL);
    for (int cx = x0; cx <= x1; cx++) {
        for (int cy = y0; cy <= y1; cy++) {
            auto it = g.find(key(cx,cy));
            if (it == g.end()) continue;
            for (int i : it->second) {
                double dx = pts[i].x - qx, dy = pts[i].y - qy;
                if (dx*dx + dy*dy <= r2) cnt++;
            }
        }
    }
    return cnt;
}

// Find min squared distance from (qx,qy) to any active satellite
// Searches expanding rings of grid cells until it finds a hit or exhausts SPREAD_R
double nearestActiveDist2(const Grid& ag, double qx, double qy) {
    // Search within SPREAD_R; if no active nearby, return large value
    int x0 = (int)floor((qx-SPREAD_R)/CELL), x1 = (int)floor((qx+SPREAD_R)/CELL);
    int y0 = (int)floor((qy-SPREAD_R)/CELL), y1 = (int)floor((qy+SPREAD_R)/CELL);
    double best = SPREAD_R * SPREAD_R + 1.0;
    for (int cx = x0; cx <= x1; cx++) {
        for (int cy = y0; cy <= y1; cy++) {
            auto it = ag.find(key(cx,cy));
            if (it == ag.end()) continue;
            for (int i : it->second) {
                double dx = pts[i].x - qx, dy = pts[i].y - qy;
                double d2 = dx*dx + dy*dy;
                if (d2 < best) best = d2;
            }
        }
    }
    return best;
}

// ──────────────────────────────────────────────
//  PRECOMPUTE MULTI-SCALE DENSITY
// ──────────────────────────────────────────────
void precomputeDensity(const Grid& sg) {
    density_score.resize(N, 0.0);
    for (int i = 0; i < N; i++) {
        double score = 0.0;
        for (int s = 0; s < N_SCALES; s++) {
            int cnt = gridCount(sg, pts[i].x, pts[i].y, D_RADII[s]);
            score += D_WEIGHTS[s] * cnt;
        }
        density_score[i] = score;
    }
    // Normalize to [0, 1]
    double maxS = *max_element(density_score.begin(), density_score.end());
    if (maxS > 0)
        for (auto& d : density_score) d /= maxS;
}

// ──────────────────────────────────────────────
//  SCORE FUNCTION  (called per candidate per query)
//
//  score = density_score[i]
//        + SPREAD_W × normalize(nearest_active_dist)
//
//  The spread term rewards candidates far from active satellites,
//  forcing the active set to spread across space.
// ──────────────────────────────────────────────
inline double candidateScore(int i, const Grid& ag) {
    double d2   = nearestActiveDist2(ag, pts[i].x, pts[i].y);
    // Normalize distance: 1.0 if >= SPREAD_R, 0.0 if distance = 0
    double norm = min(1.0, sqrt(d2) / SPREAD_R);
    return density_score[i] + SPREAD_W * norm;
}

// ──────────────────────────────────────────────
//  MAIN
// ──────────────────────────────────────────────
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ─── Phase 1: Read satellites ─────────────
    cin >> N;
    pts.resize(N);
    active.resize(N, false);

    Grid staticGrid;  // all satellites
    staticGrid.reserve(N * 2);

    for (int i = 0; i < N; i++) {
        cin >> pts[i].x >> pts[i].y;
        gridInsert(staticGrid, i, pts[i].x, pts[i].y);
    }

    // Precompute multi-scale density
    precomputeDensity(staticGrid);

    // ─── Phase 2: Answer queries ──────────────
    int Q;
    cin >> Q;

    Grid activeGrid;   // only active satellites — updated dynamically
    activeGrid.reserve(1024);

    vector<int> candidates;
    candidates.reserve(512);

    while (Q--) {
        double cx, cy, R;
        cin >> cx >> cy >> R;

        // Find all satellites inside this disk
        gridQuery(staticGrid, cx, cy, R, candidates);

        int chosen = -1;

        // ── Check if already covered by an active satellite ──
        for (int i : candidates) {
            if (active[i]) {
                chosen = i;
                break;
            }
        }

        // ── If not covered, pick best inactive candidate ──────
        if (chosen == -1) {
            double bestScore = -1e18;
            double bestDist2 = 1e18;   // tiebreaker: proximity to center

            for (int i : candidates) {
                // active already filtered above (none found), but double check
                if (active[i]) { chosen = i; break; }

                double sc = candidateScore(i, activeGrid);
                double dx = pts[i].x - cx, dy = pts[i].y - cy;
                double d2 = dx*dx + dy*dy;

                if (sc > bestScore || (sc == bestScore && d2 < bestDist2)) {
                    bestScore = sc;
                    bestDist2 = d2;
                    chosen = i;
                }
            }

            // Activate chosen satellite
            active[chosen] = true;
            gridInsert(activeGrid, chosen, pts[chosen].x, pts[chosen].y);
        }

        cout << chosen << '\n';
        cout.flush();
    }

    return 0;
}
