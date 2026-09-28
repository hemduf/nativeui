#pragma once

/// \file
/// Lexically inheritable Theme overrides for one retained subtree.
///
/// Override values are owned C++ data. Float geometry/typography tokens use
/// logical UI units and are not clamped by this layer. Live StyleScope updates
/// are UI/main-thread work and are not audio/DSP real-time operations.

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

[[nodiscard]] inline bool style_scope_color_equal(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

[[nodiscard]] inline bool style_scope_optional_color_equal(
    const std::optional<Color>& a, const std::optional<Color>& b) noexcept {
    if (a.has_value() != b.has_value()) return false;
    return !a || style_scope_color_equal(*a, *b);
}

} // namespace detail

/// Sparse inheritable palette overrides for one lexical scope.
/// Empty optionals inherit from the nearest outer scope/root Theme. Colors are
/// copied verbatim; this value owns no retained/backend object.
struct StyleScopePaletteOverrides {
    /// Root/background color override.
    std::optional<Color> background;
    /// Elevated/control surface color override.
    std::optional<Color> surface;
    /// Primary text color override.
    std::optional<Color> text;
    /// Secondary/muted text color override.
    std::optional<Color> muted_text;
    /// Standard border color override.
    std::optional<Color> border;
    /// Accent/active color override.
    std::optional<Color> accent;
    /// Disabled-content color override.
    std::optional<Color> disabled;
    /// Selection-highlight color override.
    std::optional<Color> selection;
    /// Focus-indicator color override.
    std::optional<Color> focus;
    /// Standard control background override.
    std::optional<Color> control_background;
    /// Standard hover background override.
    std::optional<Color> control_hover;
    /// Active/pressed highlight override.
    std::optional<Color> active_highlight;
    /// Standard track color override.
    std::optional<Color> track;

    /// Exact field-wise equality, including exact RGBA float components.
    [[nodiscard]] bool operator==(const StyleScopePaletteOverrides& other) const noexcept {
        return detail::style_scope_optional_color_equal(background, other.background) &&
            detail::style_scope_optional_color_equal(surface, other.surface) &&
            detail::style_scope_optional_color_equal(text, other.text) &&
            detail::style_scope_optional_color_equal(muted_text, other.muted_text) &&
            detail::style_scope_optional_color_equal(border, other.border) &&
            detail::style_scope_optional_color_equal(accent, other.accent) &&
            detail::style_scope_optional_color_equal(disabled, other.disabled) &&
            detail::style_scope_optional_color_equal(selection, other.selection) &&
            detail::style_scope_optional_color_equal(focus, other.focus) &&
            detail::style_scope_optional_color_equal(
                control_background, other.control_background) &&
            detail::style_scope_optional_color_equal(control_hover, other.control_hover) &&
            detail::style_scope_optional_color_equal(active_highlight, other.active_highlight) &&
            detail::style_scope_optional_color_equal(track, other.track);
    }
};

/// Sparse inheritable typography defaults.
/// Family strings/vectors are owned; sizes use logical UI units. These values
/// affect descendants only and do not perform font discovery themselves.
struct StyleScopeTypographyOverrides {
    /// Owned preferred font-family name.
    std::optional<std::string> family;
    /// Owned ordered fallback-family names.
    std::optional<std::vector<std::string>> fallback_families;
    /// Base/body font size in logical UI units.
    std::optional<float> base_size;
    /// Standard control font size in logical UI units.
    std::optional<float> control_size;
    /// Label font size in logical UI units.
    std::optional<float> label_size;
    /// Base/body font weight.
    std::optional<FontWeight> base_weight;
    /// Standard control font weight.
    std::optional<FontWeight> control_weight;
    /// Label font weight.
    std::optional<FontWeight> label_weight;
    /// Inherited font slant.
    std::optional<FontSlant> slant;

    /// Exact value equality, including ordered fallback families and floats.
    [[nodiscard]] bool operator==(const StyleScopeTypographyOverrides&) const = default;
};

/// Sparse inheritable spacing-scale overrides, all in logical UI units.
struct StyleScopeSpacingOverrides {
    /// Extra-small spacing token.
    std::optional<float> xs;
    /// Small spacing token.
    std::optional<float> sm;
    /// Medium spacing token.
    std::optional<float> medium;
    /// Large spacing token.
    std::optional<float> large;
    /// Extra-large spacing token.
    std::optional<float> xl;

    /// Exact token equality.
    [[nodiscard]] bool operator==(const StyleScopeSpacingOverrides&) const = default;
};

/// Sparse inheritable corner-radius overrides in logical UI units.
struct StyleScopeRadiiOverrides {
    /// Small radius token.
    std::optional<float> sm;
    /// Medium radius token.
    std::optional<float> medium;
    /// Large radius token.
    std::optional<float> large;

    /// Exact token equality.
    [[nodiscard]] bool operator==(const StyleScopeRadiiOverrides&) const = default;
};

/// Sparse inheritable standard-control metrics.
///
/// All float values use logical UI units. Per-instance layout constraints,
/// scroll/model/callback/availability state are intentionally not inheritable.
struct StyleScopeControlOverrides {
    /// Standard minimum control width; layout-affecting.
    std::optional<float> minimum_width;
    /// Standard control height; layout-affecting.
    std::optional<float> control_height;
    /// Minimum interactive target size; layout-affecting.
    std::optional<float> minimum_hit_target;
    /// Standard slider thumb diameter.
    std::optional<float> thumb_diameter;
    /// Standard track thickness.
    std::optional<float> track_thickness;
    /// Standard border thickness.
    std::optional<float> border_width;
    /// Standard focus-ring thickness.
    std::optional<float> focus_ring_width;

    /// Exact token equality.
    [[nodiscard]] bool operator==(const StyleScopeControlOverrides&) const = default;
};

/// One complete sparse lexical-scope patch.
///
/// Empty fields inherit from the nearest outer scope/root Theme. Applying
/// patches outer-to-inner implements nearest-scope-wins independently per
/// field; the object is ordinary owned value data with no retained identity.
struct StyleScopeOverrides {
    /// Inheritable palette fields.
    StyleScopePaletteOverrides palette;
    /// Inheritable typography fields.
    StyleScopeTypographyOverrides typography;
    /// Inheritable spacing tokens.
    StyleScopeSpacingOverrides spacing;
    /// Inheritable corner-radius tokens.
    StyleScopeRadiiOverrides radii;
    /// Inheritable standard-control metrics.
    StyleScopeControlOverrides controls;

    /// Exact field-wise value equality.
    [[nodiscard]] bool operator==(const StyleScopeOverrides&) const = default;
};

namespace detail {

template <class T>
inline void apply_scope_value(T& target, const std::optional<T>& value) {
    if (value) target = *value;
}

} // namespace detail

/// Apply one scope patch to an already-resolved inherited Theme value.
///
/// `inherited` is passed by value and the returned Theme owns all copied data;
/// the input Theme/overrides may be destroyed immediately after return. Empty
/// optionals preserve inherited fields. Call outer-to-inner for nested scopes.
/// The function invokes no callbacks/native work, but string/vector copies may
/// allocate and propagate allocation failure.
[[nodiscard]] inline Theme apply_style_scope_overrides(
    Theme inherited, const StyleScopeOverrides& overrides) {
    detail::apply_scope_value(inherited.palette.background, overrides.palette.background);
    detail::apply_scope_value(inherited.palette.surface, overrides.palette.surface);
    detail::apply_scope_value(inherited.palette.text, overrides.palette.text);
    detail::apply_scope_value(inherited.palette.muted_text, overrides.palette.muted_text);
    detail::apply_scope_value(inherited.palette.border, overrides.palette.border);
    detail::apply_scope_value(inherited.palette.accent, overrides.palette.accent);
    detail::apply_scope_value(inherited.palette.disabled, overrides.palette.disabled);
    detail::apply_scope_value(inherited.palette.selection, overrides.palette.selection);
    detail::apply_scope_value(inherited.palette.focus, overrides.palette.focus);
    detail::apply_scope_value(
        inherited.palette.control_background, overrides.palette.control_background);
    detail::apply_scope_value(inherited.palette.control_hover, overrides.palette.control_hover);
    detail::apply_scope_value(
        inherited.palette.active_highlight, overrides.palette.active_highlight);
    detail::apply_scope_value(inherited.palette.track, overrides.palette.track);

    detail::apply_scope_value(inherited.typography.family, overrides.typography.family);
    detail::apply_scope_value(
        inherited.typography.fallback_families, overrides.typography.fallback_families);
    detail::apply_scope_value(inherited.typography.base_size, overrides.typography.base_size);
    detail::apply_scope_value(
        inherited.typography.control_size, overrides.typography.control_size);
    detail::apply_scope_value(inherited.typography.label_size, overrides.typography.label_size);
    detail::apply_scope_value(
        inherited.typography.base_weight, overrides.typography.base_weight);
    detail::apply_scope_value(
        inherited.typography.control_weight, overrides.typography.control_weight);
    detail::apply_scope_value(
        inherited.typography.label_weight, overrides.typography.label_weight);
    detail::apply_scope_value(inherited.typography.slant, overrides.typography.slant);

    detail::apply_scope_value(inherited.spacing.xs, overrides.spacing.xs);
    detail::apply_scope_value(inherited.spacing.sm, overrides.spacing.sm);
    detail::apply_scope_value(inherited.spacing.medium, overrides.spacing.medium);
    detail::apply_scope_value(inherited.spacing.large, overrides.spacing.large);
    detail::apply_scope_value(inherited.spacing.xl, overrides.spacing.xl);

    detail::apply_scope_value(inherited.radii.sm, overrides.radii.sm);
    detail::apply_scope_value(inherited.radii.medium, overrides.radii.medium);
    detail::apply_scope_value(inherited.radii.large, overrides.radii.large);

    detail::apply_scope_value(
        inherited.controls.minimum_width, overrides.controls.minimum_width);
    detail::apply_scope_value(
        inherited.controls.control_height, overrides.controls.control_height);
    detail::apply_scope_value(
        inherited.controls.minimum_hit_target, overrides.controls.minimum_hit_target);
    detail::apply_scope_value(
        inherited.controls.thumb_diameter, overrides.controls.thumb_diameter);
    detail::apply_scope_value(
        inherited.controls.track_thickness, overrides.controls.track_thickness);
    detail::apply_scope_value(
        inherited.controls.border_width, overrides.controls.border_width);
    detail::apply_scope_value(
        inherited.controls.focus_ring_width, overrides.controls.focus_ring_width);
    return inherited;
}

/// Classify effective retained work when one scope patch is replaced.
///
/// Both patches are resolved against the supplied inherited Theme before
/// comparison, so syntactically different overrides that produce the same
/// effective Theme return None. Layout-affecting theme tokens return Layout;
/// other effective changes return Paint. This pure helper performs no retained
/// mutation/callbacks, but Theme copies may allocate.
[[nodiscard]] inline ThemeInvalidation classify_style_scope_change(
    const Theme& inherited,
    const StyleScopeOverrides& before,
    const StyleScopeOverrides& after) {
    return classify_theme_change(
        apply_style_scope_overrides(inherited, before),
        apply_style_scope_overrides(inherited, after));
}

namespace detail {

class StyleScopeComponent final : public Component, public ThemeBinding {
public:
    explicit StyleScopeComponent(StyleScopeOverrides overrides)
        : overrides_(std::move(overrides)), resolved_(default_theme()) {}

    explicit StyleScopeComponent(Binding<StyleScopeOverrides> state)
        : state_(std::move(state)), overrides_(state_->get()), resolved_(default_theme()) {}

    void bind_theme(const Theme& theme) noexcept override {
        ThemeBinding::bind_theme(theme);
        inherited_theme_ = &theme;
        resolved_ = apply_style_scope_overrides(theme, overrides_);
    }

    [[nodiscard]] const Theme& descendant_theme() const noexcept override {
        return resolved_;
    }

    void set_theme_change_invalidator(
        std::function<void(ThemeInvalidation)> callback) override {
        change_invalidator_ = std::move(callback);
    }

    [[nodiscard]] bool availability_change_affects_paint(
        const ComponentAvailability&,
        const ComponentAvailability&) const noexcept override {
        return false;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void mount(MountContext&) override {
        if (!state_ || !state_->valid()) return;
        observer_ = std::make_shared<ObserverState>();
        observer_->apply = [this](const StyleScopeOverrides& value) {
            replace_overrides(value);
        };
        subscription_ = state_->observe(
            [weak = std::weak_ptr<ObserverState>{observer_}](const StyleScopeOverrides& value) {
                if (const auto observer = weak.lock(); observer && observer->apply) {
                    observer->apply(value);
                }
            });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        if (observer_) observer_->apply = {};
        observer_.reset();
        change_invalidator_ = {};
    }

    void paint(PaintContext&) const override {}

private:
    struct ObserverState {
        std::function<void(const StyleScopeOverrides&)> apply;
    };

    void replace_overrides(const StyleScopeOverrides& value) {
        if (!inherited_theme_) {
            overrides_ = value;
            return;
        }
        auto next = apply_style_scope_overrides(*inherited_theme_, value);
        const auto invalidation = classify_theme_change(resolved_, next);
        overrides_ = value;
        resolved_ = std::move(next);
        if (invalidation != ThemeInvalidation::None && change_invalidator_) {
            change_invalidator_(invalidation);
        }
    }

    std::optional<Binding<StyleScopeOverrides>> state_;
    StyleScopeOverrides overrides_;
    const Theme* inherited_theme_{};
    Theme resolved_;
    std::function<void(ThemeInvalidation)> change_invalidator_;
    std::shared_ptr<ObserverState> observer_;
    Binding<StyleScopeOverrides>::Subscription subscription_;
};

} // namespace detail

/// Retained lexical style boundary for exactly one child subtree.
///
/// The value form owns an immutable patch. Binding/State forms subscribe only
/// while mounted, apply updates in the UI/main-thread domain, and classify the
/// effective descendant change as None/Paint/Layout. The scope is transparent
/// to child constraints/placement and introduces no process-global style state.
class StyleScope {
public:
    /// Construct an immutable scope, owning `overrides` and the child's Spec.
    template <class Child>
    StyleScope(StyleScopeOverrides overrides, Child&& child)
        : overrides_(std::move(overrides)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Construct a live scope from a Binding and own the child's Spec.
    /// The Binding follows the normal State/Binding UI-thread/lifetime contract.
    template <class Child>
    StyleScope(Binding<StyleScopeOverrides> overrides, Child&& child)
        : state_(std::move(overrides)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Convenience overload using `overrides.binding()`; no raw State* is kept.
    template <class Child>
    StyleScope(State<StyleScopeOverrides>& overrides, Child&& child)
        : StyleScope(overrides.binding(), std::forward<Child>(child)) {}

    /// Consume the builder and return a retained specification.
    ///
    /// The produced Spec owns either the immutable override value or a copy of
    /// the Binding handle plus the single child Spec. Live subscriptions are
    /// established by the retained component on mount and released on unmount.
    Spec spec() && {
        if (state_) {
            auto state = *state_;
            return Spec{
                [state] { return std::make_unique<detail::StyleScopeComponent>(state); },
                std::move(children_)};
        }
        auto overrides = std::move(overrides_);
        return Spec{
            [overrides = std::move(overrides)]() mutable {
                return std::make_unique<detail::StyleScopeComponent>(std::move(overrides));
            },
            std::move(children_)};
    }

private:
    std::optional<Binding<StyleScopeOverrides>> state_;
    StyleScopeOverrides overrides_;
    std::vector<Spec> children_;
};

} // namespace ui
