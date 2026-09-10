#include "../src/detail/view_geometry.hpp"

#include <cassert>
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
        assert(configured.has_value());
        assert(same(*configured, {160.0f, 90.0f}));
        assert(close(geometry.last_valid_scale(), scale));

        const auto physical = geometry.physical_request({100.25f, 50.25f});
        assert(physical.has_value());
        assert(same(*physical,
                    {std::ceil(100.25f * scale), std::ceil(50.25f * scale)}));

        assert(same(ui::detail::physical_to_logical_point({15.0f * scale, 7.0f * scale}, scale),
                    {15.0f, 7.0f}));
    }
}

void test_invalid_scale_retains_last_valid() {
    using ui::detail::ViewGeometryState;
    ViewGeometryState geometry{{100.0f, 50.0f}};

    assert(geometry.configure({150.0f, 75.0f}, 1.5f).has_value());
    assert(close(geometry.last_valid_scale(), 1.5f));

    for (const float invalid : {0.0f,
                                -1.0f,
                                std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        const auto configured = geometry.configure({300.0f, 150.0f}, invalid);
        assert(configured.has_value());
        assert(same(*configured, {200.0f, 100.0f}));
        assert(close(geometry.last_valid_scale(), 1.5f));
    }
}

void test_invalid_requests_and_transient_zero_configure() {
    using ui::detail::ViewGeometryState;
    ViewGeometryState geometry{{120.0f, 80.0f}};
    assert(geometry.configure({240.0f, 160.0f}, 2.0f).has_value());
    const auto previous = geometry.logical_size();

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const ui::Size invalid : {ui::Size{0.0f, 10.0f},
                                  ui::Size{10.0f, 0.0f},
                                  ui::Size{-1.0f, 10.0f},
                                  ui::Size{10.0f, -1.0f},
                                  ui::Size{nan, 10.0f},
                                  ui::Size{10.0f, inf}}) {
        assert(!geometry.physical_request(invalid).has_value());
        assert(!geometry.pending_request().has_value());
    }

    assert(!geometry.configure({0.0f, 160.0f}, 1.25f).has_value());
    assert(!geometry.renderable());
    assert(same(geometry.logical_size(), previous));
    assert(close(geometry.last_valid_scale(), 1.25f));

    const auto restored = geometry.configure({250.0f, 125.0f}, 1.25f);
    assert(restored.has_value());
    assert(geometry.renderable());
    assert(same(*restored, {200.0f, 100.0f}));
}

void test_request_bookkeeping_is_configure_authoritative() {
    ui::detail::ViewGeometryState geometry{{100.0f, 60.0f}};
    assert(geometry.configure({150.0f, 90.0f}, 1.5f).has_value());

    const auto physical = geometry.physical_request({101.0f, 61.0f});
    assert(physical.has_value());
    assert(same(*physical, {152.0f, 92.0f}));
    assert(same(geometry.logical_size(), {100.0f, 60.0f}));
    assert(!geometry.pending_request().has_value());

    geometry.record_successful_request({101.0f, 61.0f});
    assert(geometry.pending_request().has_value());
    assert(same(*geometry.pending_request(), {101.0f, 61.0f}));
    assert(same(geometry.logical_size(), {100.0f, 60.0f}));

    const auto configured = geometry.configure({153.0f, 93.0f}, 1.5f);
    assert(configured.has_value());
    assert(same(*configured, {102.0f, 62.0f}));
    assert(!geometry.pending_request().has_value());
}

void test_fractional_dirty_and_pointer_conversion() {
    const auto physical = ui::detail::logical_to_physical_covering_rect(
        {0.2f, 1.2f, 10.2f, 4.2f}, 1.5f);
    assert(same(physical, {0.0f, 1.0f, 16.0f, 8.0f}));

    assert(same(ui::detail::physical_to_logical_point({18.75f, 11.25f}, 1.25f),
                {15.0f, 9.0f}));
}

void test_two_view_scale_isolation() {
    ui::detail::ViewGeometryState a{{100.0f, 50.0f}};
    ui::detail::ViewGeometryState b{{100.0f, 50.0f}};

    assert(a.configure({150.0f, 75.0f}, 1.5f).has_value());
    assert(b.configure({200.0f, 100.0f}, 2.0f).has_value());
    assert(a.configure({150.0f, 75.0f}, 0.0f).has_value());

    assert(close(a.last_valid_scale(), 1.5f));
    assert(close(b.last_valid_scale(), 2.0f));
    assert(same(a.logical_size(), {100.0f, 50.0f}));
    assert(same(b.logical_size(), {100.0f, 50.0f}));
}

void test_preferred_size_epsilon_coalescing_and_reentrancy() {
    ui::detail::PreferredSizeState preferred;
    std::vector<ui::Size> notifications;

    preferred.queue({100.0f, 50.0f});
    assert(preferred.dispatch_once([&](ui::Size size) {
        notifications.push_back(size);
        preferred.queue({120.0f, 60.0f});
        assert(!preferred.dispatch_once([&](ui::Size) { assert(false); }));
    }));

    assert(notifications.size() == 1);
    assert(same(notifications.front(), {100.0f, 50.0f}));
    assert(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    assert(notifications.size() == 2);
    assert(same(notifications.back(), {120.0f, 60.0f}));

    preferred.queue({120.0001f, 60.0f});
    assert(!preferred.dispatch_once([&](ui::Size) { assert(false); }));

    preferred.queue({120.00011f, 60.0f});
    assert(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    assert(notifications.size() == 3);

    preferred.queue({130.0f, 70.0f});
    preferred.queue({140.0f, 80.0f});
    assert(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    assert(same(notifications.back(), {140.0f, 80.0f}));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    preferred.queue({nan, 10.0f});
    assert(!preferred.dispatch_once([&](ui::Size) { assert(false); }));
}

void test_preferred_dispatch_survives_owner_teardown() {
    auto preferred = std::make_unique<ui::detail::PreferredSizeState>();
    preferred->queue({100.0f, 50.0f});

    bool callback_called = false;
    auto* const in_flight = preferred.get();
    assert(in_flight->dispatch_once([&](ui::Size size) {
        callback_called = true;
        assert(same(size, {100.0f, 50.0f}));
        preferred.reset();
    }));

    assert(callback_called);
    assert(!preferred);
}

} // namespace

int main() {
    test_scale_validation_and_conversion();
    test_invalid_scale_retains_last_valid();
    test_invalid_requests_and_transient_zero_configure();
    test_request_bookkeeping_is_configure_authoritative();
    test_fractional_dirty_and_pointer_conversion();
    test_two_view_scale_isolation();
    test_preferred_size_epsilon_coalescing_and_reentrancy();
    test_preferred_dispatch_survives_owner_teardown();
    return 0;
}
