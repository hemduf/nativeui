#pragma once

#include <nativeui/component.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui {

enum class Align {
    Start,
    Center,
    End,
    Stretch,
};

enum class Justify {
    Start,
    Center,
    End,
    SpaceBetween,
};


enum class ScrollAxis {
    Horizontal,
    Vertical,
    Both,
};

class ScrollState {
    struct Listener {
        std::size_t id{};
        bool active{true};
        std::function<void(Point)> callback;
    };

    struct Control {
        explicit Control(ScrollAxis value) : axis(value) {}

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
            pending_offset.reset();
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

        ScrollAxis axis{ScrollAxis::Vertical};
        Point offset{};
        Size viewport{};
        Size content{};
        bool metrics_valid{};
        std::vector<std::unique_ptr<Listener>> listeners;
        std::optional<Point> pending_offset;
        std::size_t next_listener_id{1};
        bool dispatching{};
        bool cleanup_needed{};
        bool owner_alive{true};
    };

public:
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
            if (auto control = control_.lock()) control->remove_listener(id_);
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

    explicit ScrollState(ScrollAxis axis = ScrollAxis::Vertical)
        : control_(std::make_shared<Control>(axis)) {}

    ScrollState(const ScrollState&) = delete;
    ScrollState& operator=(const ScrollState&) = delete;

    ~ScrollState() { control_->invalidate_owner(); }

    [[nodiscard]] ScrollAxis axis() const noexcept { return control_->axis; }
    [[nodiscard]] Point offset() const noexcept { return control_->offset; }
    [[nodiscard]] Size viewport_size() const noexcept { return control_->viewport; }
    [[nodiscard]] Size content_size() const noexcept { return control_->content; }
    [[nodiscard]] Point max_offset() const noexcept { return maximum_offset(*control_); }

    void set_offset(Point value) {
        auto control = control_;
        dispatch_offset(control, clamp(*control, value));
    }

    void scroll_by(Point delta) {
        auto control = control_;
        if (!control->owner_alive) return;
        const Point base = control->dispatching && control->pending_offset
                               ? *control->pending_offset
                               : control->offset;
        dispatch_offset(
            control,
            Point{base.x + finite_or_zero(delta.x), base.y + finite_or_zero(delta.y)});
    }

    Subscription observe(std::function<void(Point)> callback) {
        auto control = control_;
        if (!control->owner_alive) return {};
        const auto id = control->next_listener_id++;
        control->listeners.push_back(
            std::make_unique<Listener>(Listener{id, true, std::move(callback)}));
        return Subscription{control, id};
    }

private:
    friend class ScrollComponent;

    [[nodiscard]] static bool allows_horizontal(ScrollAxis axis) noexcept {
        return axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both;
    }
    [[nodiscard]] static bool allows_vertical(ScrollAxis axis) noexcept {
        return axis == ScrollAxis::Vertical || axis == ScrollAxis::Both;
    }
    [[nodiscard]] static float finite_or_zero(float value) noexcept {
        return std::isfinite(value) ? value : 0.0f;
    }
    [[nodiscard]] static bool same(Point a, Point b) noexcept {
        return a.x == b.x && a.y == b.y;
    }
    [[nodiscard]] static Point maximum_offset(const Control& control) noexcept {
        return Point{
            allows_horizontal(control.axis)
                ? std::max(0.0f, control.content.w - control.viewport.w)
                : 0.0f,
            allows_vertical(control.axis)
                ? std::max(0.0f, control.content.h - control.viewport.h)
                : 0.0f};
    }
    [[nodiscard]] static Point clamp(const Control& control, Point value) noexcept {
        value.x = allows_horizontal(control.axis)
                      ? std::max(0.0f, finite_or_zero(value.x))
                      : 0.0f;
        value.y = allows_vertical(control.axis)
                      ? std::max(0.0f, finite_or_zero(value.y))
                      : 0.0f;
        if (control.metrics_valid) {
            const auto maximum = maximum_offset(control);
            value.x = std::min(value.x, maximum.x);
            value.y = std::min(value.y, maximum.y);
        }
        return value;
    }

    static void dispatch_offset(const std::shared_ptr<Control>& control, Point value) {
        if (!control || !control->owner_alive) return;
        value = clamp(*control, value);

        if (control->dispatching) {
            if (same(value, control->offset)) {
                control->pending_offset.reset();
            } else {
                control->pending_offset = value;
            }
            return;
        }
        if (same(value, control->offset)) return;

        control->pending_offset = value;
        control->dispatching = true;
        try {
            while (control->owner_alive && control->pending_offset) {
                Point next = clamp(*control, *control->pending_offset);
                control->pending_offset.reset();
                if (same(next, control->offset)) continue;

                control->offset = next;
                const Point pass_value = control->offset;
                const std::size_t pass_size = control->listeners.size();
                for (std::size_t index = 0;
                     index < pass_size && control->owner_alive;
                     ++index) {
                    Listener* listener = control->listeners[index].get();
                    if (!listener || !listener->active || !listener->callback) continue;
                    listener->callback(pass_value);
                }
            }
        } catch (...) {
            // Match State<T>'s v1 failure transaction: the current pass value
            // remains committed, but recursive pending work and the unstarted
            // suffix are not retried implicitly after an observer throws.
            control->pending_offset.reset();
            control->dispatching = false;
            if (control->cleanup_needed) control->compact_inactive();
            throw;
        }

        control->dispatching = false;
        if (control->cleanup_needed) control->compact_inactive();
    }

    void update_metrics(Size viewport, Size content) {
        auto control = control_;
        if (!control->owner_alive) return;
        control->viewport = Size{std::max(0.0f, finite_or_zero(viewport.w)),
                                 std::max(0.0f, finite_or_zero(viewport.h))};
        control->content = Size{std::max(0.0f, finite_or_zero(content.w)),
                                std::max(0.0f, finite_or_zero(content.h))};
        control->metrics_valid = true;
        dispatch_offset(control, clamp(*control, control->offset));
    }

    std::shared_ptr<Control> control_;
};

enum class TrackType {
    Fixed,
    Auto,
    Flex,
};

class Track {
public:
    [[nodiscard]] static Track fixed(float extent) noexcept {
        return Track{TrackType::Fixed, sanitize(extent)};
    }

    [[nodiscard]] static Track auto_size() noexcept {
        return Track{TrackType::Auto, 0.0f};
    }

    [[nodiscard]] static Track flex(float weight = 1.0f) noexcept {
        return Track{TrackType::Flex, sanitize(weight)};
    }

    [[nodiscard]] TrackType type() const noexcept { return type_; }
    [[nodiscard]] float value() const noexcept { return value_; }

private:
    Track(TrackType type, float value) : type_(type), value_(value) {}

    [[nodiscard]] static float sanitize(float value) noexcept {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
    }

    TrackType type_{TrackType::Auto};
    float value_{};
};

struct GridTracks {
    std::vector<Track> columns;
    std::vector<Track> rows;
};

namespace detail {

[[nodiscard]] inline float main_axis_offset(Justify justify, float extra) noexcept {
    extra = std::isfinite(extra) ? std::max(0.0f, extra) : 0.0f;
    switch (justify) {
    case Justify::Center: return extra * 0.5f;
    case Justify::End: return extra;
    case Justify::Start:
    case Justify::SpaceBetween: return 0.0f;
    }
    return 0.0f;
}

[[nodiscard]] inline float distributed_gap(
    Justify justify, float base_gap, float extra, std::size_t count) noexcept {
    base_gap = std::isfinite(base_gap) ? std::max(0.0f, base_gap) : 0.0f;
    extra = std::isfinite(extra) ? std::max(0.0f, extra) : 0.0f;
    return (justify == Justify::SpaceBetween && count > 1)
               ? base_gap + extra / static_cast<float>(count - 1)
               : base_gap;
}

[[nodiscard]] inline float cross_axis_offset(
    Align align, float available, float extent) noexcept {
    available = std::isfinite(available) ? std::max(0.0f, available) : 0.0f;
    extent = std::isfinite(extent) ? std::clamp(extent, 0.0f, available) : 0.0f;
    const float extra = std::max(0.0f, available - extent);
    switch (align) {
    case Align::Center: return extra * 0.5f;
    case Align::End: return extra;
    case Align::Start:
    case Align::Stretch: return 0.0f;
    }
    return 0.0f;
}

struct MainAxisAllocation {
    std::vector<float> extents;
    float free_space{};
};

[[nodiscard]] inline float flex_weight(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

template <class ExtentFn>
[[nodiscard]] inline MainAxisAllocation allocate_main_axis(
    const std::vector<ChildMetrics>& children,
    float available,
    float gap,
    ExtentFn extent) {
    available = std::isfinite(available) ? std::max(0.0f, available) : 0.0f;
    gap = std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f;

    MainAxisAllocation result;
    result.extents.reserve(children.size());

    const float gap_total = children.size() > 1
                                ? gap * static_cast<float>(children.size() - 1)
                                : 0.0f;
    const float available_for_children = std::max(0.0f, available - gap_total);

    float preferred_total = 0.0f;
    for (const auto& child : children) {
        const float minimum = std::max(0.0f, extent(child.minimum));
        const float preferred = std::max(minimum, std::max(0.0f, extent(child.preferred)));
        result.extents.push_back(preferred);
        preferred_total += preferred;
    }

    if (preferred_total < available_for_children) {
        const float extra = available_for_children - preferred_total;
        float total_grow = 0.0f;
        for (const auto& child : children) total_grow += flex_weight(child.flex.grow);

        if (total_grow > 0.0f) {
            for (std::size_t i = 0; i < children.size(); ++i) {
                const float weight = flex_weight(children[i].flex.grow);
                if (weight > 0.0f) {
                    result.extents[i] += extra * (weight / total_grow);
                }
            }
            result.free_space = 0.0f;
        } else {
            result.free_space = extra;
        }
        return result;
    }

    float deficit = preferred_total - available_for_children;
    constexpr float epsilon = 0.0001f;

    while (deficit > epsilon) {
        float total_shrink = 0.0f;
        for (std::size_t i = 0; i < children.size(); ++i) {
            const float minimum = std::max(0.0f, extent(children[i].minimum));
            if (result.extents[i] > minimum + epsilon) {
                total_shrink += flex_weight(children[i].flex.shrink);
            }
        }
        if (total_shrink <= 0.0f) break;

        const float pass_deficit = deficit;
        float reduced = 0.0f;
        for (std::size_t i = 0; i < children.size(); ++i) {
            const float weight = flex_weight(children[i].flex.shrink);
            const float minimum = std::max(0.0f, extent(children[i].minimum));
            const float capacity = std::max(0.0f, result.extents[i] - minimum);
            if (weight <= 0.0f || capacity <= epsilon) continue;

            const float requested = pass_deficit * (weight / total_shrink);
            const float delta = std::min(capacity, requested);
            result.extents[i] -= delta;
            reduced += delta;
        }

        if (reduced <= epsilon) break;
        deficit = std::max(0.0f, deficit - reduced);
    }

    // If minimum sizes cannot fit, overflow remains explicit. T011 defines
    // clipping/overflow policy; silently shrinking below a minimum here would
    // violate the constraint contract.
    result.free_space = 0.0f;
    return result;
}

struct GridTrackMetrics {
    float minimum{};
    float preferred{};
};

[[nodiscard]] inline std::vector<float> allocate_grid_tracks(
    const std::vector<Track>& tracks,
    const std::vector<GridTrackMetrics>& content,
    float available,
    float gap) {
    const std::size_t count = tracks.size();
    std::vector<float> extents(count, 0.0f);
    if (count == 0) return extents;

    available = std::isfinite(available) ? std::max(0.0f, available) : 0.0f;
    gap = std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f;
    const float gap_total = gap * static_cast<float>(count - 1);
    const float available_for_tracks = std::max(0.0f, available - gap_total);

    float base_total = 0.0f;
    float total_flex = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const auto metrics = i < content.size() ? content[i] : GridTrackMetrics{};
        switch (tracks[i].type()) {
        case TrackType::Fixed:
            extents[i] = tracks[i].value();
            break;
        case TrackType::Auto:
            extents[i] = std::max(metrics.minimum, metrics.preferred);
            break;
        case TrackType::Flex:
            if (tracks[i].value() > 0.0f) {
                extents[i] = metrics.minimum;
                total_flex += tracks[i].value();
            } else {
                // A zero-weight flex track behaves like intrinsic auto sizing
                // instead of collapsing content to its minimum.
                extents[i] = std::max(metrics.minimum, metrics.preferred);
            }
            break;
        }
        base_total += extents[i];
    }

    if (base_total < available_for_tracks && total_flex > 0.0f) {
        const float extra = available_for_tracks - base_total;
        for (std::size_t i = 0; i < count; ++i) {
            if (tracks[i].type() == TrackType::Flex && tracks[i].value() > 0.0f) {
                extents[i] += extra * (tracks[i].value() / total_flex);
            }
        }
        return extents;
    }

    if (base_total > available_for_tracks) {
        float deficit = base_total - available_for_tracks;
        float total_capacity = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            if (tracks[i].type() != TrackType::Auto) continue;
            const auto metrics = i < content.size() ? content[i] : GridTrackMetrics{};
            total_capacity += std::max(0.0f, extents[i] - metrics.minimum);
        }

        if (total_capacity > 0.0f) {
            const float requested = std::min(deficit, total_capacity);
            for (std::size_t i = 0; i < count; ++i) {
                if (tracks[i].type() != TrackType::Auto) continue;
                const auto metrics = i < content.size() ? content[i] : GridTrackMetrics{};
                const float capacity = std::max(0.0f, extents[i] - metrics.minimum);
                if (capacity > 0.0f) {
                    extents[i] -= requested * (capacity / total_capacity);
                }
            }
        }
    }

    // Fixed tracks and content minimums are never violated. If they cannot fit,
    // the grid overflows explicitly until T011 applies clipping policy.
    return extents;
}

} // namespace detail
