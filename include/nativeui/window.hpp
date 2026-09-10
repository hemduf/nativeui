#pragma once

#include <nativeui/dispatcher.hpp>
#include <nativeui/ui.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace ui {

namespace detail {
struct ApplicationPlatformState;
struct ApplicationBackendAccess;
} // namespace detail

using NativeParentHandle = std::uintptr_t;
using NativeViewHandle = std::uintptr_t;

struct WindowDesc {
    std::string title{"NativeUI"};
    Size size{720.0f, 520.0f};
    bool resizable{true};
};

enum class QuitPolicy {
    OnLastWindowClosed,
    ExplicitOnly,
};

/// Explicit owner of the standalone application world/event loop.
///
/// Construction, polling, running and destruction are confined to the
/// platform/UI thread. The object owns exactly one standalone Pugl PROGRAM
/// world; v1 standalone windows attach explicitly to this instance.
class Application final {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;

    int run();
    bool poll(double timeout_seconds = 0.0);

    void request_quit();
    [[nodiscard]] bool quit_requested() const noexcept;

    void set_quit_policy(QuitPolicy policy) noexcept;
    [[nodiscard]] QuitPolicy quit_policy() const noexcept;

private:
    friend class StandaloneWindow;
    friend struct detail::ApplicationBackendAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Declared after Impl intentionally: reverse member destruction tears down
    // source-private platform services (including Linux D-Bus) before the Pugl
    // PROGRAM world and dispatcher backend owned by Impl are destroyed.
    std::unique_ptr<detail::ApplicationPlatformState> platform_state_;
};

/// Standalone native window for one UI instance.
///
/// Construction, use and destruction are confined to the platform/UI thread.
/// The wrapper is intentionally non-movable: its platform implementation keeps
/// a stable non-owning PlatformServices reference to this exact object.
class StandaloneWindow final : public PlatformServices {
public:
    StandaloneWindow(Application& application, UI& ui, WindowDesc desc = {});

    /// Pre-v1 single-window compatibility path. T069 removes this overload
    /// from the 1.0 public API; it never uses a hidden shared Application.
    [[deprecated("Use StandaloneWindow(Application&, UI&, WindowDesc) for the v1 standalone path")]]
    StandaloneWindow(UI& ui, WindowDesc desc = {});
    ~StandaloneWindow() override;

    StandaloneWindow(const StandaloneWindow&) = delete;
    StandaloneWindow& operator=(const StandaloneWindow&) = delete;
    StandaloneWindow(StandaloneWindow&&) = delete;
    StandaloneWindow& operator=(StandaloneWindow&&) = delete;

    /// Prefer Application::run()/poll() for the explicit v1 path. This method
    /// remains only so the pre-v1 constructor can keep source compatibility
    /// until T069 removes legacy per-window loop ownership.
    int run();
    bool poll(double timeout_seconds = -1.0);
    void request_close();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] Dispatcher dispatcher() const noexcept;
    bool set_size(Size logical_size);

    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) override;
    void set_clipboard_text(std::string_view text) override;
    void request_clipboard_text() override;
    bool accept_drop(std::string_view type, Rect region) override;
    void reject_drop(Rect region) override;

private:
    void mark_application_window_closed() noexcept;
    void unregister_from_application() noexcept;

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
    [[nodiscard]] Dispatcher dispatcher() const noexcept;
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
