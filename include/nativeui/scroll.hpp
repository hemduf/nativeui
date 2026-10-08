#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <cstddef>
#include <functional>
#include <optional>

namespace ui {

enum class ScrollAxis { Horizontal, Vertical, Both };

namespace detail { class RetainedScrollComponent; class ScrollMetricsAccess; }

class ScrollState {
    struct Listener {
        std::size_t id{};
        bool active{true};
        std::function<void(Point)> callback;
    };

    struct Control {
        explicit Control(ScrollAxis value);

        [[nodiscard]] bool has_listener(std::size_t id) const noexcept;

        void remove_listener(std::size_t id) noexcept;

        void invalidate_owner() noexcept;

        void compact_inactive() noexcept;

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
    class LifetimeToken {
    public:
        LifetimeToken() = default;

        [[nodiscard]] bool active() const noexcept;

    private:
        friend class ScrollState;
        explicit LifetimeToken(std::weak_ptr<Control> control);

        std::weak_ptr<Control> control_;
    };

    class Subscription {
    public:
        Subscription() = default;
        Subscription(std::weak_ptr<Control> control, std::size_t id);
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& other) noexcept;
        Subscription& operator=(Subscription&& other) noexcept;
        ~Subscription();

        void reset() noexcept;

        [[nodiscard]] bool active() const noexcept;

    private:
        std::weak_ptr<Control> control_;
        std::size_t id_{};
    };

    explicit ScrollState(ScrollAxis axis = ScrollAxis::Vertical);

    ScrollState(const ScrollState&) = delete;
    ScrollState& operator=(const ScrollState&) = delete;

    ~ScrollState();

    [[nodiscard]] ScrollAxis axis() const noexcept;
    [[nodiscard]] Point offset() const noexcept;
    [[nodiscard]] Size viewport_size() const noexcept;
    [[nodiscard]] Size content_size() const noexcept;
    [[nodiscard]] Point max_offset() const noexcept;
    [[nodiscard]] LifetimeToken lifetime_token() const noexcept;

    void set_offset(Point value);

    void scroll_by(Point delta);

    Subscription observe(std::function<void(Point)> callback);

private:
    friend class ScrollComponent;
    friend class detail::RetainedScrollComponent;
    friend class detail::ScrollMetricsAccess;

    [[nodiscard]] static bool allows_horizontal(ScrollAxis axis) noexcept;
    [[nodiscard]] static bool allows_vertical(ScrollAxis axis) noexcept;
    [[nodiscard]] static float finite_or_zero(float value) noexcept;
    [[nodiscard]] static bool same(Point a, Point b) noexcept;
    [[nodiscard]] static Point maximum_offset(const Control& control) noexcept;
    [[nodiscard]] static Point clamp(const Control& control, Point value) noexcept;

    static void dispatch_offset(const std::shared_ptr<Control>& control, Point value);

    void update_metrics(Size viewport, Size content);

    std::shared_ptr<Control> control_;
};

class ScrollComponent final : public Component {
public:
    explicit ScrollComponent(ScrollState& state);

    [[nodiscard]] bool clips_children() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void mount(MountContext& context) override;

    void unmount(LifecycleContext&) override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;

private:
    [[nodiscard]] bool alive() const noexcept;

    ScrollState* state_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
    ScrollState::LifetimeToken lifetime_;
    ScrollState::Subscription subscription_;
};

namespace detail {

class ScrollMetricsAccess final {
public:
    [[nodiscard]] static bool publish(ScrollState* state,ScrollState::LifetimeToken lifetime,
                                     Size viewport,Size content);
};

class RetainedScrollComponent final : public Component {
public:
    RetainedScrollComponent(
        ScrollState* state,
        ScrollAxis axis,
        ScrollState::LifetimeToken lifetime);

    [[nodiscard]] bool clips_children() const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void mount(MountContext& context) override;

    void unmount(LifecycleContext&) override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override;

    void paint(PaintContext&) const override;

private:
    [[nodiscard]] bool alive() const noexcept;

    ScrollState* state_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
    ScrollState::LifetimeToken lifetime_;
    ScrollState::Subscription subscription_;
};

} // namespace detail

class Scroll {
public:
    template <class Child>
    Scroll(ScrollState& state, Child&& child)
        : state_(&state), axis_(state.axis()), lifetime_(state.lifetime_token()) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() &&;

private:
    ScrollState* state_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
    ScrollState::LifetimeToken lifetime_;
    std::vector<Spec> children_;
};

} // namespace ui
