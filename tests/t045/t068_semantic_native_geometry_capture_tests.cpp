#include "../../src/detail/semantic_native_bounds.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

bool close(float a, float b, float epsilon = 0.00001f) {
    return std::fabs(a - b) <= epsilon;
}

bool same(ui::Point a, ui::Point b) {
    return close(a.x, b.x) && close(a.y, b.y);
}

void capture_source_tracks_only_valid_authoritative_geometry() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    const auto source = geometry.retain_native_geometry_capture_state();

    T068_CHECK(source != nullptr);
    T068_CHECK(close(source->last_valid_scale(), 1.0f));
    T068_CHECK(same(source->physical_screen_origin(), {0.0f, 0.0f}));

    T068_CHECK(geometry.observe_scale(1.5f));
    T068_CHECK(geometry.observe_physical_screen_origin({-1920.5f, 240.25f}));
    T068_CHECK(close(source->last_valid_scale(), 1.5f));
    T068_CHECK(same(source->physical_screen_origin(), {-1920.5f, 240.25f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    T068_CHECK(!geometry.observe_scale(nan));
    T068_CHECK(!geometry.observe_physical_screen_origin({nan, 12.0f}));
    T068_CHECK(close(source->last_valid_scale(), 1.5f));
    T068_CHECK(same(source->physical_screen_origin(), {-1920.5f, 240.25f}));
}

void capture_source_is_lifetime_detached_from_view_geometry_owner() {
    std::shared_ptr<const ui::detail::ViewNativeGeometryCaptureState> source;
    {
        ui::detail::ViewGeometryState geometry{{80.0f, 40.0f}};
        T068_CHECK(geometry.observe_scale(2.0f));
        T068_CHECK(geometry.observe_physical_screen_origin({320.0f, -180.0f}));
        source = geometry.retain_native_geometry_capture_state();
    }

    T068_CHECK(source != nullptr);
    T068_CHECK(close(source->last_valid_scale(), 2.0f));
    T068_CHECK(same(source->physical_screen_origin(), {320.0f, -180.0f}));

    const ui::detail::SemanticNativeBoundsTransform transform{*source};
    const auto geometry = transform.geometry();
    T068_CHECK(close(geometry.scale, 2.0f));
    T068_CHECK(same(geometry.physical_screen_origin, {320.0f, -180.0f}));
}

void copied_native_transform_does_not_follow_later_source_updates() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    T068_CHECK(geometry.observe_scale(1.25f));
    T068_CHECK(geometry.observe_physical_screen_origin({10.0f, 20.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const ui::detail::SemanticNativeBoundsTransform first{*source};

    T068_CHECK(geometry.observe_scale(2.0f));
    T068_CHECK(geometry.observe_physical_screen_origin({300.0f, 400.0f}));
    const ui::detail::SemanticNativeBoundsTransform second{*source};

    T068_CHECK(close(first.scale(), 1.25f));
    T068_CHECK(same(first.physical_screen_origin(), {10.0f, 20.0f}));
    T068_CHECK(close(second.scale(), 2.0f));
    T068_CHECK(same(second.physical_screen_origin(), {300.0f, 400.0f}));
}

void capture_sources_are_isolated_per_view_geometry() {
    ui::detail::ViewGeometryState first{{100.0f, 50.0f}};
    ui::detail::ViewGeometryState second{{100.0f, 50.0f}};
    const auto first_source = first.retain_native_geometry_capture_state();
    const auto second_source = second.retain_native_geometry_capture_state();

    T068_CHECK(first_source.get() != second_source.get());
    T068_CHECK(first.observe_scale(1.5f));
    T068_CHECK(first.observe_physical_screen_origin({-100.0f, 25.0f}));

    T068_CHECK(close(first_source->last_valid_scale(), 1.5f));
    T068_CHECK(same(first_source->physical_screen_origin(), {-100.0f, 25.0f}));
    T068_CHECK(close(second_source->last_valid_scale(), 1.0f));
    T068_CHECK(same(second_source->physical_screen_origin(), {0.0f, 0.0f}));
}

void copying_geometry_never_shares_a_capture_source() {
    ui::detail::ViewGeometryState first{{100.0f, 50.0f}};
    T068_CHECK(first.observe_scale(1.25f));
    T068_CHECK(first.observe_physical_screen_origin({12.0f, 24.0f}));
    const auto first_source = first.retain_native_geometry_capture_state();

    ui::detail::ViewGeometryState copy = first;
    const auto copy_source = copy.retain_native_geometry_capture_state();
    T068_CHECK(first_source.get() != copy_source.get());
    T068_CHECK(close(copy_source->last_valid_scale(), 1.25f));
    T068_CHECK(same(copy_source->physical_screen_origin(), {12.0f, 24.0f}));

    T068_CHECK(first.observe_scale(2.0f));
    T068_CHECK(first.observe_physical_screen_origin({100.0f, 200.0f}));
    T068_CHECK(close(copy_source->last_valid_scale(), 1.25f));
    T068_CHECK(same(copy_source->physical_screen_origin(), {12.0f, 24.0f}));

    copy = first;
    T068_CHECK(close(copy_source->last_valid_scale(), 2.0f));
    T068_CHECK(same(copy_source->physical_screen_origin(), {100.0f, 200.0f}));
}

void capture_lease_accepts_fresh_platform_origin_without_mutating_source() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    T068_CHECK(geometry.observe_scale(1.5f));
    T068_CHECK(geometry.observe_physical_screen_origin({20.0f, 30.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const ui::detail::SemanticNativeGeometryCaptureLease lease{source};

    const auto moved = lease.capture_with_physical_screen_origin(
        ui::Point{-310.0f, 415.0f});
    T068_CHECK(close(moved.scale, 1.5f));
    T068_CHECK(same(moved.physical_screen_origin, {-310.0f, 415.0f}));
    T068_CHECK(same(source->physical_screen_origin(), {20.0f, 30.0f}));

    // Scale remains sourced from the retained T043 state at capture time while
    // the platform origin can refresh independently when an embedded parent
    // moved without a child configure event.
    T068_CHECK(geometry.observe_scale(2.0f));
    const auto rescaled = lease.capture_with_physical_screen_origin(
        ui::Point{640.0f, -120.0f});
    T068_CHECK(close(rescaled.scale, 2.0f));
    T068_CHECK(same(rescaled.physical_screen_origin, {640.0f, -120.0f}));

    // Failed and invalid platform samples fail closed to the source's retained
    // per-view origin rather than publishing NaN/Inf or overwriting the source.
    const auto missing = lease.capture_with_physical_screen_origin(std::nullopt);
    T068_CHECK(close(missing.scale, 2.0f));
    T068_CHECK(same(missing.physical_screen_origin, {20.0f, 30.0f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto invalid = lease.capture_with_physical_screen_origin(
        ui::Point{nan, 99.0f});
    T068_CHECK(close(invalid.scale, 2.0f));
    T068_CHECK(same(invalid.physical_screen_origin, {20.0f, 30.0f}));
    T068_CHECK(same(source->physical_screen_origin(), {20.0f, 30.0f}));
}

void deferred_capture_samples_only_when_checkpoint_invokes_it() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    T068_CHECK(geometry.observe_scale(1.25f));
    T068_CHECK(geometry.observe_physical_screen_origin({15.0f, 25.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const ui::detail::SemanticNativeGeometryCaptureLease lease{source};

    int platform_queries = 0;
    ui::Point platform_origin{-480.0f, 720.0f};
    const ui::detail::DeferredSemanticNativeGeometryCapture capture{
        lease,
        [&platform_queries, &platform_origin]() noexcept -> std::optional<ui::Point> {
            ++platform_queries;
            return platform_origin;
        }};

    T068_CHECK(platform_queries == 0);

    // The retained state can advance after the capture object is prepared. The
    // checkpoint must observe the newest scale while querying the native origin
    // only when the deferred callable itself is invoked.
    T068_CHECK(geometry.observe_scale(2.0f));
    T068_CHECK(geometry.observe_physical_screen_origin({40.0f, 60.0f}));
    const auto captured = capture();

    T068_CHECK(platform_queries == 1);
    T068_CHECK(close(captured.scale, 2.0f));
    T068_CHECK(same(captured.physical_screen_origin, {-480.0f, 720.0f}));
    T068_CHECK(same(source->physical_screen_origin(), {40.0f, 60.0f}));

    const ui::detail::DeferredSemanticNativeGeometryCapture failed_capture{
        lease,
        []() noexcept -> std::optional<ui::Point> { return std::nullopt; }};
    const auto fallback = failed_capture();
    T068_CHECK(close(fallback.scale, 2.0f));
    T068_CHECK(same(fallback.physical_screen_origin, {40.0f, 60.0f}));
}

void suite() {
    capture_source_tracks_only_valid_authoritative_geometry();
    capture_source_is_lifetime_detached_from_view_geometry_owner();
    copied_native_transform_does_not_follow_later_source_updates();
    capture_sources_are_isolated_per_view_geometry();
    copying_geometry_never_shares_a_capture_source();
    capture_lease_accepts_fresh_platform_origin_without_mutating_source();
    deferred_capture_samples_only_when_checkpoint_invokes_it();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t068 semantic native geometry capture\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic native geometry capture: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
