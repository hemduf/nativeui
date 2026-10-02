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

/// Initial standalone-window configuration.
///
/// All sizes are expressed in logical pixels. The descriptor is consumed during
/// native-view construction; the window does not retain references to this
/// object or to its title string.
struct WindowDesc {
    /// Initial UTF-8 native window title.
    std::string title{"NativeUI"};

    /// Requested initial client size in logical pixels.
    ///
    /// Native configure events remain authoritative for the size eventually
    /// reported by `StandaloneWindow::size()`.
    Size size{720.0f, 520.0f};

    /// Whether the platform window should expose user resize affordances.
    bool resizable{true};

    /// Optional minimum client size in logical pixels.
    ///
    /// `std::nullopt` removes the minimum. Invalid/non-finite constraints or a
    /// minimum larger than the maximum make native-view construction fail.
    std::optional<Size> min_size{};

    /// Optional maximum client size in logical pixels.
    ///
    /// `std::nullopt` removes the maximum. Programmatic size requests are
    /// clamped to the active minimum/maximum constraints.
    std::optional<Size> max_size{};
};

/// Policy controlling when a standalone `Application` enters quit state.
enum class QuitPolicy {
    /// Request application quit after the last successfully registered window closes.
    OnLastWindowClosed,
    /// Keep the event loop runnable with zero windows until `request_quit()`.
    ExplicitOnly,
};

/// Result returned from a native/user close-request callback.
enum class CloseDecision {
    /// Accept the request and schedule terminal close at a safe checkpoint.
    Accept,
    /// Veto this native/user request; the window remains open.
    Cancel,
};

/// Explicit owner of the standalone application world/event loop.
///
/// Construction, polling, running and destruction are confined to the
/// platform/UI thread. The object owns exactly one standalone Pugl PROGRAM
/// world; v1 standalone windows attach explicitly to this instance.
class Application final {
public:
    /// Create one standalone program-world/event-loop owner.
    ///
    /// Construction is UI/main-thread work and may allocate. Platform
    /// initialization failure is reported by `valid() == false` and
    /// `last_error()`; callers should test validity before creating windows.
    Application();

    /// Destroy the standalone application world.
    ///
    /// Every attached `StandaloneWindow` must already have been destroyed.
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /// Return whether the application world is initialized and runnable.
    [[nodiscard]] bool valid() const noexcept;

    /// Borrow the most recent application/platform error message.
    ///
    /// The returned view is owned by this `Application` and is invalidated by
    /// destruction or by a later operation that replaces the stored error.
    [[nodiscard]] std::string_view last_error() const noexcept;

    /// Run the standalone event loop until quit or a terminal platform failure.
    ///
    /// This is UI/main-thread work and may dispatch application/component
    /// callbacks reentrantly. It is not audio/DSP real-time safe. On desktop,
    /// zero means a normal quit and non-zero reports invalid/terminal failure.
    /// On Emscripten exactly one `Application::run()` may own the module's
    /// browser main-loop slot; additional application instances remain usable
    /// through non-blocking `poll()`.
    int run();

    /// Pump the standalone world and all registered window dispatchers once.
    ///
    /// `timeout_seconds` is expressed in seconds: zero requests a non-blocking
    /// pump and a negative value permits waiting indefinitely, subject to queued
    /// dispatcher/lifecycle work. Returns false once quit/terminal failure stops
    /// further pumping. Callbacks reached by the pump may re-enter user code.
    bool poll(double timeout_seconds = 0.0);

    /// Idempotently request event-loop termination after the current safe callback boundary.
    ///
    /// This does not close or destroy any window.
    void request_quit();

    /// Return whether application quit has been requested or the owner is unusable.
    [[nodiscard]] bool quit_requested() const noexcept;

    /// Replace the policy used when registered windows close.
    ///
    /// The policy change itself does not close windows or synchronously dispatch callbacks.
    void set_quit_policy(QuitPolicy policy) noexcept;

    /// Return the currently configured quit policy.
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
    /// Create one top-level native window attached to `application`.
    ///
    /// `application` and `ui` are borrowed and must both outlive this window.
    /// `desc` is consumed by value; sizes use logical pixels. Normal
    /// platform/descriptor construction failures leave an invalid window and are
    /// exposed through `last_error()`; allocation failure may still propagate.
    /// Construction is UI/main-thread only and is not audio-RT safe.
    StandaloneWindow(Application& application, UI& ui, WindowDesc desc = {});

    /// Pre-v1 single-window compatibility path with per-window loop ownership.
    ///
    /// `ui` is borrowed for this object's lifetime. Prefer the explicit
    /// `Application` overload; this compatibility path is not the v1 ownership
    /// model and may propagate construction failures directly.
    [[deprecated("Use StandaloneWindow(Application&, UI&, WindowDesc) for the v1 standalone path")]]
    StandaloneWindow(UI& ui, WindowDesc desc = {});

    /// Tear down the native view and unregister it from its `Application`.
    ///
    /// Direct C++ destruction is not reported through `on_closed()`.
    ~StandaloneWindow() override;

    StandaloneWindow(const StandaloneWindow&) = delete;
    StandaloneWindow& operator=(const StandaloneWindow&) = delete;
    StandaloneWindow(StandaloneWindow&&) = delete;
    StandaloneWindow& operator=(StandaloneWindow&&) = delete;

    /// Run this window's event loop compatibility surface.
    ///
    /// With an explicit `Application`, this delegates to `Application::run()`.
    /// Prefer using the application directly in v1 code.
    int run();

    /// Pump native/dispatcher work for this window compatibility surface.
    ///
    /// With an explicit `Application`, this delegates to the shared application
    /// pump. `timeout_seconds` is seconds; zero is non-blocking and negative
    /// permits an indefinite wait when no queued control/dispatcher work forces
    /// an immediate checkpoint. Returns false after terminal close/failure.
    bool poll(double timeout_seconds = -1.0);

    /// Request a programmatic accepted close.
    ///
    /// This bypasses `on_close_request()` veto logic and schedules terminal
    /// close for a safe platform checkpoint after the current callback unwinds.
    /// Repeated requests are idempotent.
    void request_close();

    /// Return whether construction succeeded and the native view is still open.
    [[nodiscard]] bool valid() const noexcept;

    /// Return whether close is pending/completed or the native view requests termination.
    [[nodiscard]] bool should_close() const noexcept;

    /// Return whether NativeUI has completed its terminal close sequence.
    [[nodiscard]] bool is_closed() const noexcept;

    /// Return the last native-configured client size in logical pixels.
    [[nodiscard]] Size size() const noexcept;

    /// Return the last valid logical-to-physical device scale factor.
    ///
    /// A neutral fallback of `1.0f` is returned when no live core exists.
    [[nodiscard]] float scale_factor() const noexcept;

    /// Borrow the opaque native view handle; zero means no usable native view.
    ///
    /// The handle is owned by this window and becomes invalid at native teardown.
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;

    /// Borrow the latest construction/native error string owned by this window.
    ///
    /// The view remains valid only until destruction or a later operation that
    /// replaces the stored error.
    [[nodiscard]] std::string_view last_error() const noexcept;

    /// Return a copyable dispatcher facade associated with this window.
    ///
    /// Copying the facade does not extend the window/native-view lifetime; after
    /// shutdown, new work is rejected according to the Dispatcher contract.
    [[nodiscard]] Dispatcher dispatcher() const noexcept override;

    /// Lazily create and borrow this window's desktop-service facade.
    ///
    /// The returned reference is owned by the window and must not outlive it.
    /// Service calls may allocate, dispatch callbacks, or enter platform APIs and
    /// are therefore UI-thread/non-real-time operations.
    [[nodiscard]] DesktopServices& desktop_services();

    /// Replace the native UTF-8 title; returns false for a closed/failed view or platform error.
    bool set_title(std::string_view title);

    /// Show and raise the standalone window; idempotent when already visible.
    ///
    /// A successful transition invalidates retained presentation for a fresh frame.
    bool show();

    /// Hide the native window while keeping the retained/native view realized.
    ///
    /// Successful hiding releases retained pointer/focus/text-input activity.
    /// Cleanup callbacks may propagate exceptions after invariants are restored.
    bool hide();

    /// Request a client size in logical pixels.
    ///
    /// The request is clamped to active min/max constraints. Returns false for
    /// invalid/non-finite geometry, a closed view, or native submission failure.
    /// A later native configure event determines the authoritative `size()`.
    bool set_size(Size logical_size);

    /// Replace/remove the minimum client size in logical pixels.
    ///
    /// `std::nullopt` removes the bound. Returns false if the resulting
    /// min/max pair is invalid or cannot be applied to the native view.
    bool set_min_size(std::optional<Size> logical_size);

    /// Replace/remove the maximum client size in logical pixels.
    ///
    /// `std::nullopt` removes the bound. Returns false if the resulting
    /// min/max pair is invalid or cannot be applied to the native view.
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
    /// Construct a visible host-owned child view with default desktop services.
    ///
    /// `ui` and `parent` are borrowed and must outlive the view. `size` is
    /// the requested child extent in logical pixels. Construction is host
    /// UI/main-thread work and may propagate descriptor/platform/allocation errors.
    EmbeddedView(UI& ui, NativeParentHandle parent, Size size);

    /// Construct a visible child view with an optional shared desktop-services backend.
    ///
    /// The backend is shared-owned by this wrapper; passing null selects the
    /// DesktopServices fallback behavior. `ui` and `parent` remain borrowed.
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
    /// Tear down the native child without taking ownership of the host parent.
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
    /// Unlike `hide()`, close is terminal for the native child lifetime and
    /// also shuts down the view dispatcher. Repeated calls are safe.
    void request_close();

    /// Return whether the embedded native view has requested/entered terminal close.
    [[nodiscard]] bool should_close() const noexcept;

    /// Return the last native-configured child extent in logical pixels.
    [[nodiscard]] Size size() const noexcept;

    /// Return the last valid logical-to-physical device scale factor.
    ///
    /// A neutral fallback of `1.0f` is returned without a live implementation.
    [[nodiscard]] float scale_factor() const noexcept;

    /// Borrow the opaque native child handle; zero means no usable native view.
    ///
    /// NativeUI owns the child handle, not the embedding host, and invalidates it
    /// during child teardown.
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;

    /// Borrow the latest native/platform error string owned by this view.
    [[nodiscard]] std::string_view last_error() const noexcept;

    /// Return a copyable dispatcher facade for this embedded view.
    ///
    /// Copying it does not extend the embedded view lifetime.
    [[nodiscard]] Dispatcher dispatcher() const noexcept override;

    /// Lazily create and borrow this view's desktop-service facade.
    ///
    /// The returned reference is owned by the `EmbeddedView` and uses the
    /// shared backend supplied at construction, if any.
    [[nodiscard]] DesktopServices& desktop_services();

    /// Request a child extent in logical pixels.
    ///
    /// Returns false for invalid/non-finite geometry, terminal view state, or
    /// native submission failure. The host/native configure event remains
    /// authoritative for the size later reported by `size()`.
    bool set_size(Size logical_size);

    /// Replace the advisory preferred-size callback.
    ///
    /// NativeUI owns the callback object and passes an owned `Size` value in
    /// logical pixels. Installing a callback resets preferred-size state and may
    /// synchronously publish the current preferred size. Later notifications run
    /// on the host UI/main thread at a safe checkpoint. NativeUI never resizes
    /// the embedding parent; the host may ignore or clamp the request. Passing an
    /// empty callback disables notifications.
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
