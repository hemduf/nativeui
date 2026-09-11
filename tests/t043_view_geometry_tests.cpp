#include "test_support.hpp"
#include "../src/detail/view_geometry.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace {

bool close(float a, float b, float epsilon = 0.00001f) {
    return std::fabs(a - b) <= epsilon;
}

bool same(ui::Size a, ui::Size b) {
    return close(a.w, b.w) && close(a.h, b.h);
}

bool same(ui::Point a, ui::Point b) {
    return close(a.x, b.x) && close(a.y, b.y);
}

bool same(ui::Rect a, ui::Rect b) {
    return close(a.x, b.x) && close(a.y, b.y) && close(a.w, b.w) && close(a.h, b.h);
}

void test_scale_validation_and_conversion() {
    using ui::detail::ViewGeometryState;

    for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
        ViewGeometryState geometry{{160.0f, 90.0f}};
        const auto configured = geometry.configure({160.0f * scale, 90.0f * scale}, scale);
        NUI_CHECK(configured.has_value());
        NUI_CHECK(same(*configured, {160.0f, 90.0f}));
        NUI_CHECK(close(geometry.last_valid_scale(), scale));

        const auto physical = geometry.physical_request({100.25f, 50.25f});
        NUI_CHECK(physical.has_value());
        NUI_CHECK(same(*physical,
                       {std::ceil(100.25f * scale), std::ceil(50.25f * scale)}));

        NUI_CHECK(same(ui::detail::physical_to_logical_point({15.0f * scale, 7.0f * scale}, scale),
                       {15.0f, 7.0f}));
    }
}

void test_invalid_scale_retains_last_valid() {
    using ui::detail::ViewGeometryState;
    ViewGeometryState geometry{{100.0f, 50.0f}};

    NUI_CHECK(geometry.configure({150.0f, 75.0f}, 1.5f).has_value());
    NUI_CHECK(close(geometry.last_valid_scale(), 1.5f));

    for (const float invalid : {0.0f,
                                -1.0f,
                                std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        const auto configured = geometry.configure({300.0f, 150.0f}, invalid);
        NUI_CHECK(configured.has_value());
        NUI_CHECK(same(*configured, {200.0f, 100.0f}));
        NUI_CHECK(close(geometry.last_valid_scale(), 1.5f));
    }
}

void test_scale_observation_acceptance_state() {
    using ui::detail::ViewGeometryState;
    ViewGeometryState geometry{{100.0f, 50.0f}};

    NUI_CHECK(geometry.last_scale_observation_valid());
    NUI_CHECK(!geometry.observe_scale(0.0f));
    NUI_CHECK(!geometry.last_scale_observation_valid());
    NUI_CHECK(close(geometry.last_valid_scale(), 1.0f));

    NUI_CHECK(geometry.observe_scale(1.25f));
    NUI_CHECK(geometry.last_scale_observation_valid());
    NUI_CHECK(close(geometry.last_valid_scale(), 1.25f));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    NUI_CHECK(geometry.configure({250.0f, 125.0f}, nan).has_value());
    NUI_CHECK(!geometry.last_scale_observation_valid());
    NUI_CHECK(close(geometry.last_valid_scale(), 1.25f));

    NUI_CHECK(geometry.configure({300.0f, 150.0f}, 1.5f).has_value());
    NUI_CHECK(geometry.last_scale_observation_valid());
    NUI_CHECK(close(geometry.last_valid_scale(), 1.5f));
}

void test_invalid_requests_and_transient_zero_configure() {
    using ui::detail::ViewGeometryState;
    ViewGeometryState geometry{{120.0f, 80.0f}};
    NUI_CHECK(geometry.configure({240.0f, 160.0f}, 2.0f).has_value());
    const auto previous = geometry.logical_size();

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const ui::Size invalid : {ui::Size{0.0f, 10.0f},
                                  ui::Size{10.0f, 0.0f},
                                  ui::Size{-1.0f, 10.0f},
                                  ui::Size{10.0f, -1.0f},
                                  ui::Size{nan, 10.0f},
                                  ui::Size{10.0f, inf}}) {
        NUI_CHECK(!geometry.physical_request(invalid).has_value());
        NUI_CHECK(!geometry.pending_request().has_value());
    }

    NUI_CHECK(!geometry.configure({0.0f, 160.0f}, 1.25f).has_value());
    NUI_CHECK(!geometry.renderable());
    NUI_CHECK(same(geometry.logical_size(), previous));
    NUI_CHECK(close(geometry.last_valid_scale(), 1.25f));

    const auto restored = geometry.configure({250.0f, 125.0f}, 1.25f);
    NUI_CHECK(restored.has_value());
    NUI_CHECK(geometry.renderable());
    NUI_CHECK(same(*restored, {200.0f, 100.0f}));
}

void test_request_bookkeeping_is_configure_authoritative() {
    ui::detail::ViewGeometryState geometry{{100.0f, 60.0f}};
    NUI_CHECK(geometry.configure({150.0f, 90.0f}, 1.5f).has_value());

    const auto physical = geometry.physical_request({101.0f, 61.0f});
    NUI_CHECK(physical.has_value());
    NUI_CHECK(same(*physical, {152.0f, 92.0f}));
    NUI_CHECK(same(geometry.logical_size(), {100.0f, 60.0f}));
    NUI_CHECK(!geometry.pending_request().has_value());

    geometry.record_successful_request({101.0f, 61.0f});
    NUI_CHECK(geometry.pending_request().has_value());
    NUI_CHECK(same(*geometry.pending_request(), {101.0f, 61.0f}));
    NUI_CHECK(same(geometry.logical_size(), {100.0f, 60.0f}));

    const auto configured = geometry.configure({153.0f, 93.0f}, 1.5f);
    NUI_CHECK(configured.has_value());
    NUI_CHECK(same(*configured, {102.0f, 62.0f}));
    NUI_CHECK(!geometry.pending_request().has_value());
}

void test_size_request_echo_does_not_recurse() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    NUI_CHECK(geometry.configure({200.0f, 100.0f}, 2.0f).has_value());

    int native_requests = 0;
    ui::Size requested_physical{};
    NUI_CHECK(ui::detail::submit_logical_size_request(
        geometry,
        {120.0f, 60.0f},
        [&](ui::Size physical) {
            ++native_requests;
            requested_physical = physical;
            return true;
        }));
    NUI_CHECK(native_requests == 1);
    NUI_CHECK(same(requested_physical, {240.0f, 120.0f}));
    NUI_CHECK(same(geometry.logical_size(), {100.0f, 50.0f}));
    NUI_CHECK(geometry.pending_request().has_value());
    NUI_CHECK(same(*geometry.pending_request(), {120.0f, 60.0f}));

    int layout_calls = 0;
    const auto configured = ui::detail::apply_authoritative_configure(
        geometry,
        {240.0f, 120.0f},
        2.0f,
        [&](ui::Size logical) {
            ++layout_calls;
            NUI_CHECK(same(logical, {120.0f, 60.0f}));
        });
    NUI_CHECK(configured.has_value());
    NUI_CHECK(layout_calls == 1);
    NUI_CHECK(native_requests == 1);
    NUI_CHECK(!geometry.pending_request().has_value());
    NUI_CHECK(same(geometry.logical_size(), {120.0f, 60.0f}));

    NUI_CHECK(!ui::detail::submit_logical_size_request(
        geometry,
        {0.0f, 60.0f},
        [&](ui::Size) {
            ++native_requests;
            return true;
        }));
    NUI_CHECK(native_requests == 1);

    NUI_CHECK(!ui::detail::submit_logical_size_request(
        geometry,
        {140.0f, 70.0f},
        [&](ui::Size) {
            ++native_requests;
            return false;
        }));
    NUI_CHECK(native_requests == 2);
    NUI_CHECK(!geometry.pending_request().has_value());
    NUI_CHECK(same(geometry.logical_size(), {120.0f, 60.0f}));
}

void test_configure_snapshot_dispatches_exactly_one_layout() {
    ui::detail::ViewGeometryState geometry{{100.0f, 50.0f}};
    int layout_calls = 0;
    ui::Size last_layout{};

    const auto apply = [&](ui::Size physical, float scale, ui::Size expected) {
        const int before = layout_calls;
        const auto configured = ui::detail::apply_authoritative_configure(
            geometry,
            physical,
            scale,
            [&](ui::Size logical) {
                ++layout_calls;
                last_layout = logical;
            });
        NUI_CHECK(configured.has_value());
        NUI_CHECK(layout_calls == before + 1);
        NUI_CHECK(same(*configured, expected));
        NUI_CHECK(same(last_layout, expected));
    };

    // Resize-only snapshot: extent changes, scale stays constant.
    apply({120.0f, 60.0f}, 1.0f, {120.0f, 60.0f});
    // Scale-only snapshot: extent stays constant, scale changes.
    apply({120.0f, 60.0f}, 2.0f, {60.0f, 30.0f});
    // Combined snapshot: both extent and scale change, still one layout.
    apply({300.0f, 150.0f}, 1.5f, {200.0f, 100.0f});

    const int before_invalid = layout_calls;
    const auto invalid = ui::detail::apply_authoritative_configure(
        geometry,
        {0.0f, 150.0f},
        1.5f,
        [&](ui::Size) { ++layout_calls; });
    NUI_CHECK(!invalid.has_value());
    NUI_CHECK(layout_calls == before_invalid);
    NUI_CHECK(same(geometry.logical_size(), {200.0f, 100.0f}));
}

void test_fractional_dirty_and_pointer_conversion() {
    const auto physical = ui::detail::logical_to_physical_covering_rect(
        {0.2f, 1.2f, 10.2f, 4.2f}, 1.5f);
    NUI_CHECK(same(physical, {0.0f, 1.0f, 16.0f, 8.0f}));

    NUI_CHECK(same(ui::detail::physical_to_logical_point({18.75f, 11.25f}, 1.25f),
                   {15.0f, 9.0f}));
}

void test_two_view_scale_isolation() {
    ui::detail::ViewGeometryState a{{100.0f, 50.0f}};
    ui::detail::ViewGeometryState b{{100.0f, 50.0f}};

    NUI_CHECK(a.configure({150.0f, 75.0f}, 1.5f).has_value());
    NUI_CHECK(b.configure({200.0f, 100.0f}, 2.0f).has_value());
    NUI_CHECK(a.configure({150.0f, 75.0f}, 0.0f).has_value());

    NUI_CHECK(close(a.last_valid_scale(), 1.5f));
    NUI_CHECK(close(b.last_valid_scale(), 2.0f));
    NUI_CHECK(same(a.logical_size(), {100.0f, 50.0f}));
    NUI_CHECK(same(b.logical_size(), {100.0f, 50.0f}));
}

void test_preferred_size_epsilon_coalescing_and_reentrancy() {
    ui::detail::PreferredSizeState preferred;
    std::vector<ui::Size> notifications;

    preferred.queue({100.0f, 50.0f});
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) {
        notifications.push_back(size);
        preferred.queue({120.0f, 60.0f});
        NUI_CHECK(!preferred.dispatch_once([&](ui::Size) { NUI_CHECK(false); }));
    }));

    NUI_CHECK(notifications.size() == 1);
    NUI_CHECK(same(notifications.front(), {100.0f, 50.0f}));
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    NUI_CHECK(notifications.size() == 2);
    NUI_CHECK(same(notifications.back(), {120.0f, 60.0f}));

    preferred.queue({120.0001f, 60.0f});
    NUI_CHECK(!preferred.dispatch_once([&](ui::Size) { NUI_CHECK(false); }));

    preferred.queue({120.00011f, 60.0f});
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    NUI_CHECK(notifications.size() == 3);

    preferred.queue({130.0f, 70.0f});
    preferred.queue({140.0f, 80.0f});
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    NUI_CHECK(same(notifications.back(), {140.0f, 80.0f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    preferred.queue({nan, 10.0f});
    NUI_CHECK(!preferred.dispatch_once([&](ui::Size) { NUI_CHECK(false); }));
}

void test_preferred_dispatch_survives_owner_teardown() {
    auto preferred = std::make_unique<ui::detail::PreferredSizeState>();
    preferred->queue({100.0f, 50.0f});

    bool callback_called = false;
    auto* const in_flight = preferred.get();
    NUI_CHECK(in_flight->dispatch_once([&](ui::Size size) {
        callback_called = true;
        NUI_CHECK(same(size, {100.0f, 50.0f}));
        preferred.reset();
    }));

    NUI_CHECK(callback_called);
    NUI_CHECK(!preferred);
}

void test_pugl_view_span_rounding_and_limit() {
    using ui::detail::physical_to_pugl_view_span;

    NUI_CHECK(physical_to_pugl_view_span(1.0f) == 1U);
    NUI_CHECK(physical_to_pugl_view_span(1.01f) == 2U);
    NUI_CHECK(physical_to_pugl_view_span(9999.0f) == 9999U);
    NUI_CHECK(physical_to_pugl_view_span(9999.01f) == 10000U);
    NUI_CHECK(physical_to_pugl_view_span(10000.0f) == 10000U);
    NUI_CHECK(physical_to_pugl_view_span(10000.01f) == 10000U);
    NUI_CHECK(physical_to_pugl_view_span(std::numeric_limits<float>::max()) == 10000U);
    NUI_CHECK(physical_to_pugl_view_span(std::numeric_limits<float>::infinity()) == 10000U);
}

void suite() {
    test_scale_validation_and_conversion();
    test_invalid_scale_retains_last_valid();
    test_scale_observation_acceptance_state();
    test_invalid_requests_and_transient_zero_configure();
    test_request_bookkeeping_is_configure_authoritative();
    test_size_request_echo_does_not_recurse();
    test_configure_snapshot_dispatches_exactly_one_layout();
    test_fractional_dirty_and_pointer_conversion();
    test_two_view_scale_isolation();
    test_preferred_size_epsilon_coalescing_and_reentrancy();
    test_preferred_dispatch_survives_owner_teardown();
    test_pugl_view_span_rounding_and_limit();
}

} // namespace

int main() {
    return test::run("t043_view_geometry_tests", suite);
}
