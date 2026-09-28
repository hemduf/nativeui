#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui {

// -----------------------------------------------------------------------------
// Generic observable UI state (no plugin/audio semantics)
// -----------------------------------------------------------------------------
//
// State<T> and Binding<T> are intentionally retained-UI/main-thread
// abstractions. They do not synchronize value/listener access and must not be
// used as an audio-thread or cross-thread transport. Plug-in adapters must hand
// data into the UI domain with an explicitly reviewed thread-safe bridge
// (atomics/queues/snapshots as appropriate) and call set()/observe() on the UI
// thread.
//
// State values must support equality comparison. Notifications are synchronous:
// every pass exposes one stable borrowed value, observers added during a pass
// start on the next pass, observers removed before their turn are skipped, and
// recursive writes are coalesced to the latest value for the next pass.
//
// Observer exceptions terminate the current notification transaction. The pass
// value is already committed, callbacks that have not started remain registered
// but are not invoked for the failed pass, and any recursive pending write is
// discarded. State restores dispatch bookkeeping and listener structure before
// rethrowing the original exception. No observer callback is invoked as part of
// exception cleanup; a later explicit set() starts a fresh notification pass.

namespace detail {

template <class T>
concept StateValue = requires(const T& lhs, const T& rhs) {
    { lhs == rhs } -> std::convertible_to<bool>;
};

} // namespace detail

template <detail::StateValue T>
class Binding;

template <detail::StateValue T>
class State {
    struct Listener {
        std::size_t id{};
        bool active{true};
        std::function<void(const T&)> callback;
    };

    struct Control {
        explicit Control(T initial)
            : value(std::move(initial)) {}

        [[nodiscard]] bool has_listener(std::size_t id) const noexcept {
            for (const auto& listener : listeners) {
                if (listener && listener->id == id && listener->active) return true;
            }
            return false;
        }

        void remove_listener(std::size_t id) noexcept {
            for (auto& listener : listeners) {
                if (listener && listener->id == id) {
                    listener->active = false;
                    cleanup_needed = true;
                    break;
                }
            }
            if (!dispatching && cleanup_needed) compact_inactive();
        }

        void invalidate_owner() noexcept {
            owner_alive = false;
            pending_value.reset();
            for (auto& listener : listeners) {
                if (listener) listener->active = false;
            }
            cleanup_needed = true;
            if (!dispatching) compact_inactive();
        }

        void compact_inactive() noexcept {
            std::erase_if(listeners, [](const auto& listener) {
                return !listener || !listener->active;
            });
            cleanup_needed = false;
        }

        T value;
        std::vector<std::unique_ptr<Listener>> listeners;
        std::optional<T> pending_value;
        std::size_t next_listener_id{1};
        bool dispatching{false};
        bool cleanup_needed{false};
        bool owner_alive{true};
    };

public:
    using Callback = std::function<void(const T&)>;

    class Subscription {
    public:
        Subscription() = default;
        Subscription(std::weak_ptr<Control> control, std::size_t id)
            : control_(std::move(control)), id_(id) {}
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& other) noexcept
            : control_(std::move(other.control_)), id_(std::exchange(other.id_, 0)) {}
        Subscription& operator=(Subscription&& other) noexcept {
            if (this == &other) return *this;
            reset();
            control_ = std::move(other.control_);
            id_ = std::exchange(other.id_, 0);
            return *this;
        }
        ~Subscription() { reset(); }

        void reset() noexcept {
            if (id_ == 0) return;
            if (auto control = control_.lock()) {
                control->remove_listener(id_);
            }
            control_.reset();
            id_ = 0;
        }

        [[nodiscard]] bool active() const noexcept {
            if (id_ == 0) return false;
            if (auto control = control_.lock()) {
                return control->owner_alive && control->has_listener(id_);
            }
            return false;
        }

    private:
        std::weak_ptr<Control> control_;
        std::size_t id_{};
    };

    explicit State(T initial = {})
        : control_(std::make_shared<Control>(std::move(initial))) {}
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    ~State() {
        control_->invalidate_owner();
    }

    [[nodiscard]] const T& get() const noexcept { return control_->value; }

    void set(T value) {
        set_control(control_, std::move(value));
    }

    Subscription observe(Callback callback) {
        return observe_control(control_, std::move(callback));
    }

    [[nodiscard]] Binding<T> binding() noexcept;

private:
    // Shared mutation entry point for State<T> and Binding<T>. Taking the
    // control block by value is deliberate: a callback is allowed to destroy
    // the owning State while the synchronous notification stack is active.
    static void set_control(std::shared_ptr<Control> control, T value) {
        if (!control->owner_alive) return;

        if (control->dispatching) {
            // Recursive writes never mutate the value visible to the current
            // pass. The latest write wins for the next pass; writing the current
            // value cancels an earlier pending write.
            if (value == control->value) {
                control->pending_value.reset();
            } else {
                control->pending_value = std::move(value);
            }
            return;
        }

        if (value == control->value) return;

        control->pending_value = std::move(value);
        control->dispatching = true;

        try {
            while (control->owner_alive && control->pending_value.has_value()) {
                T next_value = std::move(*control->pending_value);
                control->pending_value.reset();
                if (next_value == control->value) continue;

                control->value = std::move(next_value);

                // Heap-stable listener slots avoid copying callback objects on
                // each set(). Capturing the pass size prevents observers added
                // during this pass from joining until the next pass.
                const std::size_t pass_size = control->listeners.size();
                for (std::size_t index = 0;
                     index < pass_size && control->owner_alive;
                     ++index) {
                    Listener* listener = control->listeners[index].get();
                    if (!listener || !listener->active || !listener->callback) continue;
                    listener->callback(control->value);
                }
            }
        } catch (...) {
            // Exception policy: the current value remains committed, but an
            // unstarted suffix of this pass receives no synthetic retry and any
            // recursive next-pass write is discarded. Restore only framework
            // bookkeeping/registry state while unwinding, then propagate the
            // original observer exception to the direct C++ caller.
            control->pending_value.reset();
            control->dispatching = false;
            if (control->cleanup_needed) control->compact_inactive();
            throw;
        }

        control->dispatching = false;
        if (control->cleanup_needed) control->compact_inactive();
    }

    static Subscription observe_control(std::shared_ptr<Control> control,
                                        Callback callback) {
        if (!control->owner_alive) return {};

        const auto id = control->next_listener_id++;
        control->listeners.push_back(
            std::make_unique<Listener>(Listener{id, true, std::move(callback)}));
        return Subscription{control, id};
    }

    std::shared_ptr<Control> control_;

    friend class Binding<T>;
};

// Binding<T> is a reference-like handle to one State<T> source. It retains the
// source control block, never the State object itself. Destroying State marks the
// source invalid and removes subscriptions, while the retained last value stays
// readable until the final Binding handle is released. A logically invalid
// Binding ignores writes and returns an inactive subscription from observe().
//
// Copy and move both preserve source identity. Move intentionally behaves like a
// reference-handle copy so the moved-from Binding remains a valid handle; this
// keeps every constructed Binding readable and avoids an empty-handle state with
// no retained value.
template <detail::StateValue T>
class Binding {
public:
    using Callback = typename State<T>::Callback;
    using Subscription = typename State<T>::Subscription;

    Binding(const Binding&) = default;
    Binding& operator=(const Binding&) = default;
    Binding(Binding&& other) noexcept
        : control_(other.control_) {}
    Binding& operator=(Binding&& other) noexcept {
        if (this != &other) control_ = other.control_;
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept {
        return control_->owner_alive;
    }

    [[nodiscard]] const T& get() const noexcept {
        return control_->value;
    }

    void set(T value) {
        State<T>::set_control(control_, std::move(value));
    }

    Subscription observe(Callback callback) {
        return State<T>::observe_control(control_, std::move(callback));
    }

private:
    explicit Binding(std::shared_ptr<typename State<T>::Control> control) noexcept
        : control_(std::move(control)) {}

    std::shared_ptr<typename State<T>::Control> control_;

    friend class State<T>;
};

template <detail::StateValue T>
Binding<T> State<T>::binding() noexcept {
    return Binding<T>{control_};
}

} // namespace ui
