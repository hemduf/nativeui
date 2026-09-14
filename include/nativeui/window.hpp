#pragma once

#include <nativeui/desktop_services.hpp>
#include <nativeui/dispatcher.hpp>
#include <nativeui/ui.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ui {

namespace detail {
struct ApplicationPlatformState;
struct ApplicationBackendAccess;
} // namespace detail

using NativeParentHandle = std::uintptr_t;
using NativeViewHandle = std::uintptr_t;
using PreferredSizeCallback = std::function<void(Size)>;

struct WindowDesc {
    std::string title{"NativeUI"};
    Size size{720.0f, 520.0f};
    bool resizable{true};
    std::optional<Size> min_size{};
    std::optional<Size> max_size{};
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
/// Construction, polling, running and destruction are confined to the
/// platform/UI thread. The object owns exactly one standalone program world;
/// v1 standalone windows attach explicitly to this instance.
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
    std::unique_ptr<detail::ApplicationPlatformState> platform_state_;
};

/// Standalone native window for one UI instance.
/// Construction, use and destruction are confined to the platform/UI thread.
/// Application is the sole v1 standalone event-loop owner.
class StandaloneWindow final : public PlatformServices, public DispatcherProvider {
public:
    StandaloneWindow(Application& application, UI& ui, WindowDesc desc = {});
    ~StandaloneWindow() override;

    StandaloneWindow(const StandaloneWindow&) = delete;
    StandaloneWindow& operator=(const StandaloneWindow&) = delete;
    StandaloneWindow(StandaloneWindow&&) = delete;
    StandaloneWindow& operator=(StandaloneWindow&&) = delete;

    void request_close();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] bool is_closed() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] Dispatcher dispatcher() const noexcept override;
    [[nodiscard]] DesktopServices& desktop_services();

    bool set_title(std::string_view title);
    bool show();
    bool hide();
    bool set_size(Size logical_size);
    bool set_min_size(std::optional<Size> logical_size);
    bool set_max_size(std::optional<Size> logical_size);

    void on_close_request(std::function<CloseDecision()> callback);
    void on_closed(std::function<void()> callback);
    void set_preferred_size_callback(PreferredSizeCallback callback);

    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) override;
    void set_clipboard_text(std::string_view text) override;
    void request_clipboard_text() override;
    bool accept_drop(std::string_view type, Rect region) override;
    void reject_drop(Rect region) override;

private:
    // Private implementation compatibility only. These declarations keep the
    // existing source implementation linkable while removing the legacy API
    // from the v1 consumer surface. They are not callable/constructible by users.
    StandaloneWindow(UI& ui, WindowDesc desc = {});
    int run();
    bool poll(double timeout_seconds = -1.0);

    void handle_native_close_request();
    void process_native_close_request();
    void schedule_close_completion();
    void complete_close();
    void mark_application_window_closed() noexcept;
    void unregister_from_application() noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<DesktopServicesBackend> desktop_services_backend_;
    std::unique_ptr<DesktopServices> desktop_services_;
};

/// Embedded native child view for one UI/plugin-editor instance.
/// Construction, polling, native-view mutation and destruction are confined to
/// the host UI/main thread. poll() is non-blocking.
class EmbeddedView final : public PlatformServices, public DispatcherProvider {
public:
    EmbeddedView(UI& ui, NativeParentHandle parent, Size size);
    EmbeddedView(UI& ui,
                 NativeParentHandle parent,
                 Size size,
                 std::shared_ptr<DesktopServicesBackend> desktop_services_backend);
    ~EmbeddedView() override;

    EmbeddedView(const EmbeddedView&) = delete;
    EmbeddedView& operator=(const EmbeddedView&) = delete;
    EmbeddedView(EmbeddedView&&) = delete;
    EmbeddedView& operator=(EmbeddedView&&) = delete;

    bool poll();
    void request_close();

    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] Dispatcher dispatcher() const noexcept override;
    [[nodiscard]] DesktopServices& desktop_services();
    bool set_size(Size logical_size);
    void set_preferred_size_callback(PreferredSizeCallback callback);

    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) override;
    void set_clipboard_text(std::string_view text) override;
    void request_clipboard_text() override;
    bool accept_drop(std::string_view type, Rect region) override;
    void reject_drop(Rect region) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<DesktopServicesBackend> desktop_services_backend_;
    std::unique_ptr<DesktopServices> desktop_services_;
};

} // namespace ui
