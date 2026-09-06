#pragma once

#include <nativeui/component.hpp>

#include <functional>
#include <utility>

namespace ui {

class UI {
public:
    template <class Root>
    explicit UI(Root&& root)
        : tree_(compile(make_spec(std::forward<Root>(root)))) {
        tree_.mount();
    }

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
