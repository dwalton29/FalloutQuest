# World streaming checks

Run the portable residency planner checks without Android or game assets:

```sh
cmake -S tests/world -B build/host-world
cmake --build build/host-world
ctest --test-dir build/host-world --output-on-failure
```

Checks cover the 5x5 resident set, complete 7x7 warm buffer, directional
seven-CELL lookahead strips, edge thresholds, negative grids, transitions,
independent planner instances and movement across CELL boundaries.

Scene preparation checks additionally cover complete worker-result publication,
overlapping-job rejection, failed/throwing jobs, restart, cooperative cancellation
and worker joins during reset/destruction.

Placement/index extraction is additionally compared against the previous
worldspace loader using the supplied Fallout3.esm during development. Game
assets and the temporary comparison executables are not repository contents.
