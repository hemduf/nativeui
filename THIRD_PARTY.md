# Third-party dependencies

NativeUI does not vendor binary dependency outputs in this source package. They are acquired by CPM at configure time.

- **Pugl** — `hemduf/pugl`, pinned to `577efc8283281092d7bd96ace1c5d63c11ce0063`, ISC license. NativeUI compiles the Pugl core and OpenGL backend statically. This fork retains the upstream Pugl codebase and carries reviewed desktop drag-and-drop fixes required by NativeUI.
- **Skia** — binary static libraries from `olilarkin/skia-builder`, release `chrome/m149`. The builder project is MIT-licensed; Skia itself uses its upstream BSD-style license and bundled third-party licenses.
- **CPM.cmake** — dependency manager bootstrap pinned to 0.43.1 and distributed under its upstream MIT licence.

## Licensing boundary

The NativeUI dual licence in `LICENSE.md` applies only to copyright and other rights that the NativeUI Licensor is entitled to license. It does **not** relicense Pugl, Skia, skia-builder, CPM.cmake or their transitive dependencies.

Consumers must comply with the upstream licence files, notices and attribution requirements provided by those projects and their transitive dependencies. A NativeUI commercial licence removes NativeUI's AGPL obligations for the commercially licensed NativeUI code, but it does not remove independent third-party obligations.

When a dependency version or acquisition method changes, this file must be reviewed and updated before release.

## Pugl drag-and-drop integration note

The pinned Pugl fork implements `puglRejectOffer()` on macOS, Windows and X11, so NativeUI calls the public API directly and no longer carries a platform-specific rejection workaround. The pin also fixes the Cocoa drag lifecycle so accepted drag data is delivered once at the actual drop boundary, and fixes Windows `WM_DROPFILES` UTF-8 byte accounting. Windows rejection remains a successful bounded no-op for valid general/drag offers because `WM_DROPFILES` has no negotiable pre-drop rejection phase.
