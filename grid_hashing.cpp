#include <vector>
#include <unordered_map>
#include <cmath>
#include <iostream>
using namespace std;

static const double CELL_SIZE  = 100.0;
static const double DENSITY_R  = 100.0;
static const double DENSITY_R2 = DENSITY_R * DENSITY_R;
static const double ALPHA      = 0.4;
static const double BETA       = 0.6;
static const int    DENSITY_CAP = 500;  // cap density to avoid O(N^2)

struct Satellite {
    double x, y;
    int    density;
    bool   active;
};

int N;
vector<Satellite> sats;
unordered_map<long long, vector<int>> grid;

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

    for (int dx = -exp; dx <= exp && (int)result.size() < 10000; dx++) {
        for (int dy = -exp; dy <= exp && (int)result.size() < 10000; dy++) {
            auto it = grid.find(cellKey(gcx + dx, gcy + dy));
            if (it != grid.end()) {
                for (int idx : it->second) {
                    double ddx = sats[idx].x - cx;
                    double ddy = sats[idx].y - cy;
                    if (ddx*ddx + ddy*ddy <= r*r + 1e-9)
                        result.push_back(idx);
                }
            }
        }
    }
    return result;
}

// Density: count neighbours within DENSITY_R, capped at DENSITY_CAP
int computeDensity(int idx) {
    double cx = sats[idx].x, cy = sats[idx].y;
    int gcx = (int)floor(cx / CELL_SIZE);
    int gcy = (int)floor(cy / CELL_SIZE);
    int count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            auto it = grid.find(cellKey(gcx + dx, gcy + dy));
            if (it != grid.end()) {
                for (int jdx : it->second) {
                    if (jdx == idx) continue;
                    double ddx = sats[jdx].x - cx;
                    double ddy = sats[jdx].y - cy;
                    if (ddx*ddx + ddy*ddy <= DENSITY_R2) {
                        count++;
                        if (count >= DENSITY_CAP) return count;
                    }
                }
            }
        }
    }
    return count;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> N;
    sats.resize(N);
    for (int i = 0; i < N; i++) {
        cin >> sats[i].x >> sats[i].y;
        sats[i].active  = false;
        sats[i].density = 0;
    }

    for (int i = 0; i < N; i++) insertGrid(i);

    // Precompute density with cap
    int maxDensity = 1;
    for (int i = 0; i < N; i++) {
        sats[i].density = computeDensity(i);
        maxDensity = max(maxDensity, sats[i].density);
    }

    int Q;
    cin >> Q;

    while (Q--) {
        double cx, cy, r;
        cin >> cx >> cy >> r;

        auto cands = queryCandidates(cx, cy, r);

        int chosen = -1;
        // Check active first
        for (int idx : cands) {
            if (sats[idx].active) {
                chosen = idx;
                break;
            }
        }

        if (chosen == -1) {
            double bestScore = -1e18;
            for (int idx : cands) {
                double ddx  = sats[idx].x - cx;
                double ddy  = sats[idx].y - cy;
                double dist = sqrt(ddx*ddx + ddy*ddy);
                double centrality   = (r - dist) / (r + 1e-9);
                double density_norm = (double)sats[idx].density / (double)maxDensity;
                double score = ALPHA * centrality + BETA * density_norm;
                if (score > bestScore) {
                    bestScore = score;
                    chosen    = idx;
                }
            }
        }

        if (!sats[chosen].active)
            sats[chosen].active = true;

        cout << chosen << "\n";
        cout.flush();
    }
    return 0;
}
