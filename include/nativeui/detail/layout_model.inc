#pragma once

#include <nativeui/component.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <functional>
#include <memory>
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
        std::function<void(Point)> callback;
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
            if (auto registry = registry_.lock()) registry->remove_listener(id_);
            registry_.reset();
            id_ = 0;
        }

    private:
        std::weak_ptr<Registry> registry_;
        std::size_t id_{};
    };

    explicit ScrollState(ScrollAxis axis = ScrollAxis::Vertical)
        : axis_(axis), registry_(std::make_shared<Registry>()) {}

    ScrollState(const ScrollState&) = delete;
    ScrollState& operator=(const ScrollState&) = delete;

    [[nodiscard]] ScrollAxis axis() const noexcept { return axis_; }
    [[nodiscard]] Point offset() const noexcept { return offset_; }
    [[nodiscard]] Size viewport_size() const noexcept { return viewport_; }
    [[nodiscard]] Size content_size() const noexcept { return content_; }
    [[nodiscard]] Point max_offset() const noexcept {
        return Point{allows_horizontal() ? std::max(0.0f, content_.w - viewport_.w) : 0.0f,
                     allows_vertical() ? std::max(0.0f, content_.h - viewport_.h) : 0.0f};
    }

    void set_offset(Point value) {
        value = clamp(value);
        if (same(value, offset_)) return;
        offset_ = value;
        notify_offset();
    }

    void scroll_by(Point delta) {
        set_offset(Point{offset_.x + finite_or_zero(delta.x),
                         offset_.y + finite_or_zero(delta.y)});
    }

    Subscription observe(std::function<void(Point)> callback) {
        const auto id = registry_->next_listener_id++;
        registry_->listeners.push_back(Listener{id, std::move(callback)});
        return Subscription{registry_, id};
    }

private:
    friend class ScrollComponent;

    [[nodiscard]] bool allows_horizontal() const noexcept {
        return axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both;
    }
    [[nodiscard]] bool allows_vertical() const noexcept {
        return axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both;
    }
    [[nodiscard]] static float finite_or_zero(float value) noexcept {
        return std::isfinite(value) ? value : 0.0f;
    }
    [[nodiscard]] static bool same(Point a, Point b) noexcept {
        return a.x == b.x && a.y == b.y;
    }
    [[nodiscard]] Point clamp(Point value) const noexcept {
        value.x = allows_horizontal() ? std::max(0.0f, finite_or_zero(value.x)) : 0.0f;
        value.y = allows_vertical() ? std::max(0.0f, finite_or_zero(value.y)) : 0.0f;
        if (metrics_valid_) {
            const auto maximum = max_offset();
            value.x = std::min(value.x, maximum.x);
            value.y = std::min(value.y, maximum.y);
        }
        return value;
    }

    void update_metrics(Size viewport, Size content) {
        viewport_ = Size{std::max(0.0f, finite_or_zero(viewport.w)),
                         std::max(0.0f, finite_or_zero(viewport.h))};
        content_ = Size{std::max(0.0f, finite_or_zero(content.w)),
                        std::max(0.0f, finite_or_zero(content.h))};
        metrics_valid_ = true;
        const auto clamped = clamp(offset_);
        if (!same(clamped, offset_)) {
            offset_ = clamped;
            notify_offset();
        }
    }

    void notify_offset() {
        const auto listeners = registry_->listeners;
        for (const auto& listener : listeners) {
            if (listener.callback) listener.callback(offset_);
        }
    }

    ScrollAxis axis_{ScrollAxis::Vertical};
    Point offset_{};
    Size viewport_{};
    Size content_{};
    bool metrics_valid_{};
    std::shared_ptr<Registry> registry_;
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
