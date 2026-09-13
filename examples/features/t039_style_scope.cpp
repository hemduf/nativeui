#include "example_support.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace {

[[nodiscard]] bool same_color(ui::Color a, ui::Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

[[nodiscard]] ui::Color byte_color(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    constexpr float scale = 1.0f / 255.0f;
    return {static_cast<float>(r) * scale,
            static_cast<float>(g) * scale,
            static_cast<float>(b) * scale,
            1.0f};
}

[[nodiscard]] bool pixel_matches(
    ui::Rgba8 pixel,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    int tolerance = 1) noexcept {
    const auto within = [tolerance](std::uint8_t actual, std::uint8_t expected) {
        const int delta = static_cast<int>(actual) - static_cast<int>(expected);
        return delta >= -tolerance && delta <= tolerance;
    };
    return within(pixel.r, r) && within(pixel.g, g) && within(pixel.b, b) && pixel.a == 255;
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

int verify_retained_scope_invalidation() {
    ui::StyleScopeOverrides initial;
    initial.palette.control_background = ui::Color{0.12f, 0.18f, 0.25f, 1.0f};
    ui::State<ui::StyleScopeOverrides> scoped{initial};

    ui::UI tree{ui::Row{
        ui::StyleScope{scoped, ui::Button{"Scoped", [] {}}},
        ui::Button{"Sibling", [] {}}
    }.gap(40.0f)};
    const ui::Size size{600.0f, 160.0f};
    tree.resize(size);
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("retained scope baseline render failed");
    if (tree.dirty()) return example::fail("baseline render left retained scope dirty");

    auto paint_only = scoped.get();
    paint_only.palette.control_background = ui::Color{0.36f, 0.16f, 0.10f, 1.0f};
    scoped.set(paint_only);
    if (tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("paint-only scope replacement did not stay paint-only");
    }
    if (tree.dirty_regions().empty()) {
        return example::fail("paint-only scope replacement produced no scoped dirty region");
    }
    for (const auto rect : tree.dirty_regions()) {
        if (rect.w >= size.w && rect.h >= size.h) {
            return example::fail("paint-only scope replacement dirtied the whole UI");
        }
    }

    if (!renderer.render(tree)) return example::fail("paint-only scope rerender failed");
    scoped.set(paint_only);
    if (tree.dirty()) return example::fail("equal scope replacement invalidated the tree");

    auto layout = scoped.get();
    layout.controls.control_height = 82.0f;
    scoped.set(layout);
    if (!tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("layout-affecting scope replacement missed layout+paint invalidation");
    }
    return 0;
}

int verify_nested_retained_precedence() {
    ui::StyleScopeOverrides outer;
    outer.palette.control_background = ui::Color{0.14f, 0.20f, 0.28f, 1.0f};
    outer.controls.control_height = 48.0f;

    ui::StyleScopeOverrides inner;
    inner.controls.control_height = 86.0f;

    ui::UI outer_only{ui::StyleScope{outer, ui::Button{"Outer", [] {}}}};
    ui::UI nested{ui::StyleScope{
        outer,
        ui::StyleScope{inner, ui::Button{"Nested", [] {}}}}};

    const auto outer_metrics = outer_only.measure();
    const auto nested_metrics = nested.measure();
    if (nested_metrics.preferred.h <= outer_metrics.preferred.h) {
        return example::fail("nearest retained scope did not override inherited control geometry");
    }
    return 0;
}

int verify_sibling_isolation() {
    ui::StyleScopeOverrides tall;
    tall.controls.control_height = 92.0f;

    ui::UI isolated{ui::Column{
        ui::StyleScope{tall, ui::Button{"Scoped", [] {}}},
        ui::Button{"Sibling", [] {}}
    }.gap(0.0f)};
    ui::UI both_scoped{ui::StyleScope{
        tall,
        ui::Column{
            ui::Button{"One", [] {}},
            ui::Button{"Two", [] {}}
        }.gap(0.0f)}};

    const auto isolated_metrics = isolated.measure();
    const auto both_metrics = both_scoped.measure();
    if (isolated_metrics.preferred.h >= both_metrics.preferred.h) {
        return example::fail("scope leaked into an unscoped sibling subtree");
    }
    return 0;
}

int verify_scope_restoration() {
    ui::StyleScopeOverrides outer;
    outer.palette.control_background = byte_color(65, 66, 67);

    ui::StyleScopeOverrides inner_value;
    inner_value.palette.control_background = byte_color(68, 69, 70);
    ui::State<ui::StyleScopeOverrides> inner{inner_value};

    constexpr ui::Size size{180.0f, 64.0f};
    ui::UI tree{ui::StyleScope{
        outer,
        ui::StyleScope{inner, ui::Button{"Restore", [] {}}}}};
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("scope restoration baseline render failed");
    if (!pixel_matches(renderer.pixel(20, 20), 68, 69, 70)) {
        return example::fail("inner scope did not apply before restoration");
    }

    inner.set(ui::StyleScopeOverrides{});
    if (!renderer.render(tree)) return example::fail("scope restoration rerender failed");
    if (!pixel_matches(renderer.pixel(20, 20), 65, 66, 67)) {
        return example::fail("removing inner overrides left stale resolved descendant style");
    }
    return 0;
}

int verify_dynamic_descendant_ancestry() {
    ui::State<bool> visible{false};
    ui::StyleScopeOverrides scoped;
    scoped.palette.control_background = byte_color(74, 75, 76);

    constexpr ui::Size size{180.0f, 64.0f};
    ui::UI tree{ui::StyleScope{
        scoped,
        ui::If{visible, ui::Button{"Inserted", [] {}}}}};
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("dynamic empty-scope baseline render failed");

    visible.set(true);
    if (!renderer.render(tree)) return example::fail("dynamic scoped insertion render failed");
    if (!pixel_matches(renderer.pixel(20, 20), 74, 75, 76)) {
        return example::fail("T058 descendant did not resolve the current lexical scope from ancestry");
    }
    return 0;
}

int verify_two_tree_isolation() {
    ui::StyleScopeOverrides first_value;
    first_value.palette.control_background = byte_color(80, 81, 82);
    ui::StyleScopeOverrides second_value;
    second_value.palette.control_background = byte_color(83, 84, 85);
    ui::State<ui::StyleScopeOverrides> first{first_value};
    ui::State<ui::StyleScopeOverrides> second{second_value};

    constexpr ui::Size size{180.0f, 64.0f};
    ui::UI first_tree{ui::StyleScope{first, ui::Button{"First", [] {}}}};
    ui::UI second_tree{ui::StyleScope{second, ui::Button{"Second", [] {}}}};
    ui::HeadlessRenderer first_renderer{size, 1.0f};
    ui::HeadlessRenderer second_renderer{size, 1.0f};
    if (!first_renderer.render(first_tree) || !second_renderer.render(second_tree)) {
        return example::fail("two-tree scope baseline render failed");
    }
    const auto second_before = second_renderer.pixel(20, 20);

    auto next = first.get();
    next.palette.control_background = byte_color(86, 87, 88);
    first.set(next);
    if (!second_tree.dirty()) {
        if (!first_renderer.render(first_tree)) return example::fail("first isolated scope rerender failed");
    } else {
        return example::fail("mutating one UI scope dirtied an independent UI tree");
    }
    if (!second_renderer.render(second_tree)) return example::fail("second isolated scope rerender failed");
    const auto second_after = second_renderer.pixel(20, 20);
    if (second_before.r != second_after.r || second_before.g != second_after.g ||
        second_before.b != second_after.b || second_before.a != second_after.a) {
        return example::fail("mutating one UI scope changed another UI tree");
    }
    return 0;
}

int verify_headless_scope_golden() {
    constexpr ui::Size size{180.0f, 64.0f};
    std::array<ui::Rgba8, 4> actual{};

    {
        ui::StyleScopeOverrides outer;
        outer.palette.control_background = byte_color(65, 66, 67);
        ui::UI tree{ui::StyleScope{outer, ui::Button{"Outer", [] {}}}};
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("outer golden render failed");
        actual[0] = renderer.pixel(20, 20);
    }

    {
        ui::StyleScopeOverrides outer;
        outer.palette.control_background = byte_color(65, 66, 67);
        ui::StyleScopeOverrides inner;
        inner.palette.control_background = byte_color(68, 69, 70);
        ui::UI tree{ui::StyleScope{
            outer,
            ui::StyleScope{inner, ui::Button{"Inner", [] {}}}}};
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("inner golden render failed");
        actual[1] = renderer.pixel(20, 20);
    }

    {
        ui::StyleScopeOverrides outer;
        outer.palette.control_background = byte_color(65, 66, 67);
        ui::ButtonStyle explicit_style;
        explicit_style.base.fill = byte_color(71, 72, 73);
        ui::UI tree{ui::StyleScope{
            outer,
            ui::Button{"Explicit", [] {}}.style(explicit_style)}};
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("explicit golden render failed");
        actual[2] = renderer.pixel(20, 20);
    }

    {
        ui::State<bool> visible{false};
        ui::StyleScopeOverrides outer;
        outer.palette.control_background = byte_color(74, 75, 76);
        ui::UI tree{ui::StyleScope{
            outer,
            ui::If{visible, ui::Button{"Dynamic", [] {}}}}};
        ui::HeadlessRenderer renderer{size, 1.0f};
        if (!renderer.render(tree)) return example::fail("dynamic golden baseline render failed");
        visible.set(true);
        if (!renderer.render(tree)) return example::fail("dynamic golden insertion render failed");
        actual[3] = renderer.pixel(20, 20);
    }

    constexpr std::array<std::array<std::uint8_t, 3>, 4> expected{{
        {{65, 66, 67}},
        {{68, 69, 70}},
        {{71, 72, 73}},
        {{74, 75, 76}},
    }};
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (!pixel_matches(actual[i], expected[i][0], expected[i][1], expected[i][2])) {
            return example::fail("nested StyleScope headless golden matrix changed");
        }
    }
    return 0;
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

    if (const auto retained = verify_retained_scope_invalidation(); retained != 0) return retained;
    if (const auto retained = verify_nested_retained_precedence(); retained != 0) return retained;
    if (const auto isolated = verify_sibling_isolation(); isolated != 0) return isolated;
    if (const auto restored = verify_scope_restoration(); restored != 0) return restored;
    if (const auto dynamic = verify_dynamic_descendant_ancestry(); dynamic != 0) return dynamic;
    if (const auto isolated = verify_two_tree_isolation(); isolated != 0) return isolated;
    if (const auto golden = verify_headless_scope_golden(); golden != 0) return golden;
    return 0;
}

ui::UI make_demo() {
    auto outer = outer_scope();
    auto inner = inner_scope();
    return ui::UI{ui::Column{
        ui::Header{"T039 — Scoped Style Resolution"},
        ui::Label{"Outer/inner lexical scopes affect only their retained descendants."},
        ui::StyleScope{
            outer,
            ui::Column{
                ui::Button{"Outer scope", [] {}},
                ui::StyleScope{inner, ui::Button{"Nearest inner scope", [] {}}}
            }.gap(outer.spacing.large.value_or(12.0f))},
        ui::Button{"Unscoped sibling", [] {}}
    }.gap(14.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto ui = make_demo();
    return example::run_window(ui, "NativeUI T039 Style Scope", {720.0f, 360.0f});
}
