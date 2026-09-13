#include "example_support.hpp"

#include <type_traits>

namespace {

[[nodiscard]] bool same_color(ui::Color a, ui::Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

template <class T>
concept HasWidthMember = requires(T value) { value.width; };

template <class T>
concept HasEnabledMember = requires(T value) { value.enabled; };

ui::StyleScopeOverrides outer_scope() {
    ui::StyleScopeOverrides result;
    result.palette.accent = ui::Color{0.18f, 0.52f, 0.88f, 1.0f};
    result.palette.text = ui::Color{0.86f, 0.90f, 0.96f, 1.0f};
    result.typography.control_size = 15.0f;
    result.spacing.large = 18.0f;
    result.controls.control_height = 38.0f;
    return result;
}

ui::StyleScopeOverrides inner_scope() {
    ui::StyleScopeOverrides result;
    result.palette.accent = ui::Color{0.92f, 0.42f, 0.20f, 1.0f};
    result.radii.medium = 11.0f;
    return result;
}

int self_test() {
    static_assert(!HasWidthMember<ui::StyleScopeOverrides>);
    static_assert(!HasEnabledMember<ui::StyleScopeOverrides>);

    const auto base = ui::default_theme();
    const auto outer = ui::apply_style_scope_overrides(base, outer_scope());
    const auto nested = ui::apply_style_scope_overrides(outer, inner_scope());

    if (!same_color(outer.palette.accent, *outer_scope().palette.accent) ||
        !same_color(nested.palette.accent, *inner_scope().palette.accent)) {
        return example::fail("nearest scope did not win for the explicitly overridden field");
    }
    if (!same_color(nested.palette.text, *outer_scope().palette.text) ||
        nested.typography.control_size != 15.0f || nested.spacing.large != 18.0f ||
        nested.controls.control_height != 38.0f) {
        return example::fail("inner partial override erased unrelated outer fields");
    }
    if (nested.radii.medium != 11.0f || nested.radii.sm != base.radii.sm) {
        return example::fail("typed radius override did not preserve inherited siblings");
    }

    auto paint_before = outer_scope();
    auto paint_after = paint_before;
    paint_after.palette.accent = ui::Color{0.20f, 0.56f, 0.92f, 1.0f};
    if (ui::classify_style_scope_change(base, paint_before, paint_after) !=
        ui::ThemeInvalidation::Paint) {
        return example::fail("paint-only scope change was not classified as paint");
    }

    auto layout_after = paint_before;
    layout_after.typography.control_size = 19.0f;
    if (ui::classify_style_scope_change(base, paint_before, layout_after) !=
        ui::ThemeInvalidation::Layout) {
        return example::fail("typography scope change was not classified as layout");
    }
    if (ui::classify_style_scope_change(base, paint_before, paint_before) !=
        ui::ThemeInvalidation::None) {
        return example::fail("equal scope replacement was not a no-op");
    }

    // T038 consumes an already-resolved inherited recipe before applying the
    // component-local recipe and visual-state patch. This proves the T039 pure
    // layer feeds that existing seam without inventing a second style engine.
    const auto inherited_button = ui::default_button_style(nested);
    ui::ButtonStyle explicit_button;
    explicit_button.base.control_height = 51.0f;
    explicit_button.base.fill = ui::Color{0.07f, 0.09f, 0.12f, 1.0f};
    const auto resolved = ui::resolve_button_style(
        inherited_button,
        explicit_button,
        ui::VisualState{.enabled = true, .hovered = true});
    if (resolved.control_height != 51.0f ||
        !same_color(resolved.fill, nested.palette.control_hover)) {
        return example::fail("component/state precedence did not follow the T038 resolver seam");
    }

    return 0;
}

ui::UI make_demo() {
    auto theme = ui::apply_style_scope_overrides(ui::default_theme(), outer_scope());
    theme = ui::apply_style_scope_overrides(std::move(theme), inner_scope());
    return ui::UI{
        ui::Column{
            ui::Header{"T039 — Scoped Style Resolution"},
            ui::Label{"Typed lexical overrides compose outer to inner; component styles and visual state remain T038-owned."},
            ui::Button{"Resolved inherited theme", [] {}}
        }.gap(theme.spacing.large),
        std::move(theme)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto ui = make_demo();
    return example::run_window(ui, "NativeUI T039 Style Scope", {720.0f, 300.0f});
}
