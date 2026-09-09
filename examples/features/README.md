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
| T018 | `nativeui_example_t018_drop` | background-window drops and local text-file preview |
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

For a real macOS file-drop check, launch `./build/nativeui_example_t018_drop
--trace-drops`, then drag `/tmp/hello.txt` from Finder onto the panel without
first clicking the destination window. The status should become `Received
hello.txt` and its text should appear. The diagnostic option prints only the
acceptance result, drop byte count and preview byte count, never file contents. Restart any
previously running example after a rebuild: a running process still uses its
old code.

T018 receives the first local regular file regardless of its name or extension.
It offers a text preview only when the inspected contents are valid UTF-8 with
no binary control bytes (tab/line endings are allowed). A binary file still
produces `Received <filename> (no UTF-8 text preview)` and clears the old
preview; lack of a text preview is not drop rejection. This is not an image
viewer. Test an image followed by `/tmp/hello.txt` to verify recovery.

The preview reads up to 64 KiB plus three UTF-8 lookahead bytes and displays
up to 120 bytes. `ui::text::utf8_prefix` shares the renderer's decoder; the
existing text boundary helper keeps the displayed prefix character-aligned.
URI decoding/file I/O and preview policy belong to the example, not the
toolkit's generic drop API. The self-test uses an isolated temporary UTF-8
filename and leaves the user's `/tmp/hello.txt` untouched. The macOS CTest smoke traverses the native
OpenGL/Cocoa/Pugl destination callbacks, but does not synthesize a Finder drag.
