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

The current cases cover Canvas geometry, Row layout geometry, Toggle geometry,
and the background/stripe geometry of a Label scene.

The Label scene clips its text to a fixed `(24,24,172,32)` box. It compares the
background above that box and both ends of the stripe, excluding the text box
and framework footer. A synthetic regression checks that glyph changes are
ignored while background and stripe changes still fail. The channel tolerance
remains 2, with no mismatched pixels allowed.

`nativeui_label_tests` separately requires visible colored glyphs and verifies
left/center/right alignment against the current platform's measured text width.
This catches missing or misplaced text without storing platform-specific glyph
shapes as cross-platform expectations.

`label_scene.ppm` was regenerated with the real pinned Skia package after its
earlier validation-renderer baseline was found to contain an incorrect gray
background and no text. Only this baseline was replaced; normal test runs remain
read-only with respect to all baseline files.
