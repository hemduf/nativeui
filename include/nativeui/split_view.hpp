#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ui {

enum class SplitOrientation { Horizontal, Vertical };

struct SplitViewStyle {
    double line_thickness{1.0};
    double hit_grip{6.0};
    std::optional<Color> color;
    std::optional<Color> hover_color;
    std::optional<Color> drag_color;
    std::optional<Color> focus_color;
    std::string accessible_name{"Séparateur"};
};

namespace detail { struct SplitViewState; struct SplitGeometry; }

class SplitViewComponent final : public Component {
public:
    SplitViewComponent(Binding<double> first_extent, std::shared_ptr<const Spec> first,
                       std::shared_ptr<const Spec> second, SplitOrientation orientation,
                       double minimum_first, double minimum_second, double step,
                       std::function<void(double)> on_change,
                       std::function<void(double)> on_commit, SplitViewStyle style);
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;
    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t count) const override;
    void layout_children(Rect bounds, const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override;
    [[nodiscard]] std::optional<std::size_t> foreground_child_index() const noexcept override;
    void paint(PaintContext&) const override;
    void mount(MountContext& context) override;
    void unmount(LifecycleContext&) override;

private:
    void layout_committed(Rect before, Rect after) noexcept override;
    friend class SplitView;
    [[nodiscard]] std::vector<Spec> compile_children() const;
    std::shared_ptr<detail::SplitViewState> state_;
    std::shared_ptr<const Spec> first_;
    std::shared_ptr<const Spec> second_;
    mutable std::shared_ptr<const detail::SplitGeometry> candidate_geometry_;
};

class SplitView {
public:
    template <class First, class Second>
    SplitView(Binding<double> first_extent, First&& first, Second&& second)
        : first_extent_(std::move(first_extent)), first_(make_spec(std::forward<First>(first))),
          second_(make_spec(std::forward<Second>(second))) {}
    template <class First, class Second>
    SplitView(State<double>& first_extent, First&& first, Second&& second)
        : SplitView(first_extent.binding(), std::forward<First>(first),
                    std::forward<Second>(second)) {}
    SplitView&& orientation(SplitOrientation value) &&;
    SplitView&& minimum_panes(double first, double second) &&;
    SplitView&& step(double value) &&;
    SplitView&& on_change(std::function<void(double)> callback) &&;
    SplitView&& on_commit(std::function<void(double)> callback) &&;
    SplitView&& style(SplitViewStyle value) &&;
    Spec spec() &&;

private:
    Binding<double> first_extent_;
    Spec first_;
    Spec second_;
    SplitOrientation orientation_{SplitOrientation::Horizontal};
    double minimum_first_{40.0};
    double minimum_second_{40.0};
    double step_{10.0};
    std::function<void(double)> on_change_;
    std::function<void(double)> on_commit_;
    SplitViewStyle style_;
};

} // namespace ui
