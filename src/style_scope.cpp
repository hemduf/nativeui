#include <nativeui/style_scope.hpp>

namespace ui {
namespace {
// Reconstruct text from bytes at this allocation boundary. The pinned Skia
// archive can contribute an older libc++ copy helper without unwind metadata;
// relying on that helper prevents a bad_alloc from reaching our recovery path.
std::string copy_scope_text(const std::string& value) {
    return std::string{value.data(), value.size()};
}
std::vector<std::string> copy_scope_fallbacks(const std::vector<std::string>& values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto& value : values) result.push_back(copy_scope_text(value));
    return result;
}
Theme copy_scope_theme(const Theme& value) {
    ThemeTypography typography{
        copy_scope_text(value.typography.family),
        copy_scope_fallbacks(value.typography.fallback_families),
        value.typography.base_size, value.typography.control_size, value.typography.label_size,
        value.typography.base_weight, value.typography.control_weight, value.typography.label_weight,
        value.typography.slant};
    return {value.palette, std::move(typography), value.spacing, value.radii, value.controls};
}
StyleScopeOverrides copy_scope_overrides(const StyleScopeOverrides& value) {
    const auto& source = value.typography;
    StyleScopeTypographyOverrides typography{
        source.family ? std::optional<std::string>{copy_scope_text(*source.family)} : std::nullopt,
        source.fallback_families
            ? std::optional<std::vector<std::string>>{copy_scope_fallbacks(*source.fallback_families)}
            : std::nullopt,
        source.base_size, source.control_size, source.label_size,
        source.base_weight, source.control_weight, source.label_weight, source.slant};
    return {value.palette, std::move(typography), value.spacing, value.radii, value.controls};
}
} // namespace

bool StyleScopePaletteOverrides::operator==(const StyleScopePaletteOverrides& other) const noexcept {
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

detail::StyleScopeComponent::StyleScopeComponent(StyleScopeOverrides overrides)
    : overrides_(std::move(overrides)), resolved_(default_theme()) {}

detail::StyleScopeComponent::StyleScopeComponent(Binding<StyleScopeOverrides> state)
    : source_revision_(state.revision()), state_(std::move(state)),
      overrides_(copy_scope_overrides(state_->get())), resolved_(default_theme()) {}

void detail::StyleScopeComponent::bind_theme(const Theme& theme) noexcept {
    ThemeBinding::bind_theme(theme);
    inherited_theme_ = &theme;
    try {
        auto next = apply_style_scope_overrides(copy_scope_theme(theme), overrides_);
        resolved_ = std::move(next);
        theme_retry_pending_ = false;
    } catch (...) {
        // Preserve the last coherent descendant value at the historical
        // noexcept boundary. A later retained checkpoint retries the new parent.
        theme_retry_pending_ = true;
    }
}

const Theme& detail::StyleScopeComponent::descendant_theme() const noexcept {
    return resolved_;
}

void detail::StyleScopeComponent::set_theme_change_invalidator(
    std::function<void(ThemeInvalidation)> callback) {
    change_invalidator_ = std::move(callback);
}

bool detail::StyleScopeComponent::availability_change_affects_paint(
    const ComponentAvailability&,
    const ComponentAvailability&) const noexcept {
    return false;
}

Size detail::StyleScopeComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().preferred;
}

ChildMetrics detail::StyleScopeComponent::measure_constrained(const Constraints& constraints,
    const std::vector<ChildMetrics>& children) const {
    auto result = Component::measure_constrained(constraints,children);
    if (!children.empty() && children.front().participates_in_layout)
        result.first_baseline = children.front().first_baseline;
    return result;
}

Size detail::StyleScopeComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty() ? Size{} : children.front().minimum;
}

Constraints detail::StyleScopeComponent::child_constraints(
    const Constraints& constraints, std::size_t, std::size_t) const {
    return constraints;
}

void detail::StyleScopeComponent::layout_children(
    Rect bounds,
    const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const {
    if (!placements.empty()) placements.front().bounds = bounds;
}

void detail::StyleScopeComponent::mount(MountContext&) {
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

void detail::StyleScopeComponent::unmount(LifecycleContext&) {
    subscription_.reset();
    if (observer_) observer_->apply = {};
    observer_.reset();
    change_invalidator_ = {};
}

void detail::StyleScopeComponent::paint(PaintContext&) const {}

void detail::StyleScopeComponent::retained_checkpoint() {
    if (state_ && state_->revision() != source_revision_) {
        replace_overrides(state_->get());
    }
    if (!theme_retry_pending_ || !inherited_theme_) return;
    auto next = apply_style_scope_overrides(copy_scope_theme(*inherited_theme_), overrides_);
    const auto invalidation = classify_theme_change(resolved_, next);
    auto notify = change_invalidator_;
    resolved_ = std::move(next);
    theme_retry_pending_ = false;
    if (invalidation != ThemeInvalidation::None && notify) notify(invalidation);
}

void detail::StyleScopeComponent::replace_overrides(const StyleScopeOverrides& value) {
    const auto revision = state_ ? state_->revision() : 0;
    auto owned = copy_scope_overrides(value);
    if (!inherited_theme_) {
        overrides_ = std::move(owned);
        source_revision_ = revision;
        return;
    }
    auto next = apply_style_scope_overrides(copy_scope_theme(*inherited_theme_), owned);
    const auto invalidation = classify_theme_change(resolved_, next);
    auto notify = change_invalidator_;
    if (state_ && state_->revision() != revision) {
        theme_retry_pending_ = true;
        return;
    }
    overrides_ = std::move(owned);
    resolved_ = std::move(next);
    source_revision_ = revision;
    theme_retry_pending_ = false;
    if (invalidation != ThemeInvalidation::None && notify) notify(invalidation);
}

Spec StyleScope::spec() && {
    if (state_) {
        auto state = *state_;
        return Spec{
            [state] { return std::make_unique<detail::StyleScopeComponent>(state); },
            std::move(children_)};
    }
    auto overrides = std::move(overrides_);
    return Spec{
        [overrides = std::move(overrides)] {
            return std::make_unique<detail::StyleScopeComponent>(copy_scope_overrides(overrides));
        },
        std::move(children_)};
}

bool detail::style_scope_color_equal(Color a, Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool detail::style_scope_optional_color_equal(
    const std::optional<Color>& a, const std::optional<Color>& b) noexcept {
    if (a.has_value() != b.has_value()) return false;
    return !a || style_scope_color_equal(*a, *b);
}

Theme apply_style_scope_overrides(
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

    if (overrides.typography.family)
        inherited.typography.family = copy_scope_text(*overrides.typography.family);
    if (overrides.typography.fallback_families)
        inherited.typography.fallback_families = copy_scope_fallbacks(*overrides.typography.fallback_families);
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

ThemeInvalidation classify_style_scope_change(
    const Theme& inherited,
    const StyleScopeOverrides& before,
    const StyleScopeOverrides& after) {
    return classify_theme_change(
        apply_style_scope_overrides(copy_scope_theme(inherited), before),
        apply_style_scope_overrides(copy_scope_theme(inherited), after));
}

} // namespace ui
