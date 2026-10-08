#pragma once

// Standalone window close lifecycle, UI-facing delegation and tests.
#include "application.hpp"
#include "view_core.hpp"
#include <exception>
#include <functional>
#include <memory>
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
#  include "../platform_test_access.hpp"
#endif

namespace ui {

struct StandaloneWindow::Impl final : detail::WindowPlatformServices, DispatcherProvider {
    explicit Impl(std::shared_ptr<detail::DispatcherWakeBackend> wake_backend = {})
        : dispatcher_owner(
              std::make_shared<detail::DispatcherOwner>(std::move(wake_backend))),
          lifetime_token(std::make_shared<int>(0)) {}

    // Declared before `core` so reverse member destruction tears the native
    // view down while its dispatcher is still available for deactivation.
    std::shared_ptr<detail::DispatcherOwner> dispatcher_owner;
    std::unique_ptr<detail::ViewCore> core;
    Application* application{};
    std::uint64_t registration_id{};
    std::string construction_error;
    detail::WindowCloseState close_state;
    std::function<CloseDecision()> close_request_callback;
    std::function<void()> closed_callback;
    std::shared_ptr<int> lifetime_token;
    detail::WindowControlPostState close_request_post;
    detail::WindowControlPostState close_completion_post;
    bool close_request_callback_active{};

    [[nodiscard]] Dispatcher dispatcher() const noexcept override {
        return dispatcher_owner ? dispatcher_owner->dispatcher() : Dispatcher{};
    }

    void set_text_input(bool active, Rect area, float cursor_offset) override {
        if (core) core->set_text_input(active, area, cursor_offset);
    }
    void set_clipboard_text(std::string_view text) override {
        if (core) core->set_clipboard_text(text);
    }
    void request_clipboard_text() override {
        if (core) core->request_clipboard_text();
    }
    bool accept_drop(std::string_view type, Rect region) override {
        return core && core->accept_drop(type, region);
    }
    void reject_drop(Rect region) override {
        if (core) core->reject_drop(region);
    }
};


} // namespace ui
