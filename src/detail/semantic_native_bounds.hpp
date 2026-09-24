#pragma once

#include <nativeui/detail/semantic_native_publication.hpp>

#include "view_geometry.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ui::detail {

/// Immutable per-view geometry captured on the owning UI thread for native
/// accessibility queries. Semantic snapshots stay in logical view-relative
/// coordinates; platform bridges apply this small value at the native boundary
/// instead of retaining or reading live ViewGeometryState cross-thread.
class SemanticNativeBoundsTransform final {
public:
    explicit SemanticNativeBoundsTransform(SemanticNativeGeometry geometry) noexcept
        : geometry_(geometry) {}

    /// Capture the complete retained T043 native geometry as one copied value.
    /// ViewGeometryState is UI-thread-owned; native readers retain only this
    /// immutable scale/origin pair and never observe later live geometry changes.
    explicit SemanticNativeBoundsTransform(const ViewGeometryState& geometry) noexcept
        : SemanticNativeBoundsTransform(SemanticNativeGeometry{
              geometry.last_valid_scale(), geometry.physical_screen_origin()}) {}

    /// Capture from the lifetime-detached UI-thread source retained by a real
    /// semantic pump. The source itself may outlive ViewCore, but this transform
    /// immediately copies its pair so native readers never observe live mutation.
    explicit SemanticNativeBoundsTransform(
        const ViewNativeGeometryCaptureState& geometry) noexcept
        : SemanticNativeBoundsTransform(SemanticNativeGeometry{
              geometry.last_valid_scale(), geometry.physical_screen_origin()}) {}

    SemanticNativeBoundsTransform(const ViewGeometryState& geometry,
                                  Point physical_screen_origin) noexcept
        : SemanticNativeBoundsTransform(SemanticNativeGeometry{
              geometry.last_valid_scale(), physical_screen_origin}) {}

    [[nodiscard]] float scale() const noexcept { return geometry_.scale; }
    [[nodiscard]] Point physical_screen_origin() const noexcept {
        return geometry_.physical_screen_origin;
    }
    [[nodiscard]] SemanticNativeGeometry geometry() const noexcept {
        return geometry_;
    }

    [[nodiscard]] Rect physical_view_bounds(Rect logical_bounds) const noexcept {
        return logical_to_physical_covering_rect(logical_bounds, geometry_.scale);
    }

    [[nodiscard]] Rect physical_screen_bounds(Rect logical_bounds) const noexcept {
        return logical_to_physical_screen_rect(
            logical_bounds,
            geometry_.scale,
            geometry_.physical_screen_origin);
    }

private:
    SemanticNativeGeometry geometry_{};
};

/// Strong lifetime lease for one view's tiny T043 native-geometry capture
/// source. Pump records may copy this object before entering a re-entrant T065
/// drain without retaining ViewCore, Tree, Node, Component or a native handle.
/// Calling the lease afterwards samples the source's latest UI-thread values and
/// immediately returns an immutable copied pair suitable for native publication.
class SemanticNativeGeometryCaptureLease final {
public:
    explicit SemanticNativeGeometryCaptureLease(
        std::shared_ptr<const ViewNativeGeometryCaptureState> source)
        : source_(std::move(source)) {
        if (!source_) {
            throw std::invalid_argument(
                "NativeUI semantic native geometry capture source is null");
        }
    }

    [[nodiscard]] SemanticNativeGeometry operator()() const noexcept {
        return SemanticNativeBoundsTransform{*source_}.geometry();
    }

    /// Pair the latest retained T043 scale with one authoritative platform
    /// screen-origin sample taken at the current checkpoint. Embedded parents
    /// can move without a child PUGL_CONFIGURE, so their platform adapter needs
    /// this narrow override after T065 drains. Missing or non-finite samples
    /// fail closed to the retained per-view origin and never mutate the source.
    [[nodiscard]] SemanticNativeGeometry capture_with_physical_screen_origin(
        std::optional<Point> captured_origin) const noexcept {
        const auto retained = operator()();
        if (!captured_origin || !valid_physical_screen_origin(*captured_origin)) {
            return retained;
        }
        return SemanticNativeGeometry{retained.scale, *captured_origin};
    }

private:
    std::shared_ptr<const ViewNativeGeometryCaptureState> source_;
};

/// Deferred composition used by native-view pumps that need a fresh platform
/// screen origin. Construction retains only the detached geometry lease and a
/// small no-throw capture callable; the platform query itself does not run until
/// operator() is invoked by the post-dispatch checkpoint. This keeps native
/// handle sampling on the existing UI-thread pump after accepted dispatcher work
/// has drained, while still pairing it with the latest retained scale.
template <class ScreenOriginCapture>
class DeferredSemanticNativeGeometryCapture final {
public:
    DeferredSemanticNativeGeometryCapture(
        SemanticNativeGeometryCaptureLease geometry,
        ScreenOriginCapture screen_origin_capture)
        : geometry_(std::move(geometry)),
          screen_origin_capture_(std::move(screen_origin_capture)) {
        static_assert(
            std::is_nothrow_invocable_r_v<std::optional<Point>,
                                          const ScreenOriginCapture&>,
            "Native screen-origin capture must be noexcept and return optional<Point>");
    }

    [[nodiscard]] SemanticNativeGeometry operator()() const noexcept {
        return geometry_.capture_with_physical_screen_origin(
            screen_origin_capture_());
    }

private:
    SemanticNativeGeometryCaptureLease geometry_;
    ScreenOriginCapture screen_origin_capture_;
};

template <class ScreenOriginCapture>
DeferredSemanticNativeGeometryCapture(
    SemanticNativeGeometryCaptureLease,
    ScreenOriginCapture)
    -> DeferredSemanticNativeGeometryCapture<ScreenOriginCapture>;

} // namespace ui::detail
