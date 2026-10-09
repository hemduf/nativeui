#pragma once

// Pugl-independent, instance-owned input state.
#include <nativeui/input.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace ui::detail {

// Bounded per-view pointer delta tracking, without heap allocation.
class RawPointerTracker final {
public:
    void update(InputEvent& event) noexcept {
        std::size_t index = count_;
        for (std::size_t i = 0; i < count_; ++i) {
            if (positions_[i].id == event.pointer.id) {
                index = i;
                break;
            }
        }
        const bool terminal = event.type == InputType::PointerUp ||
                              event.type == InputType::PointerCancel;
        if (index != count_) {
            if (event.type != InputType::PointerDown) {
                event.delta = {event.position.x - positions_[index].position.x,
                               event.position.y - positions_[index].position.y};
            }
            if (terminal) {
                --count_;
                if (index != count_) positions_[index] = positions_[count_];
                positions_[count_] = {};
            } else {
                positions_[index].position = event.position;
            }
            return;
        }
        if (!terminal && count_ < positions_.size()) {
            positions_[count_++] = {event.pointer.id, event.position};
        }
    }

    void clear() noexcept {
        for (std::size_t i = 0; i < count_; ++i) positions_[i] = {};
        count_ = 0U;
    }

private:
    struct Position { PointerId id{}; Point position{}; };
    std::array<Position, 16U> positions_{};
    std::size_t count_{};
};

// The click sequence is independent of the retained pointer-capture state.
class MultiClickTracker final {
public:
    [[nodiscard]] int record(double now, Point position) noexcept {
        const float dx = position.x - last_position_.x;
        const float dy = position.y - last_position_.y;
        const bool close_in_time = now - last_time_ <= 0.35;
        const bool close_in_space = std::sqrt(dx * dx + dy * dy) <= 5.0f;
        count_ = close_in_time && close_in_space ? std::min(3, count_ + 1) : 1;
        last_time_ = now;
        last_position_ = position;
        return count_;
    }
    [[nodiscard]] int count() const noexcept { return count_; }

private:
    double last_time_{-1000.0};
    Point last_position_{};
    int count_{};
};

} // namespace ui::detail
