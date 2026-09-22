#include "test_support.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
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

namespace ui {

struct TreeTestAccess {
    static void make_cache_unavailable(Tree& tree) noexcept {
        tree.paint_cull_cache_.clear();
        tree.paint_cull_cache_dirty_ = false;
        tree.paint_cull_cached_viewport_ = tree.viewport_;
    }

    static void mark_cached_own_bounds_unknown(Tree& tree) noexcept {
        for (auto& entry : tree.paint_cull_cache_) entry.own_valid = false;
    }

    [[nodiscard]] static bool all_cached_own_bounds_unknown(const Tree& tree) noexcept {
        if (tree.paint_cull_cache_.empty()) return false;
        for (const auto& entry : tree.paint_cull_cache_) {
            if (entry.own_valid) return false;
        }
        return true;
    }

    [[nodiscard]] static bool cache_dirty(const Tree& tree) noexcept {
        return tree.paint_cull_cache_dirty_;
    }

    [[nodiscard]] static std::size_t exercise_warm_culling_decisions(
        const Tree& tree,
        Rect region) noexcept {
        std::size_t hits = 0;
        for (const auto& cached : tree.paint_cull_cache_) {
            const auto* entry = tree.paint_cull_cache_entry(cached.id);
            if (entry && entry->subtree_valid &&
                !intersect(entry->subtree_visual_bounds, region).empty()) {
                ++hits;
            }
        }
        return hits;
    }

    [[nodiscard]] static Rect first_child_bounds(const Tree& tree) noexcept {
        return tree.root_ && !tree.root_->children.empty()
            ? tree.root_->children.front()->bounds
            : Rect{};
    }

    [[nodiscard]] static Rect first_child_published_bounds(const Tree& tree) noexcept {
        return tree.root_ && !tree.root_->children.empty()
            ? tree.root_->children.front()->published_visual_bounds
            : Rect{};
    }
};

} // namespace ui

namespace {

struct PaintProbeState {
    int paints{};
    int unmounts{};
    int tag{};
    bool clips_children{};
    bool throw_on_paint{};
    bool throw_on_layout{};
    bool trigger_reentrant_mutation{};
    bool draws{};
    ui::Color color{};
    ui::Size measured{40.0f, 40.0f};
    ui::VisualOutset outset{};
    ui::ComponentAvailability availability{};
    std::vector<ui::Rect> child_bounds;
    std::vector<int>* order{};
    std::function<void()> on_paint;
    std::function<void()> invalidate_paint;
    std::function<void()> invalidate_availability;
};

class PaintProbeComponent final : public ui::Component {
public:
    explicit PaintProbeComponent(std::shared_ptr<PaintProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool clips_children() const noexcept override {
        return state_->clips_children;
    }

    [[nodiscard]] ui::VisualOutset visual_outset() const noexcept override {
        return state_->outset;
    }

    [[nodiscard]] ui::ComponentAvailability local_availability() const noexcept override {
        return state_->availability;
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return state_->measured;
    }

    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (state_->throw_on_layout) {
            throw std::runtime_error("layout probe failure");
        }
        for (std::size_t index = 0;
             index < placements.size() && index < state_->child_bounds.size();
             ++index) {
            const auto child = state_->child_bounds[index];
            placements[index].bounds = {
                bounds.x + child.x,
                bounds.y + child.y,
                child.w,
                child.h};
        }
    }

    void mount(ui::MountContext& context) override {
        state_->invalidate_paint = context.invalidator();
        state_->invalidate_availability = context.availability_invalidator();
    }

    void unmount(ui::LifecycleContext&) override {
        ++state_->unmounts;
    }

    void paint(ui::PaintContext& context) const override {
        ++state_->paints;
        if (state_->draws) {
            context.painter().fill_rounded_rect(context.bounds(), 0.0f, state_->color);
        }
        if (state_->order) state_->order->push_back(state_->tag);
        if (state_->on_paint) state_->on_paint();
        if (state_->throw_on_paint) throw std::runtime_error("paint probe failure");
    }

private:
    std::shared_ptr<PaintProbeState> state_;
};

ui::Spec probe_spec(std::shared_ptr<PaintProbeState> state,
                    std::vector<ui::Spec> children = {}) {
    return ui::Spec{
        [state = std::move(state)] {
            return std::make_unique<PaintProbeComponent>(state);
        },
        std::move(children)};
}

class PaintProbe {
public:
    explicit PaintProbe(std::shared_ptr<PaintProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        return probe_spec(std::move(state_));
    }

private:
    std::shared_ptr<PaintProbeState> state_;
};

void reset(const std::shared_ptr<PaintProbeState>& state) {
    state->paints = 0;
}

void paint_region(ui::Tree& tree,
                  SkCanvas& canvas,
                  test::MockPlatform& platform,
                  ui::Rect region) {
    ui::Painter painter{canvas};
    tree.paint_region(painter, platform, region);
}

void full_and_selective_order_contract() {
    std::vector<int> order;
    auto root = std::make_shared<PaintProbeState>();
    auto left = std::make_shared<PaintProbeState>();
    auto right = std::make_shared<PaintProbeState>();
    root->tag = 0;
    left->tag = 1;
    right->tag = 2;
    root->order = &order;
    left->order = &order;
    right->order = &order;
    root->child_bounds = {
        {0.0f, 0.0f, 60.0f, 100.0f},
        {140.0f, 0.0f, 60.0f, 100.0f}};

    ui::Tree tree{ui::compile(probe_spec(
        root,
        {probe_spec(left), probe_spec(right)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({200.0f, 100.0f});
    tree.paint(canvas, platform);
    NUI_CHECK((order == std::vector<int>{0, 1, 2}));

    order.clear();
    reset(root);
    reset(left);
    reset(right);
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 200.0f, 100.0f});
    NUI_CHECK((order == std::vector<int>{0, 1, 2}));

    order.clear();
    reset(root);
    reset(left);
    reset(right);
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 80.0f, 100.0f});
    NUI_CHECK((order == std::vector<int>{0, 1}));
    NUI_CHECK(right->paints == 0);
}

void invalid_region_falls_back_conservatively() {
    std::vector<int> order;
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->tag = 0;
    child->tag = 1;
    root->order = &order;
    child->order = &order;
    root->child_bounds = {{20.0f, 10.0f, 40.0f, 40.0f}};

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({100.0f, 80.0f});
    tree.paint(canvas, platform);

    order.clear();
    paint_region(
        tree,
        canvas,
        platform,
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 10.0f, 10.0f});
    NUI_CHECK((order == std::vector<int>{0, 1}));

    order.clear();
    paint_region(tree, canvas, platform, {10.0f, 10.0f, -1.0f, 10.0f});
    NUI_CHECK((order == std::vector<int>{0, 1}));

    order.clear();
    paint_region(tree, canvas, platform, {10.0f, 10.0f, 0.0f, 10.0f});
    NUI_CHECK(order.empty());

    order.clear();
    paint_region(tree, canvas, platform, {200.0f, 200.0f, 10.0f, 10.0f});
    NUI_CHECK(order.empty());
}

void descendant_outside_parent_bounds_contract() {
    auto root = std::make_shared<PaintProbeState>();
    auto parent = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->child_bounds = {{0.0f, 0.0f, 40.0f, 100.0f}};
    parent->child_bounds = {{140.0f, 0.0f, 20.0f, 20.0f}};

    ui::Tree tree{ui::compile(probe_spec(
        root,
        {probe_spec(parent, {probe_spec(child)})}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({200.0f, 100.0f});
    tree.paint(canvas, platform);
    reset(parent);
    reset(child);

    paint_region(tree, canvas, platform, {145.0f, 5.0f, 5.0f, 5.0f});
    NUI_CHECK(parent->paints == 0);
    NUI_CHECK(child->paints == 1);

    auto clipped_root = std::make_shared<PaintProbeState>();
    auto clipped_parent = std::make_shared<PaintProbeState>();
    auto clipped_child = std::make_shared<PaintProbeState>();
    clipped_root->child_bounds = {{0.0f, 0.0f, 40.0f, 100.0f}};
    clipped_parent->child_bounds = {{140.0f, 0.0f, 20.0f, 20.0f}};
    clipped_parent->clips_children = true;

    ui::Tree clipped_tree{ui::compile(probe_spec(
        clipped_root,
        {probe_spec(clipped_parent, {probe_spec(clipped_child)})}))};
    clipped_tree.mount();
    clipped_tree.layout({200.0f, 100.0f});
    clipped_tree.paint(canvas, platform);
    reset(clipped_parent);
    reset(clipped_child);

    paint_region(clipped_tree, canvas, platform, {145.0f, 5.0f, 5.0f, 5.0f});
    NUI_CHECK(clipped_parent->paints == 0);
    NUI_CHECK(clipped_child->paints == 0);
}

void visual_outset_publication_contract() {
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->child_bounds = {{100.0f, 20.0f, 20.0f, 20.0f}};

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.paint(canvas, platform);

    reset(child);
    paint_region(tree, canvas, platform, {75.0f, 20.0f, 10.0f, 20.0f});
    NUI_CHECK(child->paints == 0);

    child->outset.left = 30.0f;
    child->invalidate_paint();
    reset(child);
    paint_region(tree, canvas, platform, {75.0f, 20.0f, 10.0f, 20.0f});
    NUI_CHECK(child->paints == 1);

    child->outset.left = 0.0f;
    child->invalidate_paint();
    reset(child);
    paint_region(tree, canvas, platform, {75.0f, 20.0f, 10.0f, 20.0f});
    NUI_CHECK(child->paints == 0);
}

void availability_republishes_visual_bounds() {
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->child_bounds = {{120.0f, 20.0f, 30.0f, 30.0f}};
    child->availability.visibility = ui::VisibilityMode::Hidden;

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({180.0f, 80.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(child->paints == 0);

    child->availability.visibility = ui::VisibilityMode::Visible;
    child->invalidate_availability();
    reset(child);
    paint_region(tree, canvas, platform, {125.0f, 25.0f, 10.0f, 10.0f});
    NUI_CHECK(child->paints == 1);

    child->availability.visibility = ui::VisibilityMode::Hidden;
    child->invalidate_availability();
    reset(child);
    paint_region(tree, canvas, platform, {125.0f, 25.0f, 10.0f, 10.0f});
    NUI_CHECK(child->paints == 0);
}

void active_availability_republishes_visual_bounds() {
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->child_bounds = {{120.0f, 20.0f, 30.0f, 30.0f}};
    child->availability.visibility = ui::VisibilityMode::Hidden;

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({180.0f, 80.0f});
    tree.paint(canvas, platform);
    tree.activate_focus(platform);
    NUI_CHECK(child->paints == 0);
    NUI_CHECK(ui::TreeTestAccess::first_child_published_bounds(tree).empty());

    child->availability.visibility = ui::VisibilityMode::Visible;
    child->invalidate_availability();

    const auto published = ui::TreeTestAccess::first_child_published_bounds(tree);
    NUI_CHECK_NEAR(published.x, 120.0f, 0.0001f);
    NUI_CHECK_NEAR(published.y, 20.0f, 0.0001f);
    NUI_CHECK_NEAR(published.w, 30.0f, 0.0001f);
    NUI_CHECK_NEAR(published.h, 30.0f, 0.0001f);

    reset(child);
    paint_region(tree, canvas, platform, {125.0f, 25.0f, 10.0f, 10.0f});
    NUI_CHECK(child->paints == 1);

    tree.deactivate_focus(platform);
}

void unknown_cache_falls_back_conservatively() {
    auto root = std::make_shared<PaintProbeState>();
    auto left = std::make_shared<PaintProbeState>();
    auto right = std::make_shared<PaintProbeState>();
    root->child_bounds = {
        {0.0f, 0.0f, 40.0f, 60.0f},
        {80.0f, 0.0f, 40.0f, 60.0f}};

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(left), probe_spec(right)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({120.0f, 60.0f});
    tree.paint(canvas, platform);

    // Build a valid selective cache first, then simulate storage becoming
    // unavailable without changing retained publication state.
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    ui::TreeTestAccess::make_cache_unavailable(tree);

    reset(root);
    reset(left);
    reset(right);
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});

    // Missing cache entries are uncertainty: traversal becomes conservative
    // instead of incorrectly skipping the distant subtree.
    NUI_CHECK(root->paints == 1);
    NUI_CHECK(left->paints == 1);
    NUI_CHECK(right->paints == 1);
}

void pending_dirty_does_not_rebuild_cache() {
    auto root = std::make_shared<PaintProbeState>();
    auto left = std::make_shared<PaintProbeState>();
    auto right = std::make_shared<PaintProbeState>();
    root->child_bounds = {
        {0.0f, 0.0f, 40.0f, 60.0f},
        {80.0f, 0.0f, 40.0f, 60.0f}};

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(left), probe_spec(right)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({120.0f, 60.0f});
    tree.paint(canvas, platform);
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});

    ui::TreeTestAccess::mark_cached_own_bounds_unknown(tree);
    NUI_CHECK(ui::TreeTestAccess::all_cached_own_bounds_unknown(tree));
    NUI_CHECK(!ui::TreeTestAccess::cache_dirty(tree));

    // Damage awaiting consumption is independent from published visual state.
    tree.invalidate({0.0f, 0.0f, 4.0f, 4.0f});
    NUI_CHECK(tree.paint_dirty());
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});

    // A cache rebuild here would overwrite the injected unknown-own markers.
    NUI_CHECK(ui::TreeTestAccess::all_cached_own_bounds_unknown(tree));
    NUI_CHECK(!ui::TreeTestAccess::cache_dirty(tree));
}

void layout_publication_and_rollback_contract() {
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->child_bounds = {{10.0f, 10.0f, 20.0f, 20.0f}};

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.paint(canvas, platform);
    paint_region(tree, canvas, platform, {10.0f, 10.0f, 20.0f, 20.0f});

    const auto old_bounds = ui::TreeTestAccess::first_child_bounds(tree);
    const auto old_published = ui::TreeTestAccess::first_child_published_bounds(tree);
    NUI_CHECK_NEAR(old_bounds.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(old_published.x, 10.0f, 0.0001f);

    root->child_bounds[0].x = 100.0f;
    root->throw_on_layout = true;
    tree.invalidate_layout();

    bool threw = false;
    try {
        tree.layout({160.0f, 80.0f});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(ui::TreeTestAccess::cache_dirty(tree));

    const auto rolled_back = ui::TreeTestAccess::first_child_bounds(tree);
    const auto rolled_back_published =
        ui::TreeTestAccess::first_child_published_bounds(tree);
    NUI_CHECK_NEAR(rolled_back.x, old_bounds.x, 0.0001f);
    NUI_CHECK_NEAR(rolled_back_published.x, old_published.x, 0.0001f);

    root->throw_on_layout = false;
    tree.layout({160.0f, 80.0f});
    const auto committed = ui::TreeTestAccess::first_child_published_bounds(tree);
    NUI_CHECK_NEAR(committed.x, 100.0f, 0.0001f);

    reset(child);
    paint_region(tree, canvas, platform, {10.0f, 10.0f, 20.0f, 20.0f});
    NUI_CHECK(child->paints == 0);
    paint_region(tree, canvas, platform, {100.0f, 10.0f, 20.0f, 20.0f});
    NUI_CHECK(child->paints == 1);
}

void throwing_paint_restores_clip_state() {
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->clips_children = true;
    root->child_bounds = {{10.0f, 10.0f, 40.0f, 40.0f}};
    child->throw_on_paint = true;

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({100.0f, 80.0f});
    const int save_count = canvas.getSaveCount();

    bool threw = false;
    try {
        tree.paint(canvas, platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(canvas.getSaveCount() == save_count);
    NUI_CHECK(tree.paint_dirty());

    child->throw_on_paint = false;
    tree.paint(canvas, platform);
    NUI_CHECK(canvas.getSaveCount() == save_count);
    NUI_CHECK(!tree.paint_dirty());
}

void reentrant_structural_mutation_is_deferred() {
    ui::State<bool> visible{true};
    auto child = std::make_shared<PaintProbeState>();

    ui::Tree tree{ui::compile(ui::make_spec(ui::If{visible, PaintProbe{child}}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({120.0f, 60.0f});
    tree.paint(canvas, platform);
    NUI_CHECK(child->unmounts == 0);

    child->trigger_reentrant_mutation = true;
    child->on_paint = [&] {
        if (!child->trigger_reentrant_mutation) return;
        child->trigger_reentrant_mutation = false;
        visible.set(false);
        // A top-level layout re-entry would flush the queued structural mutation
        // and invalidate the active traversal's Node references without the paint
        // lifecycle guard.
        tree.layout({120.0f, 60.0f});
    };

    tree.paint(canvas, platform);
    NUI_CHECK(child->unmounts == 0);
    NUI_CHECK(tree.paint_dirty());

    tree.layout({120.0f, 60.0f});
    NUI_CHECK(child->unmounts == 1);
}

void widened_region_preserves_neighbor_contributor() {
    auto root = std::make_shared<PaintProbeState>();
    auto child = std::make_shared<PaintProbeState>();
    root->child_bounds = {{100.0f, 20.0f, 20.0f, 20.0f}};
    child->outset.left = 12.0f;

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(child)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.paint(canvas, platform);

    reset(child);
    paint_region(tree, canvas, platform, {80.0f, 20.0f, 5.0f, 20.0f});
    NUI_CHECK(child->paints == 0);

    reset(child);
    paint_region(tree, canvas, platform, {80.0f, 20.0f, 12.0f, 20.0f});
    NUI_CHECK(child->paints == 1);
}

void overlapping_translucent_pixel_parity() {
    constexpr int width = 180;
    constexpr int height = 100;
    const auto info = SkImageInfo::Make(
        width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    auto full_surface = SkSurfaces::Raster(info);
    auto partial_surface = SkSurfaces::Raster(info);
    NUI_CHECK(full_surface != nullptr);
    NUI_CHECK(partial_surface != nullptr);

    auto root = std::make_shared<PaintProbeState>();
    auto first = std::make_shared<PaintProbeState>();
    auto second = std::make_shared<PaintProbeState>();
    root->child_bounds = {
        {20.0f, 20.0f, 90.0f, 60.0f},
        {60.0f, 20.0f, 90.0f, 60.0f}};
    first->draws = true;
    first->color = {1.0f, 0.1f, 0.1f, 0.6f};
    second->draws = true;
    second->color = {0.1f, 0.2f, 1.0f, 0.5f};

    ui::Tree tree{ui::compile(probe_spec(
        root,
        {probe_spec(first), probe_spec(second)}))};
    test::MockPlatform platform;
    tree.mount();
    tree.layout({static_cast<float>(width), static_cast<float>(height)});

    auto* full_canvas = full_surface->getCanvas();
    auto* partial_canvas = partial_surface->getCanvas();
    NUI_CHECK(full_canvas != nullptr);
    NUI_CHECK(partial_canvas != nullptr);
    full_canvas->clear(SK_ColorTRANSPARENT);
    partial_canvas->clear(SK_ColorTRANSPARENT);

    tree.paint(*full_canvas, platform);
    {
        ui::Painter painter{*partial_canvas};
        tree.paint_region(painter, platform, {70.0f, 30.0f, 20.0f, 20.0f});
    }

    std::vector<std::uint32_t> full_pixels(static_cast<std::size_t>(width * height));
    std::vector<std::uint32_t> partial_pixels(static_cast<std::size_t>(width * height));
    NUI_CHECK(full_surface->readPixels(
        info, full_pixels.data(), static_cast<std::size_t>(width) * sizeof(std::uint32_t), 0, 0));
    NUI_CHECK(partial_surface->readPixels(
        info, partial_pixels.data(), static_cast<std::size_t>(width) * sizeof(std::uint32_t), 0, 0));

    for (int y = 30; y < 50; ++y) {
        for (int x = 70; x < 90; ++x) {
            const auto index = static_cast<std::size_t>(y * width + x);
            NUI_CHECK(full_pixels[index] == partial_pixels[index]);
        }
    }
}

void warm_culling_decisions_allocate_zero() {
    auto root = std::make_shared<PaintProbeState>();
    auto left = std::make_shared<PaintProbeState>();
    auto right = std::make_shared<PaintProbeState>();
    root->child_bounds = {
        {0.0f, 0.0f, 40.0f, 60.0f},
        {80.0f, 0.0f, 40.0f, 60.0f}};

    ui::Tree tree{ui::compile(probe_spec(root, {probe_spec(left), probe_spec(right)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({120.0f, 60.0f});
    tree.paint(canvas, platform);
    paint_region(tree, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    NUI_CHECK(!ui::TreeTestAccess::cache_dirty(tree));

    const auto before = allocation_probe::allocation_count;
    std::size_t hits = 0;
    for (int pass = 0; pass < 1024; ++pass) {
        hits += ui::TreeTestAccess::exercise_warm_culling_decisions(
            tree, {0.0f, 0.0f, 40.0f, 60.0f});
    }
    NUI_CHECK(hits != 0);
    NUI_CHECK(allocation_probe::allocation_count == before);
}

void two_tree_cache_isolation_contract() {
    auto root_a = std::make_shared<PaintProbeState>();
    auto left_a = std::make_shared<PaintProbeState>();
    auto right_a = std::make_shared<PaintProbeState>();
    root_a->child_bounds = {
        {0.0f, 0.0f, 40.0f, 60.0f},
        {80.0f, 0.0f, 40.0f, 60.0f}};

    auto root_b = std::make_shared<PaintProbeState>();
    auto left_b = std::make_shared<PaintProbeState>();
    auto right_b = std::make_shared<PaintProbeState>();
    root_b->child_bounds = root_a->child_bounds;

    ui::Tree a{ui::compile(probe_spec(root_a, {probe_spec(left_a), probe_spec(right_a)}))};
    ui::Tree b{ui::compile(probe_spec(root_b, {probe_spec(left_b), probe_spec(right_b)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    a.mount();
    b.mount();
    a.layout({120.0f, 60.0f});
    b.layout({120.0f, 60.0f});
    a.paint(canvas, platform);
    b.paint(canvas, platform);

    reset(left_a);
    reset(right_a);
    reset(left_b);
    reset(right_b);

    paint_region(a, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    paint_region(b, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    NUI_CHECK(left_a->paints == 1 && right_a->paints == 0);
    NUI_CHECK(left_b->paints == 1 && right_b->paints == 0);

    left_a->outset.right = 20.0f;
    left_a->invalidate_paint();

    reset(left_b);
    reset(right_b);
    paint_region(b, canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    NUI_CHECK(left_b->paints == 1 && right_b->paints == 0);
}

void suite() {
    full_and_selective_order_contract();
    invalid_region_falls_back_conservatively();
    descendant_outside_parent_bounds_contract();
    visual_outset_publication_contract();
    availability_republishes_visual_bounds();
    active_availability_republishes_visual_bounds();
    unknown_cache_falls_back_conservatively();
    pending_dirty_does_not_rebuild_cache();
    layout_publication_and_rollback_contract();
    throwing_paint_restores_clip_state();
    reentrant_structural_mutation_is_deferred();
    widened_region_preserves_neighbor_contributor();
    overlapping_translucent_pixel_parity();
    warm_culling_decisions_allocate_zero();
    two_tree_cache_isolation_contract();
}

} // namespace

int main() {
    return test::run("paint_culling", &suite);
}
