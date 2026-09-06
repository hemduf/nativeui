# NativeUI per-feature examples

Every feature ticket must add an executable example plus automated `--self-test`.

Current examples:

| Ticket | Target | Demonstrates |
|---|---|---|
| T007 | `nativeui_example_t007_constraints` | min/preferred/max layout constraints |
| T008 | `nativeui_example_t008_alignment` | Row cross-axis alignment and distribution |
| T009 | `nativeui_example_t009_flex` | grow/shrink allocation |
| T010 | `nativeui_example_t010_grid` | fixed/flex Grid tracks |
| T011 | `nativeui_example_t011_clipping` | paint clipping/overflow |
| T012 | `nativeui_example_t012_scroll` | generic Scroll layout state/viewport |
| T013 | `nativeui_example_t013_bubbling` | ignored leaf input bubbling to parent |
| T014 | `nativeui_example_t014_focus_scopes` | default focus and trapped reverse traversal |
| T015 | `nativeui_example_t015_pointer_capture` | capture, outside drag and terminal cancellation |
| T016 | `nativeui_example_t016_gestures` | reusable click/drag gesture helpers |
| T017 | `nativeui_example_t017_commands` | portable command/shortcut routing |
| T018 | `nativeui_example_t018_drop` | neutral drag-and-drop offers/data |
| T019 | `nativeui_example_t019_transforms` | Painter save/restore and local 2D transforms |
| T024 | `nativeui_example_t024_goldens` | golden rendering scenes and explicit baseline workflow |
| T025 | `nativeui_example_t025_text_edit_model` | reusable headless text editing model |
| T041 | `nativeui_example_t041_smoke_harness` | real standalone parent + embedded Pugl module lifecycle |

Desktop mode:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/nativeui_example_t015_pointer_capture
```

Automated executable check:

```bash
./build/nativeui_example_t015_pointer_capture --self-test
ctest --test-dir build -R nativeui_example_ --output-on-failure
```
