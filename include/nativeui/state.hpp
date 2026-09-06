#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

// -----------------------------------------------------------------------------
// Generic observable UI state (no plugin/audio semantics)
// -----------------------------------------------------------------------------

template <class T>
class State {
    struct Listener {
        std::size_t id{};
        std::function<void(const T&)> callback;
    };

    struct Registry {
        void remove_listener(std::size_t id) {
            std::erase_if(listeners, [id](const Listener& listener) { return listener.id == id; });
        }

        std::vector<Listener> listeners;
        std::size_t next_listener_id{1};
    };

public:
    class Subscription {
    public:
        Subscription() = default;
        Subscription(std::weak_ptr<Registry> registry, std::size_t id)
            : registry_(std::move(registry)), id_(id) {}
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& other) noexcept
            : registry_(std::move(other.registry_)), id_(std::exchange(other.id_, 0)) {}
        Subscription& operator=(Subscription&& other) noexcept {
            if (this == &other) return *this;
            reset();
            registry_ = std::move(other.registry_);
            id_ = std::exchange(other.id_, 0);
            return *this;
        }
        ~Subscription() { reset(); }

        void reset() noexcept {
            if (id_ == 0) return;
            if (auto registry = registry_.lock()) {
                registry->remove_listener(id_);
            }
            registry_.reset();
            id_ = 0;
        }

        [[nodiscard]] bool active() const noexcept {
            return id_ != 0 && !registry_.expired();
        }

    private:
        std::weak_ptr<Registry> registry_;
        std::size_t id_{};
    };

    explicit State(T initial = {})
        : value_(std::move(initial)), registry_(std::make_shared<Registry>()) {}
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    [[nodiscard]] const T& get() const noexcept { return value_; }

    void set(T value) {
        if (value == value_) return;
        value_ = std::move(value);
        const auto listeners = registry_->listeners;
        for (const auto& listener : listeners) {
            if (listener.callback) listener.callback(value_);
        }
    }

    Subscription observe(std::function<void(const T&)> callback) {
        const auto id = registry_->next_listener_id++;
        registry_->listeners.push_back(Listener{id, std::move(callback)});
        return Subscription{registry_, id};
    }

private:
    T value_{};
    std::shared_ptr<Registry> registry_;
};


} // namespace ui
