#pragma once

#include <nativeui/ui.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace ui {

using NativeParentHandle = std::uintptr_t;
using NativeViewHandle = std::uintptr_t;

struct WindowDesc {
    std::string title{"NativeUI"};
    Size size{720.0f, 520.0f};
    bool resizable{true};
};

class StandaloneWindow final : public PlatformServices {
public:
    StandaloneWindow(UI& ui, WindowDesc desc = {});
    ~StandaloneWindow() override;

    StandaloneWindow(const StandaloneWindow&) = delete;
    StandaloneWindow& operator=(const StandaloneWindow&) = delete;
    StandaloneWindow(StandaloneWindow&&) noexcept;
    StandaloneWindow& operator=(StandaloneWindow&&) noexcept;

    int run();
    bool poll(double timeout_seconds = -1.0);
    void request_close();

    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    bool set_size(Size logical_size);

    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) override;
    void set_clipboard_text(std::string_view text) override;
    void request_clipboard_text() override;
    bool accept_drop(std::string_view type, Rect region) override;
    void reject_drop(Rect region) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class EmbeddedView final : public PlatformServices {
public:
    EmbeddedView(UI& ui, NativeParentHandle parent, Size size);
    ~EmbeddedView() override;

    EmbeddedView(const EmbeddedView&) = delete;
    EmbeddedView& operator=(const EmbeddedView&) = delete;
    EmbeddedView(EmbeddedView&&) noexcept;
    EmbeddedView& operator=(EmbeddedView&&) noexcept;

    bool poll(); // always non-blocking
    void request_close();

    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    bool set_size(Size logical_size);

    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) override;
    void set_clipboard_text(std::string_view text) override;
    void request_clipboard_text() override;
    bool accept_drop(std::string_view type, Rect region) override;
    void reject_drop(Rect region) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};



} // namespace ui
