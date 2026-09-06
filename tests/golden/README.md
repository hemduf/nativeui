# NativeUI golden rendering tests

Golden baselines are versioned PPM (`P6`) images under `tests/golden/baselines/`.
They are never updated during a normal CTest run.

Update explicitly:

```bash
./build/nativeui_golden_tests --update-goldens
# or
cmake --build build --target nativeui_update_goldens
```

Review baseline diffs before committing them.

## Tolerance policy

- Geometry-only regions use a per-channel tolerance of 0-1.
- Widget goldens that contain text compare deterministic geometry regions only;
  font/glyph pixels are excluded because system font fallback and rasterization can
  vary across macOS, Windows and Linux.
- Anti-aliased edges are avoided unless a ticket explicitly establishes a
  cross-platform tolerance for them.
- A normal mismatch never rewrites the baseline. It writes `<name>.actual.ppm`
  and `<name>.diff.ppm` into the configured golden artifact directory and prints
  mismatch count, ratio, maximum channel delta, and first mismatching pixel.

The current cases cover Canvas geometry, Row layout geometry, and Toggle geometry.
