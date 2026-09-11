#pragma once

#include <nativeui/component.hpp>
#include <nativeui/theme.hpp>

#include <functional>
#include <string>
#include <utility>

namespace ui {

/// One retained NativeUI component tree.
///
/// UI is intentionally confined to the host/platform UI thread. Its retained
/// tree, focus/input routing, invalidation, layout and painting APIs are not
/// synchronized and must not be called directly from a VST3/CLAP audio or
/// worker thread. Plug-in adapters must transfer cross-thread state through an
/// explicitly reviewed thread-safe bridge and apply it from the UI domain.
class UI {
public:
    template <class Root>
    explicit UI(Root&& root)
        : UI(std::forward<Root>(root), default_theme()) {}

    template <class Root>
    UI(Root&& root, Theme theme)
        : tree_(compile(make_spec(std::forward<Root>(root)))) {
        tree_.set_theme(std::move(theme));
        tree_.mount();
    }

    [[nodiscard]] const Theme& theme() const noexcept { return tree_.theme(); }
    void set_theme(Theme theme) { tree_.set_theme(std::move(theme)); }

    [[nodiscard]] ChildMetrics measure(const Constraints& constraints = Constraints::unbounded()) const {
        return tree_.measure(constraints);
    }
    void resize(Size viewport) { tree_.layout(viewport); }
    void activate(PlatformServices& platform) { tree_.activate_focus(platform); }
    void deactivate(PlatformServices& platform) { tree_.deactivate_focus(platform); }
    void refresh_focus(PlatformServices& platform) { tree_.refresh_focus(platform); }
    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        return tree_.dispatch(event, platform);
    }
    EventResult cancel_pointer(PlatformServices& platform) {
        return tree_.cancel_pointer(platform);
    }
    void set_command_handler(std::function<EventResult(Command)> handler) {
        tree_.set_global_command_handler(std::move(handler));
    }
    [[nodiscard]] bool dirty() const noexcept { return tree_.dirty(); }
    [[nodiscard]] bool layout_dirty() const noexcept { return tree_.layout_dirty(); }
    [[nodiscard]] bool paint_dirty() const noexcept { return tree_.paint_dirty(); }
    [[nodiscard]] const std::vector<Rect>& dirty_regions() const noexcept {
        return tree_.dirty_regions();
    }
    [[nodiscard]] const std::string& structural_diagnostic() const noexcept {
        return tree_.structural_diagnostic();
    }
    [[nodiscard]] std::optional<ComponentAvailability> component_availability(
        NodeId id) const noexcept {
        return tree_.component_availability(id);
    }
    void set_invalidation_callback(std::function<void(Rect)> callback) {
        tree_.set_invalidation_callback(std::move(callback));
    }
    void set_invalidation_callback(std::function<void()> callback) {
        tree_.set_invalidation_callback(std::move(callback));
    }
    void clear_invalidation_callback() {
        tree_.set_invalidation_callback(std::function<void(Rect)>{});
    }
    void invalidate() { tree_.invalidate(); }
    void invalidate(Rect rect) { tree_.invalidate(rect); }
    void invalidate_layout() { tree_.invalidate_layout(); }
    void paint(SkCanvas& canvas, PlatformServices& platform) { tree_.paint(canvas, platform); }

private:
    Tree tree_;
};


using PluginUI = UI; // compatibility alias for the original POC


} // namespace ui
