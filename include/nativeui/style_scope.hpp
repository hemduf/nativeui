#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/state.hpp>
#include <nativeui/theme.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

[[nodiscard]] bool style_scope_color_equal(Color a, Color b) noexcept;

[[nodiscard]] bool style_scope_optional_color_equal(
    const std::optional<Color>& a, const std::optional<Color>& b) noexcept;

} // namespace detail

/// Typed inheritable palette overrides for one lexical style scope.
struct StyleScopePaletteOverrides {
    std::optional<Color> background;
    std::optional<Color> surface;
    std::optional<Color> text;
    std::optional<Color> muted_text;
    std::optional<Color> border;
    std::optional<Color> accent;
    std::optional<Color> disabled;
    std::optional<Color> selection;
    std::optional<Color> focus;
    std::optional<Color> control_background;
    std::optional<Color> control_hover;
    std::optional<Color> active_highlight;
    std::optional<Color> track;

    [[nodiscard]] bool operator==(const StyleScopePaletteOverrides& other) const noexcept;
};

/// Typed inheritable typography overrides. These are style defaults only;
/// component/layout constraints are deliberately absent from the scope API.
struct StyleScopeTypographyOverrides {
    std::optional<std::string> family;
    std::optional<std::vector<std::string>> fallback_families;
    std::optional<float> base_size;
    std::optional<float> control_size;
    std::optional<float> label_size;
    std::optional<FontWeight> base_weight;
    std::optional<FontWeight> control_weight;
    std::optional<FontWeight> label_weight;
    std::optional<FontSlant> slant;

    [[nodiscard]] bool operator==(const StyleScopeTypographyOverrides&) const = default;
};

struct StyleScopeSpacingOverrides {
    std::optional<float> xs;
    std::optional<float> sm;
    std::optional<float> medium;
    std::optional<float> large;
    std::optional<float> xl;

    [[nodiscard]] bool operator==(const StyleScopeSpacingOverrides&) const = default;
};

struct StyleScopeRadiiOverrides {
    std::optional<float> sm;
    std::optional<float> medium;
    std::optional<float> large;

    [[nodiscard]] bool operator==(const StyleScopeRadiiOverrides&) const = default;
};

/// Inheritable standard-control defaults. These are the same typed defaults
/// already supplied by Theme; per-instance width/height/Flex/Grid/scroll state
/// and availability/callback/model state are intentionally not representable.
struct StyleScopeControlOverrides {
    std::optional<float> minimum_width;
    std::optional<float> control_height;
    std::optional<float> minimum_hit_target;
    std::optional<float> thumb_diameter;
    std::optional<float> track_thickness;
    std::optional<float> border_width;
    std::optional<float> focus_ring_width;

    [[nodiscard]] bool operator==(const StyleScopeControlOverrides&) const = default;
};

/// One lexical scope patch. Empty fields inherit from the nearest outer scope
/// or, ultimately, the owning UI Theme. Applying multiple values outer-to-inner
/// therefore implements nearest-scope-wins independently for each field.
struct StyleScopeOverrides {
    StyleScopePaletteOverrides palette;
    StyleScopeTypographyOverrides typography;
    StyleScopeSpacingOverrides spacing;
    StyleScopeRadiiOverrides radii;
    StyleScopeControlOverrides controls;

    [[nodiscard]] bool operator==(const StyleScopeOverrides&) const = default;
};

namespace detail {

template <class T>
inline void apply_scope_value(T& target, const std::optional<T>& value) {
    if (value) target = *value;
}

} // namespace detail

/// Apply one typed scope patch to an already-resolved inherited Theme value.
/// Call this outermost-to-innermost to obtain the lexical scope result. T038
/// component-local recipes still apply afterwards in each widget resolver.
[[nodiscard]] Theme apply_style_scope_overrides(
    Theme inherited, const StyleScopeOverrides& overrides);

/// Classify the effective invalidation caused by replacing one scope patch
/// under a specific inherited Theme. Equal effective values are a strict no-op.
[[nodiscard]] ThemeInvalidation classify_style_scope_change(
    const Theme& inherited,
    const StyleScopeOverrides& before,
    const StyleScopeOverrides& after);

namespace detail {

class StyleScopeComponent final : public Component, public ThemeBinding {
public:
    explicit StyleScopeComponent(StyleScopeOverrides overrides);

    explicit StyleScopeComponent(Binding<StyleScopeOverrides> state);

    void bind_theme(const Theme& theme) noexcept override;
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }

    [[nodiscard]] const Theme& descendant_theme() const noexcept override;

    void set_theme_change_invalidator(
        std::function<void(ThemeInvalidation)> callback) override;

    [[nodiscard]] bool availability_change_affects_paint(
        const ComponentAvailability&,
        const ComponentAvailability&) const noexcept override;

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override;

    void mount(MountContext&) override;

    void unmount(LifecycleContext&) override;

    void paint(PaintContext&) const override;

private:
    struct ObserverState {
        std::function<void(const StyleScopeOverrides&)> apply;
    };

    void replace_overrides(const StyleScopeOverrides& value);
    void retained_checkpoint() override;
    bool theme_retry_pending_{};
    std::uint64_t source_revision_{};

    std::optional<Binding<StyleScopeOverrides>> state_;
    StyleScopeOverrides overrides_;
    const Theme* inherited_theme_{};
    Theme resolved_;
    std::function<void(ThemeInvalidation)> change_invalidator_;
    std::shared_ptr<ObserverState> observer_;
    Binding<StyleScopeOverrides>::Subscription subscription_;
};

} // namespace detail

/// One explicit lexical style scope. The value form is immutable for the
/// retained lifetime; the Binding-backed form supports UI-thread replacement
/// with exact effective no-op/paint/layout invalidation classification. Legacy
/// State syntax delegates through State::binding() and never retains State*.
class StyleScope {
public:
    template <class Child>
    StyleScope(StyleScopeOverrides overrides, Child&& child)
        : overrides_(std::move(overrides)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
    StyleScope(Binding<StyleScopeOverrides> overrides, Child&& child)
        : state_(std::move(overrides)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    template <class Child>
    StyleScope(State<StyleScopeOverrides>& overrides, Child&& child)
        : StyleScope(overrides.binding(), std::forward<Child>(child)) {}

    Spec spec() &&;

private:
    std::optional<Binding<StyleScopeOverrides>> state_;
    StyleScopeOverrides overrides_;
    std::vector<Spec> children_;
};

} // namespace ui
