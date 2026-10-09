#pragma once

// Embedded/plugin view ownership and UI-facing delegation.
#include "view_core.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>

namespace ui {

struct EmbeddedView::Impl final : detail::WindowPlatformServices, DispatcherProvider {
    Impl(UI& ui, NativeParentHandle parent, Size size, EmbeddedViewOptions options)
        : dispatcher_owner(std::make_shared<detail::DispatcherOwner>()),
          core(ui,
               *this,
               PUGL_MODULE,
               WindowDesc{"NativeUI Embedded", size, true, std::nullopt, std::nullopt},
               parent, nullptr, {}, detail::NativeViewConstructionFaultStage::AfterViewCreation,
               nullptr, options.initially_visible) {}

    ~Impl() override {
        if (dispatcher_owner) dispatcher_owner->shutdown();
    }

    [[nodiscard]] Dispatcher dispatcher() const noexcept override {
        return dispatcher_owner ? dispatcher_owner->dispatcher() : Dispatcher{};
    }

    void set_text_input(bool active, Rect area, float cursor_offset) override {
        core.set_text_input(active, area, cursor_offset);
    }
    void set_clipboard_text(std::string_view text) override { core.set_clipboard_text(text); }
    void request_clipboard_text() override { core.request_clipboard_text(); }
    bool accept_drop(std::string_view type, Rect region) override {
        return core.accept_drop(type, region);
    }
    void reject_drop(Rect region) override { core.reject_drop(region); }

    // Keep dispatcher alive through detail::ViewCore destruction/deactivation.
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;
    detail::ViewCore core;
};


} // namespace ui
