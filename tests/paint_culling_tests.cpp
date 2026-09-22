#include "test_support.hpp"

#include "include/core/SkCanvas.h"

#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

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

    void paint(ui::PaintContext&) const override {
        ++state_->paints;
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
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 200.0f, 100.0f});
    NUI_CHECK((order == std::vector<int>{0, 1, 2}));

    order.clear();
    reset(root);
    reset(left);
    reset(right);
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 80.0f, 100.0f});
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
    tree.paint_region(
        canvas,
        platform,
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 10.0f, 10.0f});
    NUI_CHECK((order == std::vector<int>{0, 1}));

    order.clear();
    tree.paint_region(canvas, platform, {10.0f, 10.0f, -1.0f, 10.0f});
    NUI_CHECK((order == std::vector<int>{0, 1}));

    order.clear();
    tree.paint_region(canvas, platform, {10.0f, 10.0f, 0.0f, 10.0f});
    NUI_CHECK(order.empty());

    order.clear();
    tree.paint_region(canvas, platform, {200.0f, 200.0f, 10.0f, 10.0f});
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

    tree.paint_region(canvas, platform, {145.0f, 5.0f, 5.0f, 5.0f});
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

    clipped_tree.paint_region(canvas, platform, {145.0f, 5.0f, 5.0f, 5.0f});
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
    tree.paint_region(canvas, platform, {75.0f, 20.0f, 10.0f, 20.0f});
    NUI_CHECK(child->paints == 0);

    child->outset.left = 30.0f;
    child->invalidate_paint();
    reset(child);
    tree.paint_region(canvas, platform, {75.0f, 20.0f, 10.0f, 20.0f});
    NUI_CHECK(child->paints == 1);

    child->outset.left = 0.0f;
    child->invalidate_paint();
    reset(child);
    tree.paint_region(canvas, platform, {75.0f, 20.0f, 10.0f, 20.0f});
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
    tree.paint_region(canvas, platform, {125.0f, 25.0f, 10.0f, 10.0f});
    NUI_CHECK(child->paints == 1);

    child->availability.visibility = ui::VisibilityMode::Hidden;
    child->invalidate_availability();
    reset(child);
    tree.paint_region(canvas, platform, {125.0f, 25.0f, 10.0f, 10.0f});
    NUI_CHECK(child->paints == 0);
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
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    ui::TreeTestAccess::make_cache_unavailable(tree);

    reset(root);
    reset(left);
    reset(right);
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});

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
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});

    ui::TreeTestAccess::mark_cached_own_bounds_unknown(tree);
    NUI_CHECK(ui::TreeTestAccess::all_cached_own_bounds_unknown(tree));
    NUI_CHECK(!ui::TreeTestAccess::cache_dirty(tree));

    // Damage awaiting consumption is independent from published visual state.
    tree.invalidate({0.0f, 0.0f, 4.0f, 4.0f});
    NUI_CHECK(tree.paint_dirty());
    tree.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});

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
    tree.paint_region(canvas, platform, {10.0f, 10.0f, 20.0f, 20.0f});

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
    tree.paint_region(canvas, platform, {10.0f, 10.0f, 20.0f, 20.0f});
    NUI_CHECK(child->paints == 0);
    tree.paint_region(canvas, platform, {100.0f, 10.0f, 20.0f, 20.0f});
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

    a.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    b.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    NUI_CHECK(left_a->paints == 1 && right_a->paints == 0);
    NUI_CHECK(left_b->paints == 1 && right_b->paints == 0);

    left_a->outset.right = 20.0f;
    left_a->invalidate_paint();

    reset(left_b);
    reset(right_b);
    b.paint_region(canvas, platform, {0.0f, 0.0f, 40.0f, 60.0f});
    NUI_CHECK(left_b->paints == 1 && right_b->paints == 0);
}

void suite() {
    full_and_selective_order_contract();
    invalid_region_falls_back_conservatively();
    descendant_outside_parent_bounds_contract();
    visual_outset_publication_contract();
    availability_republishes_visual_bounds();
    unknown_cache_falls_back_conservatively();
    pending_dirty_does_not_rebuild_cache();
    layout_publication_and_rollback_contract();
    throwing_paint_restores_clip_state();
    reentrant_structural_mutation_is_deferred();
    two_tree_cache_isolation_contract();
}

} // namespace

int main() {
    return test::run("paint_culling", &suite);
}
