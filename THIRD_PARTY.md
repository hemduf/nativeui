# Third-party dependencies

NativeUI does not vendor binary dependency outputs in this source package. They are acquired by CPM at configure time.

- **Pugl** — `hemduf/pugl`, pinned to `0187a800276776c50a4d4890b3e6de7a100ff876`, ISC license. NativeUI compiles the Pugl core and OpenGL backend statically. This fork retains the upstream Pugl codebase and carries reviewed desktop drag-and-drop fixes required by NativeUI.
- **Skia** — binary static libraries from `olilarkin/skia-builder`, release `chrome/m149`. The builder project is MIT-licensed; Skia itself uses its upstream BSD-style license and bundled third-party licenses.
- **CPM.cmake** — dependency manager bootstrap pinned to 0.43.1 and distributed under its upstream MIT licence.

## Licensing boundary

The NativeUI dual licence in `LICENSE.md` applies only to copyright and other rights that the NativeUI Licensor is entitled to license. It does **not** relicense Pugl, Skia, skia-builder, CPM.cmake or their transitive dependencies.

Consumers must comply with the upstream licence files, notices and attribution requirements provided by those projects and their transitive dependencies. A NativeUI commercial licence removes NativeUI's AGPL obligations for the commercially licensed NativeUI code, but it does not remove independent third-party obligations.

When a dependency version or acquisition method changes, this file must be reviewed and updated before release.

## Pugl drag-and-drop integration note

The pinned Pugl fork implements `puglRejectOffer()` on macOS, Windows and X11, so NativeUI calls the public API directly and no longer carries a platform-specific rejection workaround. The pin fixes the Cocoa drag lifecycle so accepted drag data is delivered once at the actual drop boundary, fixes Windows `WM_DROPFILES` UTF-8 byte accounting and lifetime cleanup, and preserves the portable `PUGL_DATA_OFFER` → accept/reject → `PUGL_DATA` contract on Windows before payload exposure. `WM_DROPFILES` still has no native hover-time negotiation phase, so Windows offer/reject decisions cannot change OS feedback before release, but rejected data is not delivered to NativeUI and accepted data keeps the actual drop coordinates.
