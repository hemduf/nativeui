#include "src/detail/platform_test_access.hpp"

#include <nativeui/nativeui.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke accessibility] " << stage << ": " << message << '\n';
    return 1;
}

[[nodiscard]] bool near(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

[[nodiscard]] bool same_geometry(const ui::detail::SemanticNativeGeometry& left,
                                 const ui::detail::SemanticNativeGeometry& right) {
    return near(left.scale, right.scale) &&
           near(left.physical_screen_origin.x, right.physical_screen_origin.x) &&
           near(left.physical_screen_origin.y, right.physical_screen_origin.y);
}

[[nodiscard]] bool exactly_bounds_changed(
    const std::vector<ui::SemanticChange>& changes) {
    return changes.size() == 1 &&
           changes.front() == ui::SemanticChange::BoundsChanged;
}

[[nodiscard]] std::string change_summary(
    const std::vector<ui::SemanticChange>& changes) {
    std::string summary = "changes=[";
    for (std::size_t i = 0; i < changes.size(); ++i) {
        if (i != 0) summary += ", ";
        summary += std::to_string(static_cast<int>(changes[i]));
    }
    summary += ']';
    return summary;
}

/// Records each per-view native publication delivery so the smoke can prove the
/// production pumps deliver the committed batch exactly once, with the
/// post-drain geometry and the committed generation, and never after a
/// re-entrant view destruction.
class RecordingSink final : public ui::detail::SemanticNativeNotificationSink {
public:
    void on_native_publication(
        const ui::detail::SemanticNativePublicationBatch& batch) override {
        ++calls;
        last_publication = batch.publication;
        last_changes = batch.changes;
        last_native_generation = batch.generation();
        last_semantic_generation = batch.semantic_generation();
        last_geometry = batch.publication
            ? batch.publication->geometry
            : ui::detail::SemanticNativeGeometry{};
    }

    int calls{};
    std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot>
        last_publication;
    std::vector<ui::SemanticChange> last_changes;
    std::uint64_t last_native_generation{};
    std::uint64_t last_semantic_generation{};
    ui::detail::SemanticNativeGeometry last_geometry{};
};

using ui::detail::PlatformTestAccess;
using ui::detail::SemanticPublicationDiagnostics;

[[nodiscard]] bool diagnostics_match_sink(
    const SemanticPublicationDiagnostics& diagnostics,
    const RecordingSink& sink) {
    return diagnostics.has_publication &&
           diagnostics.native_generation == sink.last_native_generation &&
           diagnostics.semantic_generation == sink.last_semantic_generation &&
           same_geometry(diagnostics.geometry, sink.last_geometry);
}

int run_application_standalone() {
    const char* stage = "application-construct";
    ui::Application application;
    if (!application.valid()) {
        return fail(stage, application.last_error());
    }

    ui::UI app_ui{
        ui::Column{ui::Label{"NativeUI accessibility smoke"}}.padding(12.0f)};
    auto window = std::make_unique<ui::StandaloneWindow>(
        application,
        app_ui,
        ui::WindowDesc{.title = "NativeUI accessibility smoke",
                       .size = {320.0f, 180.0f},
                       .resizable = true});
    if (!window->valid()) {
        return fail(stage, window->last_error());
    }

    stage = "application-install-sink";
    auto sink = std::make_shared<RecordingSink>();
    if (!PlatformTestAccess::install_semantic_notification_sink(*window, sink)) {
        return fail(stage, "window has no live semantic domain");
    }

    stage = "application-initial-publication";
    for (int i = 0; i < 4 && !window->should_close(); ++i) {
        (void)application.poll(0.0);
    }
    if (sink->calls == 0) {
        return fail(stage, "initial semantic publication never reached the sink");
    }
    const auto initial = PlatformTestAccess::semantic_publication_diagnostics(*window);
    if (!diagnostics_match_sink(initial, *sink)) {
        return fail(stage, "sink delivery does not match the committed publication");
    }
    if (!window->last_error().empty()) return fail(stage, window->last_error());

    // Accepted T065 work changes the T043 scale/origin, and the Application
    // pump must capture it only after the drain, publish exactly one
    // bounds-only generation and deliver that exact batch once.
    stage = "application-post-drain-geometry";
    const int delivered_before = sink->calls;
    const float injected_scale = 1.75f;
    const ui::Point injected_origin{123.0f, -45.0f};
    if (!window->dispatcher().post([&window, injected_scale, injected_origin] {
            (void)PlatformTestAccess::observe_native_scale(*window, injected_scale);
            (void)PlatformTestAccess::observe_native_physical_screen_origin(
                *window, injected_origin);
        })) {
        return fail(stage, "dispatcher rejected the geometry update");
    }
    (void)application.poll(0.0);
    if (sink->calls != delivered_before + 1) {
        return fail(stage, "expected exactly one native notification delivery");
    }
    if (!exactly_bounds_changed(sink->last_changes)) {
        return fail(stage, change_summary(sink->last_changes));
    }
    if (!near(sink->last_geometry.scale, injected_scale) ||
        !near(sink->last_geometry.physical_screen_origin.x, injected_origin.x) ||
        !near(sink->last_geometry.physical_screen_origin.y, injected_origin.y)) {
        return fail(stage, "sink did not observe the post-drain geometry");
    }
    if (sink->last_native_generation != initial.native_generation + 1 ||
        sink->last_semantic_generation != initial.semantic_generation) {
        return fail(stage, "geometry-only publication changed the logical semantic generation");
    }
    const auto moved = PlatformTestAccess::semantic_publication_diagnostics(*window);
    if (!diagnostics_match_sink(moved, *sink)) {
        return fail(stage, "committed publication and sink delivery diverged");
    }

    // An invalid platform origin fails closed: the previous committed geometry
    // stays authoritative and no new native notification is manufactured.
    stage = "application-invalid-origin";
    const int calls_before_invalid = sink->calls;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (!window->dispatcher().post([&window, nan] {
            (void)PlatformTestAccess::observe_native_physical_screen_origin(
                *window, {nan, nan});
            (void)PlatformTestAccess::observe_native_scale(*window, -1.0f);
        })) {
        return fail(stage, "dispatcher rejected the invalid geometry observation");
    }
    (void)application.poll(0.0);
    const auto after_invalid = PlatformTestAccess::semantic_publication_diagnostics(*window);
    if (sink->calls != calls_before_invalid) {
        return fail(stage, "invalid platform geometry produced a native notification");
    }
    if (!same_geometry(after_invalid.geometry, moved.geometry) ||
        after_invalid.native_generation != moved.native_generation) {
        return fail(stage, "invalid platform geometry replaced the retained origin");
    }

    // Destroying the owning window re-entrantly during the T065 drain must not
    // reach the sink and must not touch destroyed view state.
    stage = "application-reentrant-destruction";
    const int calls_before_destruction = sink->calls;
    if (!window->dispatcher().post([&window] { window.reset(); })) {
        return fail(stage, "dispatcher rejected the destruction callback");
    }
    (void)application.poll(0.0);
    if (window != nullptr) {
        return fail(stage, "destruction callback did not release the window");
    }
    if (sink->calls != calls_before_destruction) {
        return fail(stage, "retired view still delivered a native notification");
    }
    return 0;
}

int run_legacy_standalone() {
    const char* stage = "legacy-construct";
    ui::UI app_ui{
        ui::Column{ui::Label{"NativeUI legacy accessibility smoke"}}.padding(12.0f)};
    auto window = PlatformTestAccess::make_unmanaged_standalone_window(
        app_ui,
        ui::WindowDesc{.title = "NativeUI legacy accessibility smoke",
                       .size = {320.0f, 180.0f},
                       .resizable = true});
    if (!window->valid()) {
        return fail(stage, window->last_error());
    }

    stage = "legacy-install-sink";
    auto sink = std::make_shared<RecordingSink>();
    if (!PlatformTestAccess::install_semantic_notification_sink(*window, sink)) {
        return fail(stage, "window has no live semantic domain");
    }

    stage = "legacy-initial-publication";
    for (int i = 0; i < 4 && !window->should_close(); ++i) {
        (void)window->poll(0.0);
    }
    if (sink->calls == 0) {
        return fail(stage, "initial semantic publication never reached the sink");
    }
    const auto initial = PlatformTestAccess::semantic_publication_diagnostics(*window);
    if (!diagnostics_match_sink(initial, *sink)) {
        return fail(stage, "sink delivery does not match the committed publication");
    }
    if (!window->last_error().empty()) return fail(stage, window->last_error());

    stage = "legacy-post-drain-geometry";
    const int delivered_before = sink->calls;
    const float injected_scale = 0.5f;
    const ui::Point injected_origin{-12.0f, 240.0f};
    if (!window->dispatcher().post([&window, injected_scale, injected_origin] {
            (void)PlatformTestAccess::observe_native_scale(*window, injected_scale);
            (void)PlatformTestAccess::observe_native_physical_screen_origin(
                *window, injected_origin);
        })) {
        return fail(stage, "dispatcher rejected the geometry update");
    }
    (void)window->poll(0.0);
    if (sink->calls != delivered_before + 1) {
        return fail(stage, "expected exactly one native notification delivery");
    }
    if (!exactly_bounds_changed(sink->last_changes)) {
        return fail(stage, change_summary(sink->last_changes));
    }
    if (!near(sink->last_geometry.scale, injected_scale) ||
        !near(sink->last_geometry.physical_screen_origin.x, injected_origin.x) ||
        !near(sink->last_geometry.physical_screen_origin.y, injected_origin.y)) {
        return fail(stage, "sink did not observe the post-drain geometry");
    }
    if (sink->last_native_generation != initial.native_generation + 1 ||
        sink->last_semantic_generation != initial.semantic_generation) {
        return fail(stage, "geometry-only publication changed the logical semantic generation");
    }

    stage = "legacy-reentrant-destruction";
    const int calls_before_destruction = sink->calls;
    if (!window->dispatcher().post([&window] { window.reset(); })) {
        return fail(stage, "dispatcher rejected the destruction callback");
    }
    (void)window->poll(0.0);
    if (window != nullptr) {
        return fail(stage, "destruction callback did not release the window");
    }
    if (sink->calls != calls_before_destruction) {
        return fail(stage, "retired view still delivered a native notification");
    }
    return 0;
}

int run_embedded() {
    const char* stage = "embedded-parent";
    ui::UI parent_ui{
        ui::Column{ui::Label{"NativeUI embedded accessibility smoke"}}
            .padding(12.0f)};
    auto parent = PlatformTestAccess::make_unmanaged_standalone_window(
        parent_ui,
        ui::WindowDesc{.title = "NativeUI embedded accessibility smoke",
                       .size = {420.0f, 280.0f},
                       .resizable = true});
    if (!parent->valid()) {
        return fail(stage, parent->last_error());
    }

    ui::UI child_ui{
        ui::Column{ui::Label{"Embedded accessibility child"}}.padding(10.0f)};
    stage = "embedded-construct";
    auto child = std::make_unique<ui::EmbeddedView>(
        child_ui, parent->native_handle(), ui::Size{260.0f, 140.0f});
    if (!child->native_handle()) {
        return fail(stage, "embedded native handle is zero");
    }

    stage = "embedded-install-sink";
    auto sink = std::make_shared<RecordingSink>();
    if (!PlatformTestAccess::install_semantic_notification_sink(*child, sink)) {
        return fail(stage, "embedded view has no live semantic domain");
    }

    stage = "embedded-initial-publication";
    for (int i = 0; i < 4 && !child->should_close(); ++i) {
        (void)parent->poll(0.0);
        (void)child->poll();
    }
    if (sink->calls == 0) {
        return fail(stage, "initial semantic publication never reached the sink");
    }
    const auto initial = PlatformTestAccess::semantic_publication_diagnostics(*child);
    if (!diagnostics_match_sink(initial, *sink)) {
        return fail(stage, "sink delivery does not match the committed publication");
    }
    if (!child->last_error().empty()) return fail(stage, child->last_error());

    // The embedded pump re-queries the platform screen origin after the drain.
    // The injected post-drain scale must still be the committed one, and the
    // delivered origin must be finite and exactly the committed pair.
    stage = "embedded-post-drain-geometry";
    const int delivered_before = sink->calls;
    const float injected_scale = 1.25f;
    if (!child->dispatcher().post([&child, injected_scale] {
            (void)PlatformTestAccess::observe_native_scale(*child, injected_scale);
        })) {
        return fail(stage, "dispatcher rejected the scale update");
    }
    (void)child->poll();
    if (sink->calls != delivered_before + 1) {
        return fail(stage, "expected exactly one native notification delivery");
    }
    if (!exactly_bounds_changed(sink->last_changes)) {
        return fail(stage, change_summary(sink->last_changes));
    }
    if (!near(sink->last_geometry.scale, injected_scale)) {
        return fail(stage, "sink did not observe the post-drain scale");
    }
    if (!std::isfinite(sink->last_geometry.physical_screen_origin.x) ||
        !std::isfinite(sink->last_geometry.physical_screen_origin.y)) {
        return fail(stage, "committed physical screen origin is not finite");
    }
    if (sink->last_native_generation != initial.native_generation + 1 ||
        sink->last_semantic_generation != initial.semantic_generation) {
        return fail(stage, "geometry-only publication changed the logical semantic generation");
    }
    const auto moved = PlatformTestAccess::semantic_publication_diagnostics(*child);
    if (!diagnostics_match_sink(moved, *sink)) {
        return fail(stage, "committed publication and sink delivery diverged");
    }

    stage = "embedded-reentrant-destruction";
    const int calls_before_destruction = sink->calls;
    if (!child->dispatcher().post([&child] { child.reset(); })) {
        return fail(stage, "dispatcher rejected the destruction callback");
    }
    (void)child->poll();
    if (child != nullptr) {
        return fail(stage, "destruction callback did not release the embedded view");
    }
    if (sink->calls != calls_before_destruction) {
        return fail(stage, "retired embedded view still delivered a native notification");
    }
    if (!parent->last_error().empty()) return fail(stage, parent->last_error());
    return 0;
}

} // namespace

int main() {
    const char* stage = "application";
    try {
        if (const int result = run_application_standalone()) return result;

        stage = "legacy";
        if (const int result = run_legacy_standalone()) return result;

        stage = "embedded";
        if (const int result = run_embedded()) return result;
        return 0;
    } catch (const std::exception& error) {
        return fail(stage, error.what());
    } catch (...) {
        return fail(stage, "unknown exception");
    }
}
