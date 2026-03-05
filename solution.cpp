#include <vector>
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <algorithm>

using namespace std;

// --- Tunable Weights ---
static const double W_DIST   = 0.45;  // Reward satellites closer to the center of the anomaly
static const double W_COVER  = 0.40;  // Reward satellites covering many other dense areas
static const double W_SPARSE = 0.15;  // Reward satellites in sparse regions (unique coverage)

struct Satellite {
    double x, y;
    bool   active;
    int    precomputed_density;
};

int N; // Total number of satellites
vector<Satellite> sats;

// --- Multi-scale Grid Hashing ---
// Small grid for highly localized anomalies (r < 50)
static const double CELL_SMALL = 25.0;
// Large grid for wide-area anomalies (r >= 50)
static const double CELL_LARGE = 100.0;

unordered_map<long long, vector<int>> gridSmall, gridLarge;

// Generates a unique 1D key for a 2D grid cell to store in the hash map
inline long long cellKey(int cx, int cy, int scale) {
    return ((long long)(cx + 50000)) * 200003LL + (long long)(cy + 50000) + (long long)scale * 1000000007LL;
}

// Places a satellite into both the small and large grids for fast spatial lookups
void insertGrids(int idx) {
    int cxS = (int)floor(sats[idx].x / CELL_SMALL);
    int cyS = (int)floor(sats[idx].y / CELL_SMALL);
    gridSmall[cellKey(cxS, cyS, 0)].push_back(idx);

    int cxL = (int)floor(sats[idx].x / CELL_LARGE);
    int cyL = (int)floor(sats[idx].y / CELL_LARGE);
    gridLarge[cellKey(cxL, cyL, 1)].push_back(idx);
}

// Retrieves all satellites that fall within the radius `r` of the anomaly center (cx, cy)
vector<int> queryCandidates(double cx, double cy, double r) {
    bool useSmall = (r < 50.0);
    double cellSize = useSmall ? CELL_SMALL : CELL_LARGE;
    auto& g = useSmall ? gridSmall : gridLarge;
    int scale = useSmall ? 0 : 1;

    int gcx = (int)floor(cx / cellSize);
    int gcy = (int)floor(cy / cellSize);
    int exp = (int)ceil(r / cellSize) + 1; // Number of adjacent cells to check

    vector<int> result;
    result.reserve(128);
    double r2 = r * r;

    for (int dx = -exp; dx <= exp; dx++) {
        for (int dy = -exp; dy <= exp; dy++) {
            auto it = g.find(cellKey(gcx + dx, gcy + dy, scale));
            if (it != g.end()) {
                for (int idx : it->second) {
                    double ddx = sats[idx].x - cx;
                    double ddy = sats[idx].y - cy;
                    // Check strict geometric coverage (with tiny epsilon for floating point errors)
                    if (ddx * ddx + ddy * ddy <= r2 + 1e-9) {
                        result.push_back(idx);
                    }
                }
            }
        }
    }
    return result;
}

// Counts neighbors for the sparsity/coverage heuristic. 
// Uses early exit (cap) to strictly prevent Time Limit Exceeded (TLE) in max density areas.
int countNeighbors(int idx, double r, int cap = 300) {
    double cx = sats[idx].x, cy = sats[idx].y;
    bool useSmall = (r < 50.0);
    double cellSize = useSmall ? CELL_SMALL : CELL_LARGE;
    auto& g = useSmall ? gridSmall : gridLarge;
    int scale = useSmall ? 0 : 1;

    int gcx = (int)floor(cx / cellSize);
    int gcy = (int)floor(cy / cellSize);
    int exp = (int)ceil(r / cellSize) + 1;

    int count = 0;
    double r2 = r * r;

    for (int dx = -exp; dx <= exp; dx++) {
        for (int dy = -exp; dy <= exp; dy++) {
            auto it = g.find(cellKey(gcx + dx, gcy + dy, scale));
            if (it != g.end()) {
                for (int jdx : it->second) {
                    if (jdx == idx) continue;
                    double ddx = sats[jdx].x - cx;
                    double ddy = sats[jdx].y - cy;
                    if (ddx * ddx + ddy * ddy <= r2 + 1e-9) {
                        count++;
                        if (count >= cap) return count; // EARLY EXIT: Stop looping if very dense
                    }
                }
            }
        }
    }
    return count;
}

int main() {
    // Fast I/O
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // ==========================================
    // PHASE 1: INITIALIZATION
    // ==========================================
    if (!(cin >> N)) return 0;
    
    sats.resize(N);
    for (int i = 0; i < N; i++) {
        cin >> sats[i].x >> sats[i].y;
        sats[i].active = false;
        sats[i].precomputed_density = 0;
    }

    // Populate the multi-scale grids
    for (int i = 0; i < N; i++) {
        insertGrids(i);
    }

    // Precompute local density (Medium Range)
    int maxDensity = 1;
    for (int i = 0; i < N; i++) {
        sats[i].precomputed_density = countNeighbors(i, 50.0, 300);
        maxDensity = max(maxDensity, sats[i].precomputed_density);
    }

    // ==========================================
    // PHASE 2: ONLINE QUERIES
    // ==========================================
    int Q;
    if (!(cin >> Q)) return 0;

    vector<int> active_list; // Track activated satellites for the fast-path check

    while (Q--) {
        double cx, cy, r;
        cin >> cx >> cy >> r;

        // 1. FAST PATH: Check previously activated satellites FIRST (Prevents TLE on overlapping queries)
        bool already_covered = false;
        for (int idx : active_list) {
            double ddx = sats[idx].x - cx;
            double ddy = sats[idx].y - cy;
            if (ddx * ddx + ddy * ddy <= r * r + 1e-9) {
                // RULE 3: Output the index of the already active satellite, then flush.
                cout << idx << endl; // endl outputs \n AND mechanically flushes the stream
                already_covered = true;
                break;
            }
        }
        
        // If an active satellite already handles this, move to the next query immediately
        if (already_covered) continue;

        // 2. SEARCH GRID: Only occurs if we actually need a new satellite
        auto cands = queryCandidates(cx, cy, r);

        // Fallback: If no candidate is found (technically shouldn't happen per constraints), output 0 safely.
        if (cands.empty()) {
            cout << 0 << endl;
            active_list.push_back(0);
            sats[0].active = true;
            continue;
        }

        // 3. SCORING & SELECTION
        int chosen = -1;
        double bestScore = -1e18; // Start with lowest possible score

        // Safely determine if we want to run the expensive "coverage" calculation
        bool doCoverage = (cands.size() <= 200);

        // Sub-sample logic: Prevents TLE in Test Case 9 (100k satellites clumped together)
        // If > 500 candidates, we stride through them to evaluate a perfectly uniform sub-sample.
        int max_eval = 500;
        int step = max(1, (int)cands.size() / max_eval);

        for (size_t i = 0; i < cands.size(); i += step) {
            int idx = cands[i];
            
            // Centrality Score (closer to center of anomaly = better)
            double ddx = sats[idx].x - cx;
            double ddy = sats[idx].y - cy;
            double dist = sqrt(ddx * ddx + ddy * ddy);
            double centrality = (r - dist) / (r + 1e-9);

            // Coverage / Sparsity Scores
            double coverageScore = 0.0;
            double sparsityScore = 0.0;

            if (doCoverage) {
                // If it's a sparse region, evaluate how much future coverage this point offers
                int localCts = countNeighbors(idx, min(100.0, 2.0 * r), 150);
                coverageScore = (double)localCts / 150.0;
                
                double densityRatio = (double)sats[idx].precomputed_density / (double)maxDensity;
                sparsityScore = 1.0 - densityRatio; // Reward isolation
            } else {
                // If it's densely packed, everyone has massive coverage. Just use precomputed.
                coverageScore = (double)sats[idx].precomputed_density / (double)maxDensity;
                sparsityScore = 0.0;
            }

            // Weighted combination
            double score = (W_DIST * centrality) + 
                           (W_COVER * coverageScore) + 
                           (W_SPARSE * sparsityScore);

            // Keep track of the highest scoring index
            if (score > bestScore) {
                bestScore = score;
                chosen = idx;
            }
        }

        // If something went wrong with the step calculation, fallback to first candidate
        if (chosen == -1) {
            chosen = cands[0]; 
        }

        // 4. ACTIVATE AND OUTPUT
        sats[chosen].active = true;
        active_list.push_back(chosen);

        // Output securely and flush Interactive Protocol
        cout << chosen << endl;
    }
    
    return 0;
}