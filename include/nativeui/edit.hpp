#pragma once

#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace ui {

enum class EditSource { Pointer, Keyboard, Wheel, Accessibility };

template <detail::StateValue T>
struct EditCallbacks {
    std::function<void(EditSource)> begin;
    std::function<void(const T&, EditSource)> change;
    std::function<void(EditSource)> end;
    std::function<void(EditSource)> cancel;
};

/// UI-thread value editing, with no parameter, normalization or host semantics.
/// Direct State/Binding writes never emit these callbacks. Cancellation keeps
/// the last committed value. Each begun interaction has exactly one terminal
/// end/cancel notification, including owner destruction (no-throw cancellation).
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
    explicit EditSession(Binding<T> state, EditCallbacks<T> callbacks = {})
        : control_(std::make_shared<Control>(std::move(state), std::move(callbacks))) {}
    EditSession(const EditSession&) = delete;
    EditSession& operator=(const EditSession&) = delete;
    EditSession(EditSession&&) = delete;
    EditSession& operator=(EditSession&&) = delete;

    ~EditSession() noexcept {
        const auto control = control_;
        control->alive = false;
        cancel_noexcept(control);
    }

    [[nodiscard]] bool active() const noexcept { return control_->active; }
    bool begin(EditSource source) { return begin_control(control_, source); }
    bool update(T value) { return update_control(control_, std::move(value)); }
    void end() { const auto control = control_; terminate(control, Terminal::End); }
    void cancel() { const auto control = control_; terminate(control, Terminal::Cancel); }

    /// Commit a final pointer value and end without accessing a deleted owner
    /// if an observer/callback removes the component during the commit.
    bool finish(T value) {
        const auto control = control_;
        if (control->dispatching) return false;
        const bool changed = update_control(control, std::move(value));
        terminate(control, Terminal::End);
        return changed;
    }

    /// One atomic keyboard/wheel/semantic command. An unchanged value does not
    /// begin an interaction. An already-active session is never interrupted.
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
