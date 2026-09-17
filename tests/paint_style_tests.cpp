#include "test_support.hpp"

#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>

namespace allocation_probe {

std::size_t allocation_count{};
bool fail_next_allocation{};

void* allocate(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc{};
    }
    ++allocation_count;
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc{};
}

void fail_next() noexcept { fail_next_allocation = true; }
void disable_failure() noexcept { fail_next_allocation = false; }

} // namespace allocation_probe

void* operator new(std::size_t size) { return allocation_probe::allocate(size); }
void* operator new[](std::size_t size) { return allocation_probe::allocate(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

static_assert(noexcept(ui::Brush{ui::Color{}}));
static_assert(std::is_copy_constructible_v<ui::Brush>);
static_assert(std::is_copy_assignable_v<ui::Brush>);
static_assert(std::is_nothrow_move_constructible_v<ui::Brush>);
static_assert(std::is_nothrow_move_assignable_v<ui::Brush>);
static_assert(std::is_nothrow_destructible_v<ui::Brush>);

namespace {

bool red_dominant(ui::Rgba8 pixel) {
    return pixel.r > 150 && pixel.r > pixel.g * 2 && pixel.r > pixel.b * 2;
}

bool blue_dominant(ui::Rgba8 pixel) {
    return pixel.b > 150 && pixel.b > pixel.r * 2 && pixel.b > pixel.g * 2;
}

bool black(ui::Rgba8 pixel) {
    return pixel.r < 8 && pixel.g < 8 && pixel.b < 8;
}

ui::LinearGradient red_blue_gradient(float width) {
    return ui::LinearGradient{
        {0.0f, 0.0f},
        {width, 0.0f},
        {
            ui::GradientStop{0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            ui::GradientStop{1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
    };
}

void two_stop_linear_gradient() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    };

    ui::UI tree{
        ui::Canvas{32.0f, 8.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 8.0f}, gradient);
        }}
    };

    ui::HeadlessRenderer renderer{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto left = renderer.pixel(2, 4);
    const auto middle = renderer.pixel(16, 4);
    const auto right = renderer.pixel(29, 4);

    NUI_CHECK(left.r > 180 && left.b < 80);
    NUI_CHECK(middle.r > 70 && middle.b > 70);
    NUI_CHECK(right.b > 180 && right.r < 80);
}

void multi_stop_linear_gradient() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f},
        {32.0f, 0.0f},
        {
            ui::GradientStop{0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            ui::GradientStop{0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
            ui::GradientStop{1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
    };

    ui::UI tree{
        ui::Canvas{32.0f, 8.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 8.0f}, gradient);
        }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto left = renderer.pixel(2, 4);
    const auto middle = renderer.pixel(16, 4);
    const auto right = renderer.pixel(29, 4);
    NUI_CHECK(left.r > left.g && left.r > left.b);
    NUI_CHECK(middle.g > middle.r && middle.g > middle.b);
    NUI_CHECK(right.b > right.r && right.b > right.g);
}

void radial_gradient() {
    const ui::RadialGradient gradient{
        {16.0f, 8.0f},
        8.0f,
        {
            ui::GradientStop{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
            ui::GradientStop{1.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
        },
    };

    ui::UI tree{
        ui::Canvas{32.0f, 16.0f, [gradient](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 32.0f, 16.0f}, gradient);
        }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto center = renderer.pixel(16, 8);
    const auto edge = renderer.pixel(23, 8);
    NUI_CHECK(center.r > 220 && center.g > 220 && center.b > 220);
    NUI_CHECK(edge.r < 80 && edge.g < 80 && edge.b < 80);
}

void opacity_and_blend_modes() {
    const ui::LinearGradient white{
        {0.0f, 0.0f}, {16.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}};
    const ui::LinearGradient multiply_color{
        {16.0f, 0.0f}, {32.0f, 0.0f},
        {0.5f, 0.5f, 1.0f, 1.0f}, {0.5f, 0.5f, 1.0f, 1.0f}};

    ui::UI tree{
        ui::Canvas{32.0f, 8.0f, [white, multiply_color](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
            g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, white,
                        ui::PaintOptions{0.25f, ui::BlendMode::SourceOver});

            g.fill_rect({16.0f, 0.0f, 16.0f, 8.0f}, {0.8f, 0.5f, 0.25f, 1.0f});
            g.fill_rect({16.0f, 0.0f, 16.0f, 8.0f}, multiply_color,
                        ui::PaintOptions{1.0f, ui::BlendMode::Multiply});
        }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto translucent = renderer.pixel(8, 4);
    NUI_CHECK(translucent.r > 45 && translucent.r < 85);
    NUI_CHECK(translucent.g > 45 && translucent.g < 85);
    NUI_CHECK(translucent.b > 45 && translucent.b < 85);

    const auto multiplied = renderer.pixel(24, 4);
    NUI_CHECK(multiplied.r > 80 && multiplied.r < 125);
    NUI_CHECK(multiplied.g > 45 && multiplied.g < 90);
    NUI_CHECK(multiplied.b > 45 && multiplied.b < 90);
}

void brush_noexcept_and_allocation_contract() {
    auto before = allocation_probe::allocation_count;
    {
        ui::Brush solid{{0.2f, 0.3f, 0.4f, 0.5f}};
        (void)solid;
    }
    NUI_CHECK(allocation_probe::allocation_count == before);

    ui::Brush source{red_blue_gradient(16.0f)};
    before = allocation_probe::allocation_count;
    ui::Brush moved{std::move(source)};
    NUI_CHECK(allocation_probe::allocation_count == before);

    ui::Brush assigned{{0.0f, 1.0f, 0.0f, 1.0f}};
    before = allocation_probe::allocation_count;
    assigned = std::move(moved);
    NUI_CHECK(allocation_probe::allocation_count == before);
}

void brush_gradient_owns_source_storage() {
    ui::Brush brush = [] {
        const ui::LinearGradient local{
            {0.0f, 0.0f}, {24.0f, 0.0f},
            {
                ui::GradientStop{0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
                ui::GradientStop{0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
                ui::GradientStop{1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
            }};
        return ui::Brush{local};
    }();

    ui::UI tree{
        ui::Canvas{24.0f, 8.0f, [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 24.0f, 8.0f}, brush);
        }}
    };
    ui::HeadlessRenderer renderer{{24.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red_dominant(renderer.pixel(2, 4)));
    NUI_CHECK(blue_dominant(renderer.pixel(21, 4)));
}

void brush_allocation_failure_is_transactional() {
    const ui::LinearGradient gradient{
        {0.0f, 0.0f}, {24.0f, 0.0f},
        {
            ui::GradientStop{0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
            ui::GradientStop{0.5f, {0.0f, 1.0f, 0.0f, 1.0f}},
            ui::GradientStop{1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
        }};

    bool construction_threw = false;
    allocation_probe::fail_next();
    try {
        const ui::Brush failed{gradient};
        (void)failed;
    } catch (const std::bad_alloc&) {
        construction_threw = true;
    }
    allocation_probe::disable_failure();
    NUI_CHECK(construction_threw);
    NUI_CHECK(gradient.stops().size() == 3);

    const ui::Brush source{gradient};
    bool copy_threw = false;
    allocation_probe::fail_next();
    try {
        const ui::Brush failed_copy{source};
        (void)failed_copy;
    } catch (const std::bad_alloc&) {
        copy_threw = true;
    }
    allocation_probe::disable_failure();
    NUI_CHECK(copy_threw);

    ui::Brush destination{{1.0f, 0.0f, 0.0f, 1.0f}};
    bool assignment_threw = false;
    allocation_probe::fail_next();
    try {
        destination = source;
    } catch (const std::bad_alloc&) {
        assignment_threw = true;
    }
    allocation_probe::disable_failure();
    NUI_CHECK(assignment_threw);

    ui::UI tree{
        ui::Canvas{8.0f, 8.0f, [destination](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, destination);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red_dominant(renderer.pixel(4, 4)));

    const ui::Brush later_copy{source};
    (void)later_copy;
}

void brush_move_transfers_value_and_clears_source() {
    ui::Brush source{red_blue_gradient(16.0f)};
    ui::Brush transferred{std::move(source)};
    ui::Brush assigned{{0.0f, 1.0f, 0.0f, 1.0f}};
    assigned = std::move(transferred);

    const ui::Brush copied_from_moved{transferred};
    ui::UI tree{
        ui::Canvas{32.0f, 8.0f,
            [source, transferred, copied_from_moved, assigned](ui::CanvasContext2D& g) {
                g.fill_rect({0.0f, 0.0f, 16.0f, 8.0f}, assigned);
                g.fill_rect({16.0f, 0.0f, 4.0f, 8.0f}, source);
                g.fill_rect({20.0f, 0.0f, 4.0f, 8.0f}, transferred);
                g.fill_rect({24.0f, 0.0f, 8.0f, 8.0f}, copied_from_moved);
            }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red_dominant(renderer.pixel(2, 4)));
    NUI_CHECK(blue_dominant(renderer.pixel(13, 4)));
    NUI_CHECK(black(renderer.pixel(18, 4)));
    NUI_CHECK(black(renderer.pixel(22, 4)));
    NUI_CHECK(black(renderer.pixel(28, 4)));
}

void brush_color_matches_legacy_color_path() {
    constexpr ui::Color color{0.75f, 0.20f, 0.45f, 0.60f};

    ui::UI legacy{
        ui::Canvas{16.0f, 8.0f, [color](ui::CanvasContext2D& g) {
            g.fill_rounded_rect({1.0f, 1.0f, 14.0f, 6.0f}, 2.0f, color);
        }}
    };
    ui::UI generic{
        ui::Canvas{16.0f, 8.0f, [color](ui::CanvasContext2D& g) {
            const ui::Brush brush{color};
            g.fill_rounded_rect({1.0f, 1.0f, 14.0f, 6.0f}, 2.0f, brush);
        }}
    };

    ui::HeadlessRenderer legacy_renderer{{16.0f, 8.0f}, 1.0f};
    ui::HeadlessRenderer generic_renderer{{16.0f, 8.0f}, 1.0f};
    NUI_CHECK(legacy_renderer.render(legacy));
    NUI_CHECK(generic_renderer.render(generic));
    NUI_CHECK(legacy_renderer.rgba_pixels() == generic_renderer.rgba_pixels());
}

void brush_transform_clip_opacity_and_blend() {
    const ui::Brush solid{{1.0f, 1.0f, 1.0f, 1.0f}};
    ui::UI tree{
        ui::Canvas{24.0f, 12.0f, [solid](ui::CanvasContext2D& g) {
            g.save();
            g.translate(8.0f, 2.0f);
            g.push_clip({0.0f, 0.0f, 6.0f, 6.0f});
            g.fill_rect({0.0f, 0.0f, 10.0f, 6.0f}, solid,
                        ui::PaintOptions{0.25f, ui::BlendMode::SourceOver});
            g.pop_clip();
            g.restore();
        }}
    };
    ui::HeadlessRenderer renderer{{24.0f, 12.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto inside = renderer.pixel(10, 4);
    NUI_CHECK(inside.r > 45 && inside.r < 85);
    NUI_CHECK(inside.g > 45 && inside.g < 85);
    NUI_CHECK(inside.b > 45 && inside.b < 85);
    NUI_CHECK(black(renderer.pixel(16, 4)));
    NUI_CHECK(black(renderer.pixel(2, 4)));
}

void brush_invalid_gradient_preserves_fallback() {
    const ui::Brush invalid{ui::LinearGradient{
        {0.0f, 0.0f}, {8.0f, 0.0f},
        {
            ui::GradientStop{0.75f, {1.0f, 0.0f, 0.0f, 1.0f}},
            ui::GradientStop{0.25f, {0.0f, 0.0f, 1.0f, 1.0f}},
        }}};
    ui::UI tree{
        ui::Canvas{8.0f, 8.0f, [invalid](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 8.0f, 8.0f}, invalid);
        }}
    };
    ui::HeadlessRenderer renderer{{8.0f, 8.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(red_dominant(renderer.pixel(4, 4)));
}

void brush_copies_are_multi_ui_lifetime_safe() {
    std::unique_ptr<ui::UI> first;
    std::unique_ptr<ui::UI> second;
    {
        const ui::Brush original{ui::RadialGradient{
            {8.0f, 8.0f}, 7.0f,
            {
                ui::GradientStop{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
                ui::GradientStop{1.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
            }}};
        const ui::Brush copy_a{original};
        const ui::Brush copy_b{original};
        first = std::make_unique<ui::UI>(
            ui::Canvas{16.0f, 16.0f, [copy_a](ui::CanvasContext2D& g) {
                g.circle({8.0f, 8.0f}, 7.0f, copy_a);
            }});
        second = std::make_unique<ui::UI>(
            ui::Canvas{16.0f, 16.0f, [copy_b](ui::CanvasContext2D& g) {
                g.circle({8.0f, 8.0f}, 7.0f, copy_b);
            }});
    }

    ui::HeadlessRenderer renderer_a{{16.0f, 16.0f}, 1.0f};
    ui::HeadlessRenderer renderer_b{{16.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer_a.render(*first));
    NUI_CHECK(renderer_b.render(*second));
    const auto center_before = renderer_b.pixel(8, 8);
    const auto edge_before = renderer_b.pixel(14, 8);
    NUI_CHECK(center_before.r > edge_before.r + 100);

    first.reset();
    NUI_CHECK(renderer_b.render(*second));
    const auto center_after = renderer_b.pixel(8, 8);
    NUI_CHECK(center_after.r > 220 && center_after.g > 220 && center_after.b > 220);
}

void brush_strokes_match_color_and_preserve_style() {
    constexpr ui::Color color{0.72f, 0.28f, 0.62f, 0.70f};

    ui::Path miter_path;
    miter_path.move_to({4.0f, 30.0f}).line_to({10.0f, 20.0f}).line_to({16.0f, 30.0f});
    ui::Path round_path;
    round_path.move_to({28.0f, 30.0f}).line_to({34.0f, 20.0f}).line_to({40.0f, 30.0f});
    ui::Path bevel_path;
    bevel_path.move_to({52.0f, 30.0f}).line_to({58.0f, 20.0f}).line_to({64.0f, 30.0f});

    const ui::StrokeStyle miter{3.0f, ui::StrokeCap::Butt, ui::StrokeJoin::Miter, 1.25f};
    const ui::StrokeStyle round{3.0f, ui::StrokeCap::Round, ui::StrokeJoin::Round, 4.0f};
    const ui::StrokeStyle bevel{3.0f, ui::StrokeCap::Square, ui::StrokeJoin::Bevel, 8.0f};

    ui::UI legacy{
        ui::Canvas{72.0f, 36.0f,
            [color, miter_path, round_path, bevel_path, miter, round, bevel](ui::CanvasContext2D& g) {
                g.stroke_rounded_rect({2.0f, 2.0f, 14.0f, 10.0f}, 3.0f, 2.0f, color);
                g.line({20.0f, 7.0f}, {34.0f, 7.0f}, 3.0f, color);
                g.arc({46.0f, 7.0f}, 5.0f, 0.0f, 4.71238898f, 2.0f, color);
                g.stroke_path(miter_path, color, miter);
                g.stroke_path(round_path, color, round);
                g.stroke_path(bevel_path, color, bevel);
            }}
    };

    ui::UI generic{
        ui::Canvas{72.0f, 36.0f,
            [color, miter_path, round_path, bevel_path, miter, round, bevel](ui::CanvasContext2D& g) {
                const ui::Brush brush{color};
                g.stroke_rounded_rect({2.0f, 2.0f, 14.0f, 10.0f}, 3.0f, 2.0f, brush);
                g.line({20.0f, 7.0f}, {34.0f, 7.0f}, 3.0f, brush);
                g.arc({46.0f, 7.0f}, 5.0f, 0.0f, 4.71238898f, 2.0f, brush);
                g.stroke_path(miter_path, brush, miter);
                g.stroke_path(round_path, brush, round);
                g.stroke_path(bevel_path, brush, bevel);
            }}
    };

    ui::HeadlessRenderer legacy_renderer{{72.0f, 36.0f}, 1.0f};
    ui::HeadlessRenderer generic_renderer{{72.0f, 36.0f}, 1.0f};
    NUI_CHECK(legacy_renderer.render(legacy));
    NUI_CHECK(generic_renderer.render(generic));
    NUI_CHECK(legacy_renderer.rgba_pixels() == generic_renderer.rgba_pixels());
}

void brush_stroke_path_uses_shared_painter_coordinates() {
    const ui::Brush brush{red_blue_gradient(48.0f)};
    ui::Path path;
    path.move_to({4.0f, 4.0f})
        .line_to({44.0f, 4.0f})
        .line_to({44.0f, 20.0f})
        .line_to({4.0f, 20.0f});
    const ui::StrokeStyle style{4.0f, ui::StrokeCap::Butt, ui::StrokeJoin::Miter, 4.0f};

    ui::UI tree{
        ui::Canvas{48.0f, 24.0f, [brush, path, style](ui::CanvasContext2D& g) {
            g.stroke_path(path, brush, style);
        }}
    };
    ui::HeadlessRenderer renderer{{48.0f, 24.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    NUI_CHECK(red_dominant(renderer.pixel(6, 4)));
    NUI_CHECK(blue_dominant(renderer.pixel(42, 4)));
    NUI_CHECK(blue_dominant(renderer.pixel(44, 12)));
    NUI_CHECK(red_dominant(renderer.pixel(6, 20)));
    NUI_CHECK(blue_dominant(renderer.pixel(42, 20)));
}

void brush_radial_line_and_arc_rendering() {
    const ui::Brush line_brush{ui::RadialGradient{
        {8.0f, 8.0f}, 6.0f,
        {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}};
    const ui::Brush arc_brush{ui::RadialGradient{
        {24.0f, 8.0f}, 6.0f,
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};

    ui::UI tree{
        ui::Canvas{32.0f, 16.0f, [line_brush, arc_brush](ui::CanvasContext2D& g) {
            g.line({2.0f, 8.0f}, {14.0f, 8.0f}, 3.0f, line_brush);
            g.arc({24.0f, 8.0f}, 5.0f, 0.0f, 6.28318531f, 3.0f, arc_brush);
        }}
    };
    ui::HeadlessRenderer renderer{{32.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto center = renderer.pixel(8, 8);
    const auto line_edge = renderer.pixel(13, 8);
    const auto arc_edge = renderer.pixel(29, 8);
    NUI_CHECK(center.r > 220 && center.g > 220 && center.b > 220);
    NUI_CHECK(line_edge.r < 100 && line_edge.g < 100 && line_edge.b < 100);
    NUI_CHECK(arc_edge.b > arc_edge.r + 40);
}

void brush_stroke_transform_opacity_and_blend() {
    const ui::Brush transformed{red_blue_gradient(16.0f)};
    const ui::Brush multiply{ui::LinearGradient{
        {28.0f, 0.0f}, {40.0f, 0.0f},
        {0.5f, 0.5f, 1.0f, 1.0f}, {0.5f, 0.5f, 1.0f, 1.0f}}};

    ui::UI tree{
        ui::Canvas{40.0f, 16.0f, [transformed, multiply](ui::CanvasContext2D& g) {
            g.save();
            g.translate(8.0f, 4.0f);
            g.line({0.0f, 4.0f}, {16.0f, 4.0f}, 4.0f, transformed,
                   ui::PaintOptions{0.5f, ui::BlendMode::SourceOver});
            g.restore();

            g.fill_rect({28.0f, 0.0f, 12.0f, 16.0f}, {0.8f, 0.5f, 0.25f, 1.0f});
            g.line({29.0f, 8.0f}, {39.0f, 8.0f}, 4.0f, multiply,
                   ui::PaintOptions{1.0f, ui::BlendMode::Multiply});
        }}
    };
    ui::HeadlessRenderer renderer{{40.0f, 16.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));

    const auto left = renderer.pixel(10, 8);
    const auto right = renderer.pixel(22, 8);
    NUI_CHECK(left.r > 90 && left.r < 140 && left.b < 45);
    NUI_CHECK(right.b > 90 && right.b < 140 && right.r < 45);
    NUI_CHECK(black(renderer.pixel(4, 8)));

    const auto multiplied = renderer.pixel(34, 8);
    NUI_CHECK(multiplied.r > 80 && multiplied.r < 125);
    NUI_CHECK(multiplied.g > 45 && multiplied.g < 90);
    NUI_CHECK(multiplied.b > 45 && multiplied.b < 90);
}

void suite() {
    two_stop_linear_gradient();
    multi_stop_linear_gradient();
    radial_gradient();
    opacity_and_blend_modes();
    brush_noexcept_and_allocation_contract();
    brush_gradient_owns_source_storage();
    brush_allocation_failure_is_transactional();
    brush_move_transfers_value_and_clears_source();
    brush_color_matches_legacy_color_path();
    brush_transform_clip_opacity_and_blend();
    brush_invalid_gradient_preserves_fallback();
    brush_copies_are_multi_ui_lifetime_safe();
    brush_strokes_match_color_and_preserve_style();
    brush_stroke_path_uses_shared_painter_coordinates();
    brush_radial_line_and_arc_rendering();
    brush_stroke_transform_opacity_and_blend();
}

} // namespace

int main() { return test::run("paint styles and brush", suite); }
