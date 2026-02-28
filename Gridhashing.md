### Architecture

**Phase 1 – Grid Hashing**

-   Satellites are bucketed into square cells of size 100 (= max R).
-   For a disk query, only the ±1 surrounding cells (~9 cells max) are checked, making each lookup O(k) where k is the number of nearby points — typically very small.

**Density Precomputation**

-   At startup, for every satellite we count how many other satellites lie within radius 100. This is done once using the grid itself, so it's efficient.

**Phase 2 – Per-Query Decision**

1.  **If any active satellite is already inside the disk** → output it (free move, no new activation).
2.  **If not covered** → score every candidate inside the disk using:

```
score(p) = α × centrality + β × density_norm
         = 0.4 × (r - dist)/r  +  0.6 × density/maxDensity
```

-   **Centrality** (α=0.4): points closer to disk center are more likely to be caught by future overlapping disks.
-   **Density** (β=0.6): points in dense neighborhoods represent "hub" regions that are statistically likely to be queried more often.

Pick the highest-scoring candidate and activate it.

### Complexity

| Phase | Cost |
| --- | --- |
| Build grid | O(N) |
| Precompute density | O(N · k) ≈ O(N) for sparse grids |
| Per query | O(k) ≈ O(1) amortized |
| Total | **O(N + Q·k)** |



