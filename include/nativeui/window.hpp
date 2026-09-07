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

/// Standalone native window for one UI instance.
///
/// Construction, use and destruction are confined to the platform/UI thread.
/// The wrapper is intentionally non-movable: its platform implementation keeps
/// a stable non-owning PlatformServices reference to this exact object.
class StandaloneWindow final : public PlatformServices {
public:
    StandaloneWindow(UI& ui, WindowDesc desc = {});
    ~StandaloneWindow() override;

    StandaloneWindow(const StandaloneWindow&) = delete;
    StandaloneWindow& operator=(const StandaloneWindow&) = delete;
    StandaloneWindow(StandaloneWindow&&) = delete;
    StandaloneWindow& operator=(StandaloneWindow&&) = delete;

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

/// Embedded native child view for one UI/plugin-editor instance.
///
/// Construction, polling, native-view mutation and destruction are confined to
/// the host UI/main thread. `poll()` is non-blocking and this wrapper is
/// intentionally non-movable because the implementation stores a reference to
/// this PlatformServices object.
class EmbeddedView final : public PlatformServices {
public:
    EmbeddedView(UI& ui, NativeParentHandle parent, Size size);
    ~EmbeddedView() override;

    EmbeddedView(const EmbeddedView&) = delete;
    EmbeddedView& operator=(const EmbeddedView&) = delete;
    EmbeddedView(EmbeddedView&&) = delete;
    EmbeddedView& operator=(EmbeddedView&&) = delete;

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
