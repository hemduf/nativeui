#include "test_support.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace allocation_probe {
std::size_t allocation_count{};
void* allocate(std::size_t size) {
    ++allocation_count;
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc{};
}
} // namespace allocation_probe

void* operator new(std::size_t size) { return allocation_probe::allocate(size); }
void* operator new[](std::size_t size) { return allocation_probe::allocate(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace ui::detail {

struct EffectTestAccess {
    static float sigma(const Effect& effect) noexcept { return effect.sigma_x_; }
    static Point offset(const Effect& effect) noexcept { return effect.offset_; }
    static Color color(const Effect& effect) noexcept { return effect.color_; }
};

struct PainterEffectFaultAccess {
    static void fail_before_materialization(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::BeforeEffectMaterialization;
    }
    static void clear(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::None;
    }
    static bool device_output_bounds(Painter& painter,
                                     Rect source,
                                     const Effect& effect,
                                     SkIRect& output) {
        if (!Painter::valid_clip_rect(source)) return false;
        const SkRect source_bounds = Painter::to_sk_rect(source);
        SkIRect device_source;
        if (!painter.effect_source_device_bounds(source_bounds, device_source)) return false;
        auto filter = Painter::materialize_effect_filter(effect);
        return filter &&
               painter.effect_filter_output_bounds(*filter, device_source, output);
    }
};

} // namespace ui::detail

namespace ui {

struct TreeTestAccess {
    static Node* find(Node& node, NodeId id) noexcept {
        if (node.id == id) return &node;
        for (auto& child : node.children) {
            if (auto* found = find(*child, id)) return found;
        }
        return nullptr;
    }

    [[nodiscard]] static Rect published_visual_bounds(Tree& tree, NodeId id) noexcept {
        if (!tree.root_) return {};
        if (auto* node = find(*tree.root_, id)) return node->published_visual_bounds;
        return {};
    }

    [[nodiscard]] static Rect layout_bounds(Tree& tree, NodeId id) noexcept {
        if (!tree.root_) return {};
        if (auto* node = find(*tree.root_, id)) return node->bounds;
        return {};
    }
};

} // namespace ui

namespace {

static_assert(noexcept(ui::Effect::drop_shadow(ui::Point{}, 1.0f, ui::Color{})));
static_assert(noexcept(ui::Effect::drop_shadow_only(ui::Point{}, 1.0f, ui::Color{})));
static_assert(noexcept(std::declval<const ui::Effect&>().visual_outset()));
static_assert(noexcept(ui::VisualOutset::uniform(1.0f)));
static_assert(std::is_nothrow_copy_constructible_v<ui::Effect>);
static_assert(std::is_nothrow_copy_assignable_v<ui::Effect>);
static_assert(std::is_nothrow_move_constructible_v<ui::Effect>);
static_assert(std::is_nothrow_move_assignable_v<ui::Effect>);
static_assert(std::is_nothrow_destructible_v<ui::Effect>);

class PainterProbeComponent final : public ui::Component {
public:
    explicit PainterProbeComponent(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 64.0f};
    }

    void paint(ui::PaintContext& context) const override { draw_(context.painter()); }

private:
    std::function<void(ui::Painter&)> draw_;
};

class PainterProbe {
public:
    explicit PainterProbe(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    ui::Spec spec() && {
        auto draw = std::move(draw_);
        return ui::Spec{
            [draw = std::move(draw)]() mutable {
                return std::make_unique<PainterProbeComponent>(std::move(draw));
            },
            {}};
    }

private:
    std::function<void(ui::Painter&)> draw_;
};

std::vector<std::uint8_t> render(std::function<void(ui::Painter&)> draw) {
    ui::UI tree{PainterProbe{std::move(draw)}};
    ui::HeadlessRenderer renderer{{80.0f, 64.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    return renderer.rgba_pixels();
}

ui::Rgba8 pixel(const std::vector<std::uint8_t>& pixels, int x, int y) {
    const auto offset = static_cast<std::size_t>((y * 80 + x) * 4);
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

sk_sp<SkSurface> make_surface() {
    return SkSurfaces::Raster(
        SkImageInfo::Make(80, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
}

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.0001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.0001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
}

void effect_value_contract() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    const auto shadow = ui::Effect::drop_shadow(
        {300.0f, -300.0f}, 80.0f, {nan, 2.0f, -1.0f, inf});
    NUI_CHECK(ui::detail::EffectTestAccess::sigma(shadow) == 64.0f);
    const auto offset = ui::detail::EffectTestAccess::offset(shadow);
    NUI_CHECK(offset.x == 256.0f && offset.y == -256.0f);
    const auto color = ui::detail::EffectTestAccess::color(shadow);
    NUI_CHECK(color.r == 0.0f && color.g == 1.0f &&
              color.b == 0.0f && color.a == 0.0f);

    const auto blur = ui::Effect::gaussian_blur(2.0f, 3.0f).visual_outset();
    NUI_CHECK(blur.left == 6.0f && blur.right == 6.0f);
    NUI_CHECK(blur.top == 9.0f && blur.bottom == 9.0f);

    const auto positive = ui::Effect::drop_shadow(
        {6.0f, -4.0f}, 8.0f, {0.0f, 0.0f, 0.0f, 0.5f}).visual_outset();
    NUI_CHECK(positive.left == 18.0f && positive.right == 30.0f);
    NUI_CHECK(positive.top == 28.0f && positive.bottom == 20.0f);

    const auto beyond = ui::Effect::drop_shadow_only(
        {40.0f, -40.0f}, 4.0f, {0.0f, 0.0f, 0.0f, 1.0f}).visual_outset();
    NUI_CHECK(beyond.left == 0.0f && beyond.right == 52.0f);
    NUI_CHECK(beyond.top == 52.0f && beyond.bottom == 0.0f);

    const auto transparent = ui::Effect::drop_shadow(
        {40.0f, 40.0f}, 8.0f, {1.0f, 0.0f, 0.0f, 0.0f}).visual_outset();
    NUI_CHECK(transparent.left == 0.0f && transparent.top == 0.0f &&
              transparent.right == 0.0f && transparent.bottom == 0.0f);

    NUI_CHECK(ui::VisualOutset::uniform(-4.0f).left == 0.0f);
    NUI_CHECK(ui::VisualOutset::uniform(nan).right == 0.0f);
    const auto uniform = ui::VisualOutset::uniform(7.0f);
    NUI_CHECK(uniform.left == 7.0f && uniform.top == 7.0f &&
              uniform.right == 7.0f && uniform.bottom == 7.0f);

    const auto before = allocation_probe::allocation_count;
    {
        const auto a = ui::Effect::drop_shadow(
            {4.0f, 5.0f}, 6.0f, {0.1f, 0.2f, 0.3f, 0.4f});
        const auto b = ui::Effect::drop_shadow_only(
            {-4.0f, 5.0f}, 6.0f, {0.4f, 0.3f, 0.2f, 0.1f});
        auto c = a;
        c = b;
        auto d = std::move(c);
        (void)d.visual_outset();
    }
    NUI_CHECK(allocation_probe::allocation_count == before);
}

void dirty_region_publication_is_allocation_free() {
    ui::DirtyRegion dirty;
    const ui::Rect clip{0.0f, 0.0f, 1000.0f, 100.0f};
    const auto before = allocation_probe::allocation_count;
    for (int i = 0; i < 9; ++i) {
        (void)dirty.add(
            {static_cast<float>(i * 20), 0.0f, 8.0f, 8.0f}, clip);
    }
    NUI_CHECK(allocation_probe::allocation_count == before);
    NUI_CHECK(dirty.rects().size() == 1);
}

void shadow_and_shadow_only_share_shadow_contribution() {
    const auto shadow = ui::Effect::drop_shadow(
        {6.0f, 0.0f}, 3.0f, {1.0f, 0.0f, 0.0f, 0.8f});
    const auto only = ui::Effect::drop_shadow_only(
        {6.0f, 0.0f}, 3.0f, {1.0f, 0.0f, 0.0f, 0.8f});

    const auto full = render([shadow](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        auto layer = painter.scoped_layer({24, 20, 16, 16}, shadow);
        painter.fill_rounded_rect({24, 20, 16, 16}, 0, {1, 1, 1, 1});
    });
    const auto shadow_only = render([only](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        auto layer = painter.scoped_layer({24, 20, 16, 16}, only);
        painter.fill_rounded_rect({24, 20, 16, 16}, 0, {1, 1, 1, 1});
    });

    const auto outside_full = pixel(full, 46, 28);
    const auto outside_only = pixel(shadow_only, 46, 28);
    NUI_CHECK(std::abs(static_cast<int>(outside_full.r) -
                       static_cast<int>(outside_only.r)) <= 1);
    NUI_CHECK(outside_full.r > outside_full.g + 8);

    const auto source_full = pixel(full, 28, 28);
    const auto source_only = pixel(shadow_only, 28, 28);
    NUI_CHECK(source_full.g > source_only.g + 80);
}

void zero_alpha_and_zero_sigma_contract() {
    const auto plain = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0.05f, 0.05f, 0.05f, 1});
        auto layer = painter.scoped_layer({24, 20, 16, 16});
        painter.fill_rounded_rect({24, 20, 16, 16}, 0, {0.9f, 0.8f, 0.2f, 1});
    });
    const auto zero_alpha = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0.05f, 0.05f, 0.05f, 1});
        auto layer = painter.scoped_layer(
            {24, 20, 16, 16},
            ui::Effect::drop_shadow({8, 4}, 4, {1, 0, 0, 0}));
        painter.fill_rounded_rect({24, 20, 16, 16}, 0, {0.9f, 0.8f, 0.2f, 1});
    });
    NUI_CHECK(plain == zero_alpha);

    const auto zero_sigma = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        auto layer = painter.scoped_layer(
            {20, 20, 12, 12},
            ui::Effect::drop_shadow_only({6, 0}, 0, {0, 1, 0, 1}));
        painter.fill_rounded_rect({20, 20, 12, 12}, 0, {1, 1, 1, 1});
    });
    NUI_CHECK(pixel(zero_sigma, 34, 26).g > 200);
    NUI_CHECK(pixel(zero_sigma, 22, 26).g < 4);
}

void zero_alpha_skips_materialization() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    ui::Painter painter{*surface->getCanvas()};

    ui::detail::PainterEffectFaultAccess::fail_before_materialization(painter);
    {
        auto layer = painter.scoped_layer(
            {12, 12, 24, 24},
            ui::Effect::drop_shadow({4, 4}, 3, {0, 0, 0, 0}));
        painter.fill_rounded_rect({16, 16, 4, 4}, 0, {1, 1, 1, 1});
    }

    bool seam_still_armed = false;
    try {
        auto layer = painter.scoped_layer(
            {12, 12, 24, 24},
            ui::Effect::drop_shadow({4, 4}, 3, {0, 0, 0, 1}));
        (void)layer;
    } catch (const std::bad_alloc&) {
        seam_still_armed = true;
    }
    NUI_CHECK(seam_still_armed);
    ui::detail::PainterEffectFaultAccess::clear(painter);
}

void transform_and_device_support_contract() {
    auto surface = make_surface();
    NUI_CHECK(surface && surface->getCanvas());
    ui::Painter painter{*surface->getCanvas()};
    painter.translate(2.25f, 1.5f);
    painter.scale(1.5f, 2.0f);

    SkIRect output;
    NUI_CHECK(ui::detail::PainterEffectFaultAccess::device_output_bounds(
        painter,
        {10.25f, 10.25f, 8.0f, 8.0f},
        ui::Effect::drop_shadow({4.0f, -2.0f}, 3.0f, {0, 0, 0, 1}),
        output));
    NUI_CHECK(output.width() > 12);
    NUI_CHECK(output.height() > 16);

    auto rotated_surface = make_surface();
    NUI_CHECK(rotated_surface && rotated_surface->getCanvas());
    ui::Painter rotated{*rotated_surface->getCanvas()};
    rotated.rotate(0.45f);
    SkIRect rotated_output;
    NUI_CHECK(ui::detail::PainterEffectFaultAccess::device_output_bounds(
        rotated,
        {16, 16, 10, 10},
        ui::Effect::drop_shadow_only({5, 3}, 4, {0, 0, 0, 1}),
        rotated_output));
    NUI_CHECK(!rotated_output.isEmpty());
}

void paint_options_apply_once() {
    const auto full = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        auto layer = painter.scoped_layer(
            {24, 20, 16, 16},
            ui::Effect::drop_shadow_only({6, 0}, 3, {1, 0, 0, 1}));
        painter.fill_rounded_rect({24, 20, 16, 16}, 0, {1, 1, 1, 1});
    });
    const auto half = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        ui::PaintOptions options;
        options.opacity = 0.5f;
        auto layer = painter.scoped_layer(
            {24, 20, 16, 16},
            ui::Effect::drop_shadow_only({6, 0}, 3, {1, 0, 0, 1}),
            options);
        painter.fill_rounded_rect({24, 20, 16, 16}, 0, {1, 1, 1, 1});
    });

    const int full_r = static_cast<int>(pixel(full, 46, 28).r);
    const int half_r = static_cast<int>(pixel(half, 46, 28).r);
    NUI_CHECK(full_r > 8);
    NUI_CHECK(std::abs(full_r - 2 * half_r) <= 6);
}

void blend_and_inner_transform_contract() {
    const auto source_over = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0.5f, 0.5f, 0.5f, 1});
        auto layer = painter.scoped_layer(
            {20, 16, 32, 32},
            ui::Effect::drop_shadow_only({6, 0}, 3, {1, 0, 0, 1}));
        painter.fill_rounded_rect({28, 24, 8, 8}, 0, {1, 1, 1, 1});
    });
    const auto multiply = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0.5f, 0.5f, 0.5f, 1});
        ui::PaintOptions options;
        options.blend = ui::BlendMode::Multiply;
        auto layer = painter.scoped_layer(
            {20, 16, 32, 32},
            ui::Effect::drop_shadow_only({6, 0}, 3, {1, 0, 0, 1}),
            options);
        painter.fill_rounded_rect({28, 24, 8, 8}, 0, {1, 1, 1, 1});
    });
    NUI_CHECK(pixel(source_over, 42, 28).r > pixel(multiply, 42, 28).r + 4);

    const auto direct = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        auto layer = painter.scoped_layer(
            {12, 12, 48, 40},
            ui::Effect::drop_shadow_only({6, 2}, 3, {0, 1, 0, 1}));
        painter.fill_rounded_rect({24, 24, 8, 8}, 0, {1, 1, 1, 1});
    });
    const auto translated_inside = render([](ui::Painter& painter) {
        painter.fill_rounded_rect({0, 0, 80, 64}, 0, {0, 0, 0, 1});
        auto layer = painter.scoped_layer(
            {12, 12, 48, 40},
            ui::Effect::drop_shadow_only({6, 2}, 3, {0, 1, 0, 1}));
        painter.translate(8, 0);
        painter.fill_rounded_rect({16, 24, 8, 8}, 0, {1, 1, 1, 1});
    });
    NUI_CHECK(direct == translated_inside);
}

struct OutsetProbeState {
    ui::VisualOutset outset{4, 4, 4, 4};
    ui::NodeId node_id{ui::kInvalidNodeId};
    std::function<void()> invalidate;
    std::function<void()> invalidate_layout;
};

class OutsetProbeComponent final : public ui::Component {
public:
    explicit OutsetProbeComponent(std::shared_ptr<OutsetProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {160, 80};
    }

    [[nodiscard]] ui::VisualOutset visual_outset() const noexcept override {
        return state_->outset;
    }

    void mount(ui::MountContext& context) override {
        state_->node_id = context.node_id();
        state_->invalidate = context.invalidator();
        state_->invalidate_layout = context.layout_invalidator();
    }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<OutsetProbeState> state_;
};

class OutsetProbe {
public:
    explicit OutsetProbe(std::shared_ptr<OutsetProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<OutsetProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<OutsetProbeState> state_;
};

void retained_visual_bounds_contract() {
    auto state = std::make_shared<OutsetProbeState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Padding{20, OutsetProbe{state}}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({200, 120});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    std::vector<ui::Rect> exposed;
    tree.set_invalidation_callback([&](ui::Rect rect) { exposed.push_back(rect); });

    state->outset = {10, 12, 14, 16};
    state->invalidate();
    NUI_CHECK(tree.paint_dirty() && !tree.layout_dirty());
    check_rect(tree.dirty_regions().front(), {10, 8, 184, 108});
    check_rect(exposed.back(), {10, 8, 184, 108});
    tree.paint(canvas, platform);

    exposed.clear();
    state->outset = {2, 2, 2, 2};
    state->invalidate();
    check_rect(tree.dirty_regions().front(), {10, 8, 184, 108});
    tree.paint(canvas, platform);

    state->outset = {
        std::numeric_limits<float>::infinity(),
        -2.0f,
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::quiet_NaN()};
    state->invalidate();
    check_rect(tree.dirty_regions().front(), {18, 18, 182, 84});
}

void callback_publication_is_transactional() {
    auto state = std::make_shared<OutsetProbeState>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::Padding{20, OutsetProbe{state}}))};
    test::MockPlatform platform;
    SkCanvas canvas;
    tree.mount();
    tree.layout({200, 120});
    tree.paint(canvas, platform);

    int callbacks = 0;
    tree.set_invalidation_callback([&](ui::Rect) {
        ++callbacks;
        if (callbacks == 1) {
            state->outset = {12, 12, 12, 12};
            state->invalidate();
        }
    });

    state->outset = {8, 8, 8, 8};
    state->invalidate();
    NUI_CHECK(callbacks == 2);
    check_rect(tree.dirty_regions().front(), {8, 8, 184, 104});
    tree.paint(canvas, platform);

    tree.set_invalidation_callback([](ui::Rect) {
        throw std::runtime_error("invalidation callback failure");
    });
    state->outset = {16, 16, 16, 16};
    bool threw = false;
    try {
        state->invalidate();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(tree.paint_dirty());

    int replayed = 0;
    tree.set_invalidation_callback([&](ui::Rect) { ++replayed; });
    NUI_CHECK(replayed == 1);
}

void invalidation_before_first_layout_stays_conservative() {
    auto state = std::make_shared<OutsetProbeState>();
    ui::Tree tree{ui::compile(ui::make_spec(OutsetProbe{state}))};
    tree.mount();
    state->outset = {20, 20, 20, 20};
    state->invalidate();
    NUI_CHECK(tree.layout_dirty());

    std::vector<ui::Rect> exposed;
    tree.set_invalidation_callback([&](ui::Rect rect) { exposed.push_back(rect); });
    tree.layout({200, 120});
    NUI_CHECK(!exposed.empty());
    // Before first successful layout the retained Tree still owns its initial
    // conservative viewport. It is valid for the pending region to be wider
    // than the first explicit layout, but it must cover that layout completely.
    NUI_CHECK(exposed.back().contains({0, 0, 200, 120}));
}


struct FixedPlacementComponent final : ui::Component {
    ui::Rect child_bounds{};

    explicit FixedPlacementComponent(ui::Rect bounds) : child_bounds(bounds) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {200, 120};
    }

    void layout_children(ui::Rect,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements[0].bounds = child_bounds;
    }

    void paint(ui::PaintContext&) const override {}
};

void zero_sized_layout_can_have_visual_outset() {
    auto state = std::make_shared<OutsetProbeState>();
    ui::Spec root{
        [] { return std::make_unique<FixedPlacementComponent>(ui::Rect{50, 30, 0, 40}); },
        {OutsetProbe{state}.spec()}};
    ui::Tree tree{ui::compile(std::move(root))};
    test::MockPlatform platform;
    SkCanvas canvas;
    tree.mount();
    tree.layout({200, 120});
    tree.paint(canvas, platform);

    state->outset = {12, 12, 12, 12};
    state->invalidate();
    NUI_CHECK(tree.paint_dirty());
    check_rect(tree.dirty_regions().front(), {38, 18, 24, 64});
}

struct LayoutFaultState {
    float first_x{20.0f};
    bool throw_second{};
};

class TwoChildLayoutComponent final : public ui::Component {
public:
    explicit TwoChildLayoutComponent(std::shared_ptr<LayoutFaultState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {200, 120};
    }

    void layout_children(ui::Rect,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        placements[0].bounds = {state_->first_x, 20, 60, 40};
        placements[1].bounds = {100, 20, 60, 40};
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<LayoutFaultState> state_;
};

class ThrowingLayoutLeaf final : public ui::Component {
public:
    explicit ThrowingLayoutLeaf(std::shared_ptr<LayoutFaultState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {60, 40};
    }

    void layout_children(ui::Rect,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>&) const override {
        if (state_->throw_second) throw std::runtime_error("layout failure");
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<LayoutFaultState> state_;
};

void failed_layout_does_not_publish_partial_visual_bounds() {
    auto outset = std::make_shared<OutsetProbeState>();
    auto layout_state = std::make_shared<LayoutFaultState>();
    ui::Spec root{
        [layout_state] { return std::make_unique<TwoChildLayoutComponent>(layout_state); },
        {
            OutsetProbe{outset}.spec(),
            ui::Spec{
                [layout_state] { return std::make_unique<ThrowingLayoutLeaf>(layout_state); },
                {}}
        }};

    ui::Tree tree{ui::compile(std::move(root))};
    test::MockPlatform platform;
    SkCanvas canvas;
    tree.mount();
    tree.layout({200, 120});
    tree.paint(canvas, platform);

    check_rect(ui::TreeTestAccess::published_visual_bounds(tree, outset->node_id),
               {16, 16, 68, 48});
    check_rect(ui::TreeTestAccess::layout_bounds(tree, outset->node_id),
               {20, 20, 60, 40});

    layout_state->first_x = 60.0f;
    layout_state->throw_second = true;
    outset->invalidate_layout();

    bool threw = false;
    try {
        tree.layout({200, 120});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    check_rect(ui::TreeTestAccess::published_visual_bounds(tree, outset->node_id),
               {16, 16, 68, 48});
    check_rect(ui::TreeTestAccess::layout_bounds(tree, outset->node_id),
               {20, 20, 60, 40});

    layout_state->throw_second = false;
    tree.layout({200, 120});
    check_rect(ui::TreeTestAccess::published_visual_bounds(tree, outset->node_id),
               {56, 16, 68, 48});
}

void independent_tree_visual_state_isolation() {
    auto first = std::make_shared<OutsetProbeState>();
    auto second = std::make_shared<OutsetProbeState>();
    ui::Tree a{ui::compile(ui::make_spec(ui::Padding{20, OutsetProbe{first}}))};
    ui::Tree b{ui::compile(ui::make_spec(ui::Padding{20, OutsetProbe{second}}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    a.mount();
    b.mount();
    a.layout({200, 120});
    b.layout({200, 120});
    a.paint(canvas, platform);
    b.paint(canvas, platform);

    first->outset = {18, 18, 18, 18};
    first->invalidate();
    NUI_CHECK(a.paint_dirty());
    NUI_CHECK(!b.paint_dirty());

    a.unmount();
    second->outset = {10, 10, 10, 10};
    second->invalidate();
    NUI_CHECK(b.paint_dirty());
}

void structural_removal_is_invalidated_before_teardown() {
    ui::State<bool> visible{true};
    auto state = std::make_shared<OutsetProbeState>();
    ui::UI tree{ui::If{visible, OutsetProbe{state}}};
    test::MockPlatform platform;
    SkCanvas canvas;
    tree.resize({200, 120});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());

    std::vector<ui::Rect> exposed;
    tree.set_invalidation_callback([&](ui::Rect rect) { exposed.push_back(rect); });

    // Dynamic observers enqueue a full conservative repaint before structural
    // reconciliation can unmount/destroy the old subtree.
    visible.set(false);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!exposed.empty());
    NUI_CHECK(exposed.back().contains({0, 0, 200, 120}));

    tree.resize({200, 120});
    tree.paint(canvas, platform);
    NUI_CHECK(!tree.dirty());
}

void suite() {
    effect_value_contract();
    dirty_region_publication_is_allocation_free();
    shadow_and_shadow_only_share_shadow_contribution();
    zero_alpha_and_zero_sigma_contract();
    zero_alpha_skips_materialization();
    transform_and_device_support_contract();
    paint_options_apply_once();
    blend_and_inner_transform_contract();
    retained_visual_bounds_contract();
    zero_sized_layout_can_have_visual_outset();
    failed_layout_does_not_publish_partial_visual_bounds();
    independent_tree_visual_state_isolation();
    structural_removal_is_invalidated_before_teardown();
    callback_publication_is_transactional();
    invalidation_before_first_layout_stays_conservative();
}

} // namespace

int main() { return test::run("t078_drop_shadow", &suite); }
