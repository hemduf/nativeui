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

#define ACCESSIBILITY_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

bool close(float a, float b, float epsilon = 0.00001f) {
    return std::fabs(a - b) <= epsilon;
}

bool same(ui::Point a, ui::Point b) {
    return close(a.x, b.x) && close(a.y, b.y);
}

void capture_source_tracks_only_valid_authoritative_geometry() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    const auto source = geometry.retain_native_geometry_capture_state();

    ACCESSIBILITY_CHECK(source != nullptr);
    ACCESSIBILITY_CHECK(close(source->last_valid_scale(), 1.0f));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {0.0f, 0.0f}));

    ACCESSIBILITY_CHECK(geometry.observe_scale(1.5f));
    ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({-1920.5f, 240.25f}));
    ACCESSIBILITY_CHECK(close(source->last_valid_scale(), 1.5f));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {-1920.5f, 240.25f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    ACCESSIBILITY_CHECK(!geometry.observe_scale(nan));
    ACCESSIBILITY_CHECK(!geometry.observe_physical_screen_origin({nan, 12.0f}));
    ACCESSIBILITY_CHECK(close(source->last_valid_scale(), 1.5f));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {-1920.5f, 240.25f}));
}

void capture_source_is_lifetime_detached_from_view_geometry_owner() {
    std::shared_ptr<const ui::detail::ViewNativeGeometryCaptureState> source;
    {
        ui::detail::ViewGeometryState geometry{{80.0f, 40.0f}};
        ACCESSIBILITY_CHECK(geometry.observe_scale(2.0f));
        ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({320.0f, -180.0f}));
        source = geometry.retain_native_geometry_capture_state();
    }

    ACCESSIBILITY_CHECK(source != nullptr);
    ACCESSIBILITY_CHECK(close(source->last_valid_scale(), 2.0f));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {320.0f, -180.0f}));

    const ui::detail::SemanticNativeBoundsTransform transform{*source};
    const auto geometry = transform.geometry();
    ACCESSIBILITY_CHECK(close(geometry.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(geometry.physical_screen_origin, {320.0f, -180.0f}));
}

void copied_native_transform_does_not_follow_later_source_updates() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    ACCESSIBILITY_CHECK(geometry.observe_scale(1.25f));
    ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({10.0f, 20.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const ui::detail::SemanticNativeBoundsTransform first{*source};

    ACCESSIBILITY_CHECK(geometry.observe_scale(2.0f));
    ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({300.0f, 400.0f}));
    const ui::detail::SemanticNativeBoundsTransform second{*source};

    ACCESSIBILITY_CHECK(close(first.scale(), 1.25f));
    ACCESSIBILITY_CHECK(same(first.physical_screen_origin(), {10.0f, 20.0f}));
    ACCESSIBILITY_CHECK(close(second.scale(), 2.0f));
    ACCESSIBILITY_CHECK(same(second.physical_screen_origin(), {300.0f, 400.0f}));
}

void capture_sources_are_isolated_per_view_geometry() {
    ui::detail::ViewGeometryState first{{100.0f, 50.0f}};
    ui::detail::ViewGeometryState second{{100.0f, 50.0f}};
    const auto first_source = first.retain_native_geometry_capture_state();
    const auto second_source = second.retain_native_geometry_capture_state();

    ACCESSIBILITY_CHECK(first_source.get() != second_source.get());
    ACCESSIBILITY_CHECK(first.observe_scale(1.5f));
    ACCESSIBILITY_CHECK(first.observe_physical_screen_origin({-100.0f, 25.0f}));

    ACCESSIBILITY_CHECK(close(first_source->last_valid_scale(), 1.5f));
    ACCESSIBILITY_CHECK(same(first_source->physical_screen_origin(), {-100.0f, 25.0f}));
    ACCESSIBILITY_CHECK(close(second_source->last_valid_scale(), 1.0f));
    ACCESSIBILITY_CHECK(same(second_source->physical_screen_origin(), {0.0f, 0.0f}));
}

void copying_geometry_never_shares_a_capture_source() {
    ui::detail::ViewGeometryState first{{100.0f, 50.0f}};
    ACCESSIBILITY_CHECK(first.observe_scale(1.25f));
    ACCESSIBILITY_CHECK(first.observe_physical_screen_origin({12.0f, 24.0f}));
    const auto first_source = first.retain_native_geometry_capture_state();

    ui::detail::ViewGeometryState copy = first;
    const auto copy_source = copy.retain_native_geometry_capture_state();
    ACCESSIBILITY_CHECK(first_source.get() != copy_source.get());
    ACCESSIBILITY_CHECK(close(copy_source->last_valid_scale(), 1.25f));
    ACCESSIBILITY_CHECK(same(copy_source->physical_screen_origin(), {12.0f, 24.0f}));

    ACCESSIBILITY_CHECK(first.observe_scale(2.0f));
    ACCESSIBILITY_CHECK(first.observe_physical_screen_origin({100.0f, 200.0f}));
    ACCESSIBILITY_CHECK(close(copy_source->last_valid_scale(), 1.25f));
    ACCESSIBILITY_CHECK(same(copy_source->physical_screen_origin(), {12.0f, 24.0f}));

    copy = first;
    ACCESSIBILITY_CHECK(close(copy_source->last_valid_scale(), 2.0f));
    ACCESSIBILITY_CHECK(same(copy_source->physical_screen_origin(), {100.0f, 200.0f}));
}

void capture_lease_accepts_fresh_platform_origin_without_mutating_source() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    ACCESSIBILITY_CHECK(geometry.observe_scale(1.5f));
    ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({20.0f, 30.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const ui::detail::SemanticNativeGeometryCaptureLease lease{source};

    const auto moved = lease.capture_with_physical_screen_origin(
        ui::Point{-310.0f, 415.0f});
    ACCESSIBILITY_CHECK(close(moved.scale, 1.5f));
    ACCESSIBILITY_CHECK(same(moved.physical_screen_origin, {-310.0f, 415.0f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {20.0f, 30.0f}));

    // Scale remains sourced from the retained geometry state at capture time while
    // the platform origin can refresh independently when an embedded parent
    // moved without a child configure event.
    ACCESSIBILITY_CHECK(geometry.observe_scale(2.0f));
    const auto rescaled = lease.capture_with_physical_screen_origin(
        ui::Point{640.0f, -120.0f});
    ACCESSIBILITY_CHECK(close(rescaled.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(rescaled.physical_screen_origin, {640.0f, -120.0f}));

    // Failed and invalid platform samples fail closed to the source's retained
    // per-view origin rather than publishing NaN/Inf or overwriting the source.
    const auto missing = lease.capture_with_physical_screen_origin(std::nullopt);
    ACCESSIBILITY_CHECK(close(missing.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(missing.physical_screen_origin, {20.0f, 30.0f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto invalid = lease.capture_with_physical_screen_origin(
        ui::Point{nan, 99.0f});
    ACCESSIBILITY_CHECK(close(invalid.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(invalid.physical_screen_origin, {20.0f, 30.0f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {20.0f, 30.0f}));
}

void observation_writer_accepts_valid_origin_into_geometry_state() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    ACCESSIBILITY_CHECK(geometry.observe_scale(1.5f));
    ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({20.0f, 30.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const auto weak_writer = geometry.retain_native_geometry_observation_writer();
    ACCESSIBILITY_CHECK(!weak_writer.expired());

    auto writer = weak_writer.lock();
    ACCESSIBILITY_CHECK(writer != nullptr);
    ACCESSIBILITY_CHECK(writer->observe_physical_screen_origin({-310.5f, 415.25f}));
    ACCESSIBILITY_CHECK(same(geometry.physical_screen_origin(), {-310.5f, 415.25f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {-310.5f, 415.25f}));

    // Non-finite observations fail closed and preserve the exact prior origin.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ACCESSIBILITY_CHECK(!writer->observe_physical_screen_origin({nan, 1.0f}));
    ACCESSIBILITY_CHECK(!writer->observe_physical_screen_origin({1.0f, nan}));
    ACCESSIBILITY_CHECK(same(geometry.physical_screen_origin(), {-310.5f, 415.25f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {-310.5f, 415.25f}));

    // A later finite observation recovers through the same locked handle.
    ACCESSIBILITY_CHECK(writer->observe_physical_screen_origin({7.5f, -9.25f}));
    ACCESSIBILITY_CHECK(same(geometry.physical_screen_origin(), {7.5f, -9.25f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {7.5f, -9.25f}));
}

void locked_observation_writer_detaches_with_geometry_owner() {
    std::weak_ptr<ui::detail::ViewGeometryObservationWriter> weak_writer;
    std::shared_ptr<ui::detail::ViewGeometryObservationWriter> locked;
    {
        ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
        weak_writer = geometry.retain_native_geometry_observation_writer();
        locked = weak_writer.lock();
        ACCESSIBILITY_CHECK(locked != nullptr);
    }

    // The locked handle keeps the detached writer object alive by design; the
    // destroyed owner must still be unreachable through it.
    ACCESSIBILITY_CHECK(!weak_writer.expired());
    ACCESSIBILITY_CHECK(locked != nullptr);
    ACCESSIBILITY_CHECK(!locked->observe_physical_screen_origin({1.0f, 2.0f}));
    locked.reset();
    ACCESSIBILITY_CHECK(weak_writer.expired());
}

void retaining_capture_accepts_platform_origin_into_t043_state() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    ACCESSIBILITY_CHECK(geometry.observe_scale(1.5f));
    ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({20.0f, 30.0f}));
    const auto source = geometry.retain_native_geometry_capture_state();
    const auto writer = geometry.retain_native_geometry_observation_writer();
    const ui::detail::SemanticNativeGeometryCaptureLease lease{source};

    int platform_queries = 0;
    ui::Point platform_origin{-310.0f, 415.0f};
    const ui::detail::DeferredSemanticNativeGeometryCapture capture{
        lease,
        writer,
        [&platform_queries, &platform_origin]() noexcept -> std::optional<ui::Point> {
            ++platform_queries;
            return platform_origin;
        }};

    ACCESSIBILITY_CHECK(platform_queries == 0);

    // The retained state can advance after the capture object is prepared. The
    // post-drain checkpoint must accept the newest platform origin into that
    // state, pair it with the newest retained scale, and return exactly the pair
    // that later captures observe.
    ACCESSIBILITY_CHECK(geometry.observe_scale(2.0f));
    const auto captured = capture();

    ACCESSIBILITY_CHECK(platform_queries == 1);
    ACCESSIBILITY_CHECK(close(captured.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(captured.physical_screen_origin, {-310.0f, 415.0f}));
    ACCESSIBILITY_CHECK(same(geometry.physical_screen_origin(), {-310.0f, 415.0f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {-310.0f, 415.0f}));
    ACCESSIBILITY_CHECK(source->last_valid_scale() == captured.scale);
    ACCESSIBILITY_CHECK(lease() == captured);

    // A repeated configure moves the platform origin. Exactly the new origin is
    // retained and returned; nothing is translated twice.
    platform_origin = {64.0f, -8.5f};
    const auto moved = capture();
    ACCESSIBILITY_CHECK(platform_queries == 2);
    ACCESSIBILITY_CHECK(close(moved.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(moved.physical_screen_origin, {64.0f, -8.5f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {64.0f, -8.5f}));

    // Missing and non-finite observations fail closed to the exact retained pair
    // and never mutate the authority.
    const ui::detail::DeferredSemanticNativeGeometryCapture missing{
        lease,
        writer,
        []() noexcept -> std::optional<ui::Point> { return std::nullopt; }};
    const auto fallback = missing();
    ACCESSIBILITY_CHECK(close(fallback.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(fallback.physical_screen_origin, {64.0f, -8.5f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const ui::detail::DeferredSemanticNativeGeometryCapture invalid{
        lease,
        writer,
        [nan]() noexcept -> std::optional<ui::Point> {
            return ui::Point{nan, 99.0f};
        }};
    const auto rejected = invalid();
    ACCESSIBILITY_CHECK(close(rejected.scale, 2.0f));
    ACCESSIBILITY_CHECK(same(rejected.physical_screen_origin, {64.0f, -8.5f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {64.0f, -8.5f}));

    // A later valid observation recovers and is retained exactly.
    platform_origin = {-12.25f, 240.5f};
    const auto recovered = capture();
    ACCESSIBILITY_CHECK(same(recovered.physical_screen_origin, {-12.25f, 240.5f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {-12.25f, 240.5f}));
}

void retaining_capture_fails_closed_after_view_death() {
    std::shared_ptr<const ui::detail::ViewNativeGeometryCaptureState> source;
    std::weak_ptr<ui::detail::ViewGeometryObservationWriter> writer;
    {
        ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
        ACCESSIBILITY_CHECK(geometry.observe_scale(1.5f));
        ACCESSIBILITY_CHECK(geometry.observe_physical_screen_origin({20.0f, 30.0f}));
        source = geometry.retain_native_geometry_capture_state();
        writer = geometry.retain_native_geometry_observation_writer();
    }
    ACCESSIBILITY_CHECK(writer.expired());

    // A re-entrant view death leaves the detached source readable but must never
    // accept a platform observation into destroyed view state.
    const ui::detail::SemanticNativeGeometryCaptureLease lease{source};
    bool queried = false;
    const ui::detail::DeferredSemanticNativeGeometryCapture capture{
        lease,
        writer,
        [&queried]() noexcept -> std::optional<ui::Point> {
            queried = true;
            return ui::Point{999.0f, 777.0f};
        }};

    const auto captured = capture();
    ACCESSIBILITY_CHECK(queried);
    ACCESSIBILITY_CHECK(close(captured.scale, 1.5f));
    ACCESSIBILITY_CHECK(same(captured.physical_screen_origin, {20.0f, 30.0f}));
    ACCESSIBILITY_CHECK(same(source->physical_screen_origin(), {20.0f, 30.0f}));
}

void suite() {
    capture_source_tracks_only_valid_authoritative_geometry();
    capture_source_is_lifetime_detached_from_view_geometry_owner();
    copied_native_transform_does_not_follow_later_source_updates();
    capture_sources_are_isolated_per_view_geometry();
    copying_geometry_never_shares_a_capture_source();
    capture_lease_accepts_fresh_platform_origin_without_mutating_source();
    observation_writer_accepts_valid_origin_into_geometry_state();
    locked_observation_writer_detaches_with_geometry_owner();
    retaining_capture_accepts_platform_origin_into_t043_state();
    retaining_capture_fails_closed_after_view_death();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS accessibility semantic native geometry capture\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL accessibility semantic native geometry capture: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
