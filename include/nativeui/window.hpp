#pragma once

#include <nativeui/dispatcher.hpp>
#include <nativeui/ui.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ui {

using NativeParentHandle = std::uintptr_t;
using NativeViewHandle = std::uintptr_t;
using PreferredSizeCallback = std::function<void(Size)>;

struct WindowDesc {
    std::string title{"NativeUI"};
    Size size{720.0f, 520.0f};
    bool resizable{true};
    std::optional<Size> min_size;
    std::optional<Size> max_size;
};

enum class QuitPolicy {
    OnLastWindowClosed,
    ExplicitOnly,
};

enum class CloseDecision {
    Accept,
    Cancel,
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
    struct Impl;
    std::unique_ptr<Impl> impl_;
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

    /// Programmatic accepted close. This bypasses the user-close veto callback
    /// and completes at a safe platform checkpoint after the current callback
    /// unwinds.
    void request_close();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] bool is_closed() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] Dispatcher dispatcher() const noexcept;

    bool set_title(std::string_view title);
    bool show();
    bool hide();
    bool set_size(Size logical_size);
    bool set_min_size(std::optional<Size> logical_size);
    bool set_max_size(std::optional<Size> logical_size);

    /// Called for native/user close requests only. An empty callback is
    /// equivalent to accepting the close. Programmatic request_close() never
    /// calls this function.
    void on_close_request(std::function<CloseDecision()> callback);

    /// Called exactly once after an accepted close completes while this C++
    /// object remains alive. Direct C++ destruction is deliberately silent.
    void on_closed(std::function<void()> callback);

    /// Advisory logical preferred-size notification for external owners.
    /// The callback runs on the platform/UI thread at a safe top-level
    /// checkpoint and may synchronously call set_size() without recursive
    /// preferred-size notification.
    void set_preferred_size_callback(PreferredSizeCallback callback);

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

    /// Advisory preferred logical size. NativeUI never resizes the embedding
    /// parent; the host may ignore the callback or grant a different child size.
    void set_preferred_size_callback(PreferredSizeCallback callback);

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
