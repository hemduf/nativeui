#include "../../src/detail/semantic_macos_appkit.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

void check_near(double actual, double expected) {
    constexpr double epsilon = 1.0e-9;
    if (std::fabs(actual - expected) > epsilon) {
        throw std::runtime_error("check_near failed");
    }
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot(
    std::uint64_t generation,
    ui::SemanticId node_id,
    std::string name) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = node_id;

    ui::SemanticNodeSnapshot node;
    node.id = node_id;
    node.info.role = ui::SemanticRole::Slider;
    node.info.name = std::move(name);
    node.bounds = {1.0f, 2.0f, 30.0f, 12.0f};
    snapshot->nodes.push_back(std::move(node));
    return snapshot;
}

Class test_anchor_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

void accessibility_frame_tracks_current_atomic_geometry() {
    ui::detail::SemanticNativePublicationState publication_state;
    constexpr ui::SemanticId node_id = 101U;

    auto first_batch = publication_state.publish(
        ordinary_snapshot(1U, node_id, "frame"),
        {ui::SemanticChange::StructureChanged},
        {2.0f, {-120.0f, 64.0f}});
    CHECK(first_batch.has_value());

    auto state = ui::detail::MacOSAccessibilityProxyState::ordinary(
        publication_state.reader_source(), node_id);
    CHECK(state.has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_appkit_frame_555555555555_PuglWrapperView");
    NSAccessibilityElement* element =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*state));
    CHECK(element != nil);

    NSScreen* const main_screen = [NSScreen mainScreen];
    CHECK(main_screen != nil);
    const double main_height = static_cast<double>([main_screen frame].size.height);
    CHECK(std::isfinite(main_height));
    CHECK(main_height > 0.0);

    const NSRect first = [element accessibilityFrame];
    check_near(static_cast<double>(first.origin.x), -59.0);
    check_near(static_cast<double>(first.origin.y), main_height - 46.0);
    check_near(static_cast<double>(first.size.width), 30.0);
    check_near(static_cast<double>(first.size.height), 12.0);

    auto second_batch = publication_state.publish(
        ordinary_snapshot(2U, node_id, "frame newer"),
        {ui::SemanticChange::ValueChanged},
        {1.5f, {320.0f, 180.0f}});
    CHECK(second_batch.has_value());

    const NSRect second = [element accessibilityFrame];
    check_near(static_cast<double>(second.origin.x), 214.0);
    check_near(static_cast<double>(second.origin.y), main_height - 134.0);
    check_near(static_cast<double>(second.size.width), 46.0 / 1.5);
    check_near(static_cast<double>(second.size.height), 12.0);

    publication_state.shutdown();
    const NSRect defunct = [element accessibilityFrame];
    check_near(static_cast<double>(defunct.origin.x), 0.0);
    check_near(static_cast<double>(defunct.origin.y), 0.0);
    check_near(static_cast<double>(defunct.size.width), 0.0);
    check_near(static_cast<double>(defunct.size.height), 0.0);
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            accessibility_frame_tracks_current_atomic_geometry();
            std::cout << "PASS semantic macOS AppKit frame callback\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL semantic macOS AppKit frame callback: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
