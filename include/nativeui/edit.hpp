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
    /// Called once after begin() has marked the session active.
    ///
    /// The source is copied into the session and remains stable until end/cancel.
    /// A reentrant end/cancel request is deferred until this callback returns.
    /// Throwing cancels the edit once before the original exception propagates.
    std::function<void(EditSource)> begin;
    /// Called after State observers for an effective committed value change.
    ///
    /// The value reference is borrowed from the State/Binding control block for
    /// this callback only; copy it if it must survive the call. Observers run
    /// first, so the callback sees the final committed value after synchronous
    /// observer-side normalization. No callback is emitted when that final value
    /// compares equal to the value present before the update.
    ///
    /// Reentrant end/cancel is deferred. Throwing cancels once, then rethrows.
    std::function<void(const T&, EditSource)> change;
    /// Called exactly once for normal termination of a begun edit.
    ///
    /// The session is already inactive when this callback runs. Exceptions from
    /// explicit termination propagate and the terminal callback is never retried.
    std::function<void(EditSource)> end;
    /// Called exactly once when a begun edit is interrupted.
    ///
    /// Cancellation is a lifetime event and does not roll the State back. The
    /// session is inactive before this callback runs. Explicit cancellation
    /// propagates callback exceptions; destructor-triggered cancellation swallows
    /// them to preserve the noexcept destructor contract.
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
    /// Construction invokes no callbacks. Allocation failure for the shared
    /// control block propagates normally. If the originating State later dies,
    /// mutation is rejected and an active edit is cancelled when a later session
    /// operation observes the invalid Binding.
    explicit EditSession(Binding<T> state, EditCallbacks<T> callbacks = {})
        : control_(std::make_shared<Control>(std::move(state), std::move(callbacks))) {}
    /// Sessions have stable public identity and are neither copyable nor movable.
    ///
    /// Callback-safe subtree removal is handled by the internal shared control
    /// block rather than by moving the EditSession object itself.
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
    ///
    /// This is a local allocation-free flag read: it invokes no callbacks and
    /// does not validate the Binding. It can therefore remain true briefly after
    /// the originating State expires, until another operation observes that
    /// condition and converts the edit to cancellation.
    [[nodiscard]] bool active() const noexcept { return control_->active; }

    /// Begin an edit tagged with `source`.
    ///
    /// On success the source is retained for the complete lifetime and the begin
    /// callback runs synchronously. Returns false for an already-active or invalid
    /// session, during callback dispatch, or while the backing State is already
    /// notifying observers.
    ///
    /// A begin callback may request end/cancel reentrantly. That request is
    /// delivered after the callback; begin() then returns false because the edit
    /// is no longer active. A throwing begin callback cancels once and rethrows.
    bool begin(EditSource source) { return begin_control(control_, source); }

    /// Commit `value` through the Binding and report effective value change.
    ///
    /// The argument is consumed by value; no borrow survives the call. State
    /// observers run synchronously before change. The return is true only when
    /// the final committed State value differs from the value observed before
    /// this update, so observer normalization back to the previous value returns
    /// false and suppresses change.
    ///
    /// Invalid, inactive or reentrant updates return false. Deferred end/cancel
    /// requests are flushed before return; observer/change exceptions cancel once
    /// and then propagate.
    bool update(T value) { return update_control(control_, std::move(value)); }

    /// Request normal termination of the active edit.
    ///
    /// Inactive sessions are a no-op. Reentrant calls are deferred until the
    /// current begin/change callback returns. If the Binding has expired, end is
    /// converted to cancellation. The session becomes inactive before the
    /// terminal callback; an end-callback exception propagates and is not retried.
    void end() { const auto control = control_; terminate(control, Terminal::End); }

    /// Request cancellation without restoring an earlier State value.
    ///
    /// Inactive sessions are a no-op. A reentrant cancel is deferred and wins
    /// over a competing deferred end. The session becomes inactive before the
    /// callback. Explicit cancel-callback exceptions propagate and are never
    /// retried; destruction suppresses them.
    void cancel() { const auto control = control_; terminate(control, Terminal::Cancel); }

    /// Commit a final value and then request normal termination.
    ///
    /// Internal shared control keeps the transaction alive when an observer or
    /// callback removes the owning component during the commit. Outside callback
    /// dispatch, an equal final value still ends the edit and returns false. A
    /// deferred cancel wins over the planned end.
    ///
    /// Reentrant finish() returns false immediately and leaves the current edit
    /// untouched. Otherwise its return has the same effective-change meaning as
    /// update(); callback/observer exceptions propagate.
    bool finish(T value) {
        const auto control = control_;
        if (control->dispatching) return false;
        const bool changed = update_control(control, std::move(value));
        terminate(control, Terminal::End);
        return changed;
    }

    /// Perform one synchronous begin/update/end command for a discrete edit.
    ///
    /// The command starts only for a live, valid, inactive, non-reentrant session
    /// and when value differs from the current State. Equal/rejected calls emit
    /// no edit callbacks and do not interrupt an existing edit.
    ///
    /// Normal callback ordering is begin -> State observers -> change -> end.
    /// Cancellation requested from begin/change wins over the planned end. The
    /// return is true only for an effective committed value change; exceptions
    /// propagate after cancellation.
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
