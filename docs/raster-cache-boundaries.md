# Retained raster-cache content boundaries

This is the private retained foundation for dependency-driven raster memoization.
It does not introduce `CachedLayer`, allocate an offscreen surface, skip painting,
or implement a GPU cache. Those renderer and public-API steps are tracked by the
NativeUI 1.1 raster-memoization work items.

## Ownership and identity

A Tree stores sparse `RasterCacheEpoch` records only for explicitly registered,
live retained node identities. These records contain no image, graphics context,
component pointer, or process-global state. Registration may allocate; lookup,
invalidation, capture, and publication of an existing epoch do not allocate.

A content token contains a weak lifetime identity and a checked generation. It
cannot keep a removed node or its Tree alive. Retiring a mounted node removes its
record before teardown callbacks execute. Remounting even the same numeric NodeId
creates a different lifetime; stale callbacks and old renderer tokens cannot
alias the new boundary. Independent Trees may use equal NodeIds safely.

The internal `RasterCacheAccess` seam is not a normal consumer API. Future
renderers must validate both the content token and their own per-view image,
device/raster signature, and context lifetime before reusing pixels. A current
content token alone is not proof of a cache hit.

## Invalidation and publication

A fresh boundary is stale. Capturing its content generation does not publish it.
Only a successful explicit commit of that unchanged capture makes it current.
The renderer remains responsible for committing only after successful complete
rasterization and checked submission.

Content changes revoke containing boundaries before fallible notification or
retained work. Repeated changes coalesce while stale and not yet captured. A
change after capture advances the generation, even if it was already stale, so
an in-flight result cannot consume a newer invalidation. Failed or abandoned
updates never implicitly commit. Exhausting the generation counter disables
reuse for that lifetime instead of wrapping into an old identity.

The retained integration covers node paint/layout invalidators, explicit boundary
invalidators, dynamic-source notifications, theme propagation, availability, and
activation/deactivation. Inner changes revoke outer cached content; invalidating
only an outer component does not inherently change inner content. An ordinary
scene repaint is distinct from an identified content mutation.

A retained boundary invalidator carries only the existing weak Tree lifetime,
NodeId, and weak boundary identity. It schedules ordinary scene damage for the
whole affected visual subtree; it never paints synchronously. Publishing an epoch
does not clear scene damage or commit the persistent scene transaction.

## Geometry

Layout records old/new geometry before mutation. Local size changes revoke the
boundary and containing caches. Placement changes relative to a parent revoke
containing caches without inherently changing the moved boundary's local content.
A common translation of a complete subtree preserves its inner content epochs.

Queries reject provisional layout and pending structural/availability preparation.
Failed layout restores existing geometry through the normal transaction journal;
conservatively revoked epochs remain revoked and cannot hide callback invalidation.

This distinction is only retained metadata. Later renderer work must additionally
handle raster scale, fractional device placement, transforms, visual outsets,
clipping, effects, and any scene-dependent paint inputs before granting reuse.
No pixel-equivalence or translation-speedup claim is made by this foundation alone.

## Tests

The normal NativeUI build registers both epoch and real Tree integration tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build
ctest --test-dir build --output-on-failure -L raster-cache
```

The isolated epoch contracts can also run without Skia, Pugl, or a display:

```sh
cmake -S tests/raster_cache -B build-raster-epoch -DCMAKE_BUILD_TYPE=Release
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build-raster-epoch
ctest --test-dir build-raster-epoch --output-on-failure
```

Add `-DNATIVEUI_ENABLE_SANITIZERS=ON` with Clang or GCC for the isolated sanitizer
configuration. This isolated run is not a substitute for the real Tree integration
suite, the full relevant CTest suite, or supported-platform qualification.
