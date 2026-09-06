# Third-party dependencies

NativeUI does not vendor binary dependency outputs in this source package. They are acquired by CPM at configure time.

- **Pugl** — `lv2/pugl`, pinned to `b7637149ebe53124e5be90559e02a0185bbcbd73`, ISC license. NativeUI compiles the Pugl core and OpenGL backend statically.
- **Skia** — binary static libraries from `olilarkin/skia-builder`, release `chrome/m149`. The builder project is MIT-licensed; Skia itself uses its upstream BSD-style license and bundled third-party licenses.
- **CPM.cmake** — dependency manager bootstrap pinned to 0.43.1 and distributed under its upstream MIT licence.

## Licensing boundary

The NativeUI dual licence in `LICENSE.md` applies only to copyright and other rights that the NativeUI Licensor is entitled to license. It does **not** relicense Pugl, Skia, skia-builder, CPM.cmake or their transitive dependencies.

Consumers must comply with the upstream licence files, notices and attribution requirements provided by those projects and their transitive dependencies. A NativeUI commercial licence removes NativeUI's AGPL obligations for the commercially licensed NativeUI code, but it does not remove independent third-party obligations.

When a dependency version or acquisition method changes, this file must be reviewed and updated before release.

## Pugl drag-offer compatibility note

The pinned Pugl commit declares `puglRejectOffer()` in the public header, but the symbol is currently implemented only by the X11 backend. NativeUI therefore calls it only on X11. On macOS and Windows, an offer that is not accepted is left to the native backend's default rejection path. This avoids an unresolved `_puglRejectOffer` symbol while preserving portable `reject_drop()` semantics.
