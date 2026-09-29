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
struct PlatformTestAccess;
} // namespace detail

/// Opaque borrowed native parent identifier supplied by the embedding host.
using NativeParentHandle = std::uintptr_t;
/// Opaque native child-view identifier owned by a live NativeUI view wrapper.
using NativeViewHandle = std::uintptr_t;
/// Host callback receiving an advisory preferred child size in logical pixels.
using PreferredSizeCallback = std::function<void(Size)>;

/// Construction policy for an embedded host-owned child view.
///
/// The option controls local child visibility only. It does not change ownership,
/// polling, host-window visibility, occlusion, focus, or lifetime rules.
struct EmbeddedViewOptions {
    /// Realize the child initially shown when true; realize it hidden when false.
    ///
    /// A hidden child remains constructed and keeps its UI/platform resources.
    bool initially_visible{true};
};

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

    /// Runs the standalone application loop. On Emscripten exactly one
    /// Application::run() may own the module's browser main-loop slot at a time;
    /// additional/external instances remain supported through non-blocking
    /// poll().
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
class StandaloneWindow final : public PlatformServices, public DispatcherProvider {
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
    [[nodiscard]] Dispatcher dispatcher() const noexcept override;
    [[nodiscard]] DesktopServices& desktop_services();

    bool set_title(std::string_view title);
    bool show();
    bool hide();
    bool set_size(Size logical_size);
    bool set_min_size(std::optional<Size> logical_size);
    bool set_max_size(std::optional<Size> logical_size);

    /// Called for native/user close requests only. An empty callback is
    /// equivalent to accepting the close. Programmatic request_close() never
    /// calls this function. Native requests are evaluated at the next safe
    /// Dispatcher checkpoint after the OS callback has unwound.
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
    friend struct detail::PlatformTestAccess;

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
///
/// Construction, polling, native-view mutation and destruction are confined to
/// the host UI/main thread. `poll()` is non-blocking and this wrapper is
/// intentionally non-movable because the implementation stores a reference to
/// this PlatformServices object.
class EmbeddedView final : public PlatformServices, public DispatcherProvider {
public:
    EmbeddedView(UI& ui, NativeParentHandle parent, Size size);
    EmbeddedView(UI& ui,
                 NativeParentHandle parent,
                 Size size,
                 std::shared_ptr<DesktopServicesBackend> desktop_services_backend);
    /// Construct an embedded child with explicit visibility policy.
    ///
    /// `ui` and `parent` are borrowed and must outlive this view. `size` is
    /// the requested child extent in logical pixels. The optional desktop
    /// services backend is shared-owned by the view. Construction and all later
    /// native-view operations are UI/main-thread only and are not audio-RT safe.
    ///
    /// `options.initially_visible == false` realizes the child hidden without
    /// destroying its retained UI or Dispatcher. Native creation failures are
    /// reported through the normal validity/error state rather than by taking
    /// ownership of the host parent.
    EmbeddedView(UI& ui,
                 NativeParentHandle parent,
                 Size size,
                 std::shared_ptr<DesktopServicesBackend> desktop_services_backend,
                 EmbeddedViewOptions options);
    ~EmbeddedView() override;

    EmbeddedView(const EmbeddedView&) = delete;
    EmbeddedView& operator=(const EmbeddedView&) = delete;
    EmbeddedView(EmbeddedView&&) = delete;
    EmbeddedView& operator=(EmbeddedView&&) = delete;

    /// Pump pending embedded-view/platform work without blocking.
    ///
    /// Polling remains host-owned even while the child is hidden. Returns the
    /// platform/view success result; this call does not make a hidden child shown.
    bool poll();

    /// Request local child visibility without raising or showing the host window.
    ///
    /// The realized child, retained UI and Dispatcher are preserved. Showing
    /// invalidates retained presentation so the next frame is fresh. Returns
    /// false after terminal native close/failure; repeated successful show calls
    /// are idempotent.
    bool show();

    /// Hide the local child while preserving its realized resources and UI.
    ///
    /// Hiding deactivates retained focus/input, cancels active pointer edits and
    /// stops IME as part of cleanup. Cancellation callback exceptions propagate
    /// only after cleanup has restored invariants. Polling may continue while
    /// hidden. Returns false after terminal native close/failure; repeated
    /// successful hide calls are idempotent.
    bool hide();

    /// Return requested local child visibility.
    ///
    /// This is not an occlusion query: it does not report host minimization,
    /// hidden ancestors, clipping, or whether pixels are currently on screen.
    [[nodiscard]] bool visible() const noexcept;

    /// Request terminal close of this child view.
    ///
    /// Unlike hide(), close is terminal for the native child lifetime.
    void request_close();

    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] Dispatcher dispatcher() const noexcept override;
    [[nodiscard]] DesktopServices& desktop_services();
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
    friend struct detail::PlatformTestAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<DesktopServicesBackend> desktop_services_backend_;
    std::unique_ptr<DesktopServices> desktop_services_;
};

} // namespace ui
