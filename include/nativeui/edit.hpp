#pragma once

#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace ui {

/// Origin of one user-facing edit lifetime.
///
/// The tag is callback metadata only; it adds no host-automation, normalization,
/// accessibility transport, or cross-thread semantics.
enum class EditSource {
    /// Pointer press or drag interaction.
    Pointer,
    /// Keyboard command or key-repeat interaction.
    Keyboard,
    /// Mouse-wheel or equivalent scrolling interaction.
    Wheel,
    /// Semantic action initiated by an accessibility adapter.
    Accessibility
};

/// Synchronous notifications owned by an `EditSession<T>`.
///
/// Stored callbacks own their captures according to normal C++ rules. Objects
/// captured by reference or pointer are not kept alive by NativeUI. Callbacks
/// run on the UI thread and may allocate or throw.
template <detail::StateValue T>
struct EditCallbacks {
    /// Called once after an edit successfully becomes active.
    std::function<void(EditSource)> begin;
    /// Called after State observers for an effective committed value change.
    ///
    /// The value reference is borrowed from the State control block for this
    /// callback only; copy it if the value must survive the call.
    std::function<void(const T&, EditSource)> change;
    /// Called once when an active edit completes normally.
    std::function<void(EditSource)> end;
    /// Called once when an active edit is interrupted.
    ///
    /// Cancellation is a lifetime event and does not roll the State back.
    std::function<void(EditSource)> cancel;
};

/// Owns one generic UI-thread edit lifetime around a `Binding<T>`.
///
/// The Binding and callbacks are retained by value, while the originating
/// `State<T>` remains the logical value owner. Direct State/Binding writes never
/// emit edit callbacks. Cancellation keeps the latest committed value. Each begun
/// interaction receives exactly one terminal end/cancel notification, including
/// no-throw owner destruction.
///
/// Sessions sharing one State are independent. Calls may allocate and invoke
/// application callbacks; this is UI/main-thread infrastructure, not an
/// audio/DSP real-time or cross-thread transport.
///
/// begin/update/set are rejected during callbacks or while this State is
/// delivering another notification transaction. Reentrant end/cancel is
/// deferred until the current notification completes; cancel wins over end.
/// State observers run before change, which reports the final committed value.
/// A throwing begin/change/observer cancels once, then rethrows the original
/// error; a throwing terminal is never retried. No change follows its terminal
/// notification.
/// Subtree removal is supported; top-level UI/native owner destruction must be
/// deferred to its safe checkpoint, as with other NativeUI callbacks.
template <detail::StateValue T>
class EditSession final {
    enum class Terminal { None, End, Cancel };
    struct Control {
        Control(Binding<T> binding, EditCallbacks<T> notifications)
            : state(std::move(binding)), callbacks(std::move(notifications)) {}
        Binding<T> state;
        EditCallbacks<T> callbacks;
        EditSource source{EditSource::Pointer};
        Terminal terminal{Terminal::None};
        bool active{};
        bool dispatching{};
        bool alive{true};
    };
    using Owner = std::shared_ptr<Control>;

public:
    /// Construct an inactive session, owning the Binding and callbacks by value.
    ///
    /// Allocation failure propagates normally. If the originating State later
    /// dies, mutation is rejected and an active edit is cancelled when observed.
    explicit EditSession(Binding<T> state, EditCallbacks<T> callbacks = {})
        : control_(std::make_shared<Control>(std::move(state), std::move(callbacks))) {}
    EditSession(const EditSession&) = delete;
    EditSession& operator=(const EditSession&) = delete;
    EditSession(EditSession&&) = delete;
    EditSession& operator=(EditSession&&) = delete;

    /// Cancel an active edit without allowing terminal callback errors to escape.
    ~EditSession() noexcept {
        const auto control = control_;
        control->alive = false;
        cancel_noexcept(control);
    }

    /// Return whether this session currently owns an unterminated edit.
    [[nodiscard]] bool active() const noexcept { return control_->active; }

    /// Begin an edit tagged with `source`.
    ///
    /// Returns false for an already-active or invalid session, during callback
    /// dispatch, or while the backing State is already notifying observers.
    bool begin(EditSource source) { return begin_control(control_, source); }

    /// Commit `value` through the Binding and report effective value change.
    ///
    /// State observers run before `change`; equal values produce no change
    /// callback. Invalid, inactive, or reentrant updates return false.
    bool update(T value) { return update_control(control_, std::move(value)); }

    /// Request normal termination; reentrant requests are deferred.
    void end() { const auto control = control_; terminate(control, Terminal::End); }

    /// Request cancellation without restoring an earlier State value.
    ///
    /// A reentrant cancel is deferred and wins over a competing deferred end.
    void cancel() { const auto control = control_; terminate(control, Terminal::Cancel); }

    /// Commit a final value and then end while retaining internal lifetime state.
    ///
    /// Safe when an observer or callback removes the owning component during the
    /// commit. The return value reports whether the effective State value changed.
    bool finish(T value) {
        const auto control = control_;
        if (control->dispatching) return false;
        const bool changed = update_control(control, std::move(value));
        terminate(control, Terminal::End);
        return changed;
    }

    /// Perform one atomic begin/update/end command for a discrete interaction.
    ///
    /// Equal values emit nothing and an already-active session is not interrupted.
    bool set(T value, EditSource source) {
        const auto control = control_;
        if (!control->alive || !control->state.valid() || control->state.control_->dispatching || control->active ||
            control->dispatching || value == control->state.get()) return false;
        if (!begin_control(control, source)) return false;
        const bool changed = update_control(control, std::move(value));
        terminate(control, Terminal::End);
        return changed;
    }

private:
    static void flush_terminal(const Owner& control) {
        if (!control->active || control->dispatching ||
            control->terminal == Terminal::None) return;
        const auto terminal = std::exchange(control->terminal, Terminal::None);
        control->active = false;
        control->dispatching = true;
        try {
            auto& callback = terminal == Terminal::Cancel
                ? control->callbacks.cancel : control->callbacks.end;
            if (callback) callback(control->source);
        } catch (...) {
            control->dispatching = false;
            throw;
        }
        control->dispatching = false;
    }

    static void terminate(const Owner& control, Terminal terminal) {
        if (!control->active) return;
        if (!control->state.valid()) terminal = Terminal::Cancel;
        if (control->terminal != Terminal::Cancel) control->terminal = terminal;
        flush_terminal(control);
    }

    static void cancel_noexcept(const Owner& control) noexcept {
        try { terminate(control, Terminal::Cancel); } catch (...) {}
    }

    template <class Callback>
    static void notify(const Owner& control, Callback&& callback) {
        control->dispatching = true;
        try {
            std::forward<Callback>(callback)();
        } catch (...) {
            control->dispatching = false;
            cancel_noexcept(control);
            throw;
        }
        control->dispatching = false;
        flush_terminal(control);
    }

    static bool begin_control(Owner control, EditSource source) {
        if (!control->alive || !control->state.valid() || control->state.control_->dispatching || control->active ||
            control->dispatching) return false;
        control->source = source;
        control->active = true;
        notify(control, [&] {
            if (control->callbacks.begin) control->callbacks.begin(source);
        });
        if (!control->state.valid()) terminate(control, Terminal::Cancel);
        return control->alive && control->active;
    }

    static bool update_control(Owner control, T value) {
        if (!control->alive || !control->active || control->dispatching ||
            control->state.control_->dispatching) return false;
        if (!control->state.valid()) {
            terminate(control, Terminal::Cancel);
            return false;
        }
        bool changed = false;
        notify(control, [&] {
            const T previous = control->state.get();
            if (previous == value) return;
            control->state.set(std::move(value));
            if (!control->state.valid()) control->terminal = Terminal::Cancel;
            const T committed = control->state.get();
            changed = !(previous == committed);
            if (changed && control->alive && control->state.valid() && control->callbacks.change) {
                control->callbacks.change(committed, control->source);
            }
        });
        return changed;
    }

    Owner control_;
};

} // namespace ui
