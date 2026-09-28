#include "../test_support.hpp"

#include <nativeui/detail/raster_cache_access.hpp>
#include "include/core/SkCanvas.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ui {
struct TreeTestAccess {
    static std::size_t raster_records(const Tree& tree) noexcept {
        return tree.raster_cache_epochs_.size();
    }
    static detail::RasterCacheEpoch::Token lifetime(Tree& tree, NodeId id) {
        return tree.raster_cache_epochs_.at(id).lifetime_token();
    }
    static Rect bounds(Tree& tree, NodeId id) {
        return tree.retained_invalidation_node(id)->bounds;
    }
    static void rebind_theme(Tree& tree) { tree.bind_theme(*tree.root_); }
};
}

namespace {
using Access = ui::detail::RasterCacheAccess;

struct ProbeState {
    ui::NodeId id{};
    ui::ComponentAvailability availability{};
    ui::VisualOutset outset{};
    std::vector<ui::Rect> placements;
    std::function<void()> invalidate;
    std::function<void()> invalidate_layout;
    std::function<void()> invalidate_availability;
    std::function<void()> on_paint;
    std::function<void()> on_layout;
    std::function<void()> on_unmount;
    std::function<void(ui::NodeId)> on_mount;
    int paints{};
};

class ProbeComponent final : public ui::Component {
public:
    explicit ProbeComponent(std::shared_ptr<ProbeState> state) : state_(std::move(state)) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {200.0f, 100.0f};
    }
    ui::ComponentAvailability local_availability() const noexcept override {
        return state_->availability;
    }
    ui::VisualOutset visual_outset() const noexcept override { return state_->outset; }
    void mount(ui::MountContext& context) override {
        state_->id = context.node_id();
        state_->invalidate = context.invalidator();
        state_->invalidate_layout = context.layout_invalidator();
        state_->invalidate_availability = context.availability_invalidator();
        if (state_->on_mount) state_->on_mount(state_->id);
    }
    void unmount(ui::LifecycleContext&) override {
        if (state_->on_unmount) state_->on_unmount();
    }
    void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>& children,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (state_->on_layout) state_->on_layout();
        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto local = i < state_->placements.size() ? state_->placements[i]
                : ui::Rect{0, 0, bounds.w, bounds.h};
            placements[i].bounds = {bounds.x + local.x, bounds.y + local.y, local.w, local.h};
        }
    }
    void paint(ui::PaintContext&) const override {
        ++state_->paints;
        if (state_->on_paint) state_->on_paint();
    }
private:
    std::shared_ptr<ProbeState> state_;
};

ui::Spec probe(std::shared_ptr<ProbeState> state, std::vector<ui::Spec> children = {}) {
    return {[state = std::move(state)] { return std::make_unique<ProbeComponent>(state); },
            std::move(children)};
}

Access::Token publish(ui::Tree& tree, ui::NodeId id) {
    const auto token = Access::capture(tree, id);
    NUI_CHECK(!token.expired());
    NUI_CHECK(Access::commit(tree, id, token));
    NUI_CHECK(Access::reusable(tree, id, token));
    return token;
}

struct Fixture {
    std::shared_ptr<ProbeState> root = std::make_shared<ProbeState>();
    std::shared_ptr<ProbeState> outer = std::make_shared<ProbeState>();
    std::shared_ptr<ProbeState> inner = std::make_shared<ProbeState>();
    std::shared_ptr<ProbeState> leaf = std::make_shared<ProbeState>();
    std::shared_ptr<ProbeState> sibling = std::make_shared<ProbeState>();
    test::MockPlatform platform;
    SkCanvas canvas;
    ui::Tree tree;

    Fixture() : tree(ui::compile(probe(root, {
        probe(outer, {probe(inner, {probe(leaf)})}), probe(sibling)}))) {
        root->placements = {{10, 10, 100, 60}, {150, 0, 40, 30}};
        tree.mount();
        tree.layout({200, 100});
        settle();
        NUI_CHECK(Access::register_boundary(tree, outer->id));
        NUI_CHECK(Access::register_boundary(tree, inner->id));
        NUI_CHECK(Access::register_boundary(tree, sibling->id));
    }
    void settle() { tree.paint(canvas, platform); }
};

void invalidation_ancestry_and_repaint() {
    Fixture f;
    const auto outer = publish(f.tree, f.outer->id);
    const auto inner = publish(f.tree, f.inner->id);
    const auto sibling = publish(f.tree, f.sibling->id);
    f.tree.invalidate();
    f.root->invalidate();
    f.settle();
    NUI_CHECK(Access::reusable(f.tree, f.outer->id, outer));
    NUI_CHECK(Access::reusable(f.tree, f.inner->id, inner));
    f.leaf->invalidate();
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, outer));
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, inner));
    NUI_CHECK(Access::reusable(f.tree, f.sibling->id, sibling));
    NUI_CHECK(!Access::commit(f.tree, f.outer->id, outer));
    const auto next_outer = publish(f.tree, f.outer->id);
    const auto next_inner = publish(f.tree, f.inner->id);
    f.outer->invalidate();
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, next_outer));
    NUI_CHECK(Access::reusable(f.tree, f.inner->id, next_inner));
}

void placement_size_and_relative_motion() {
    Fixture f;
    const auto outer = publish(f.tree, f.outer->id);
    const auto inner = publish(f.tree, f.inner->id);
    const auto sibling = publish(f.tree, f.sibling->id);
    f.root->placements[0].x += 20;
    f.root->invalidate_layout();
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, outer)); // Layout is provisional.
    f.tree.layout({200, 100});
    NUI_CHECK(Access::reusable(f.tree, f.outer->id, outer));
    NUI_CHECK(Access::reusable(f.tree, f.inner->id, inner));
    NUI_CHECK(Access::reusable(f.tree, f.sibling->id, sibling));
    NUI_CHECK_NEAR(ui::TreeTestAccess::bounds(f.tree, f.outer->id).x, 30.0f, 0.0f);
    bool covers_old = false;
    bool covers_new = false;
    for (const auto damage : f.tree.dirty_regions()) {
        covers_old = covers_old || damage.contains({10, 10});
        covers_new = covers_new || damage.contains({30, 10});
    }
    NUI_CHECK(covers_old && covers_new);

    f.outer->placements = {{5, 0, 100, 60}};
    f.root->invalidate_layout();
    f.tree.layout({200, 100});
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, outer));
    NUI_CHECK(Access::reusable(f.tree, f.inner->id, inner));
    const auto moved_outer = publish(f.tree, f.outer->id);
    f.outer->placements.clear();
    f.root->placements[0].w = 90;
    f.root->invalidate_layout();
    f.tree.layout({200, 100});
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, moved_outer));
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, inner));
    NUI_CHECK(Access::reusable(f.tree, f.sibling->id, sibling));
}

void reentrant_paint_and_throwing_redraw() {
    Fixture f;
    const auto token = publish(f.tree, f.outer->id);
    const auto candidate = Access::capture(f.tree, f.outer->id);
    f.leaf->on_paint = [&f] { f.leaf->invalidate(); };
    f.tree.invalidate();
    f.settle();
    f.leaf->on_paint = {};
    NUI_CHECK(!Access::commit(f.tree, f.outer->id, candidate));
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, token));
    f.settle();
    const auto recovered = publish(f.tree, f.outer->id);
    f.tree.set_invalidation_callback(std::function<void(ui::Rect)>{[](ui::Rect) {
        throw std::runtime_error("injected redraw failure");
    }});
    bool caught = false;
    try { f.leaf->invalidate(); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught);
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, recovered));
    NUI_CHECK(f.tree.dirty());
    f.tree.set_invalidation_callback(std::function<void(ui::Rect)>{});
    f.settle();
    (void)publish(f.tree, f.outer->id);
}

void failed_layout_and_recovery() {
    Fixture f;
    const auto old = publish(f.tree, f.outer->id);
    const auto before = ui::TreeTestAccess::bounds(f.tree, f.inner->id);
    f.root->placements[0].w = 80;
    f.inner->on_layout = [&f, old] {
        NUI_CHECK(Access::capture(f.tree, f.outer->id).expired());
        NUI_CHECK(!Access::commit(f.tree, f.outer->id, old));
        f.leaf->invalidate();
        throw std::runtime_error("injected layout failure");
    };
    f.root->invalidate_layout();
    bool caught = false;
    try { f.tree.layout({200, 100}); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught);
    NUI_CHECK(f.tree.layout_dirty());
    NUI_CHECK_NEAR(ui::TreeTestAccess::bounds(f.tree, f.inner->id).w, before.w, 0.0f);
    NUI_CHECK(!Access::commit(f.tree, f.outer->id, old));
    f.inner->on_layout = {};
    f.tree.layout({200, 100});
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, old));
    (void)publish(f.tree, f.outer->id);
}

void dependency_callback_and_equal_state() {
    Fixture f;
    ui::State<int> revision{0};
    const auto invalidate = Access::invalidator(f.tree, f.inner->id);
    const auto subscription = revision.observe([invalidate](const int&) { invalidate(); });
    const auto token = publish(f.tree, f.inner->id);
    const auto outer = publish(f.tree, f.outer->id);
    revision.set(0);
    NUI_CHECK(Access::reusable(f.tree, f.inner->id, token));
    revision.set(1);
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, token));
    NUI_CHECK(!Access::reusable(f.tree, f.outer->id, outer));
    NUI_CHECK(f.tree.dirty());
    (void)subscription;
}

void availability_outset_and_theme() {
    Fixture f;
    auto token = publish(f.tree, f.inner->id);
    f.leaf->availability.enabled = false;
    f.leaf->invalidate_availability();
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, token));
    f.settle();
    token = publish(f.tree, f.inner->id);
    f.leaf->outset = ui::VisualOutset::uniform(4);
    f.leaf->invalidate();
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, token));
    token = publish(f.tree, f.inner->id);
    ui::TreeTestAccess::rebind_theme(f.tree);
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, token));
}

void unmount_remount_and_survivor() {
    Fixture a;
    Fixture b;
    const auto old = publish(a.tree, a.inner->id);
    const auto survivor = publish(b.tree, b.inner->id);
    const auto stale_callback = Access::invalidator(a.tree, a.inner->id);
    a.inner->on_unmount = [&a, old] {
        NUI_CHECK(old.expired());
        NUI_CHECK(!Access::commit(a.tree, a.inner->id, old));
    };
    a.tree.unmount();
    NUI_CHECK(old.expired());
    NUI_CHECK(ui::TreeTestAccess::raster_records(a.tree) == 0);
    a.inner->on_unmount = {};
    a.tree.mount();
    a.tree.layout({200, 100});
    NUI_CHECK(Access::register_boundary(a.tree, a.inner->id));
    const auto next = publish(a.tree, a.inner->id);
    stale_callback();
    NUI_CHECK(Access::reusable(a.tree, a.inner->id, next));
    NUI_CHECK(!Access::commit(a.tree, a.inner->id, old));
    NUI_CHECK(Access::reusable(b.tree, b.inner->id, survivor));

    std::function<void()> after_destruction;
    Access::Token retired;
    {
        Fixture temporary;
        retired = publish(temporary.tree, temporary.inner->id);
        after_destruction = Access::invalidator(temporary.tree, temporary.inner->id);
    }
    NUI_CHECK(retired.expired());
    after_destruction();
    NUI_CHECK(Access::reusable(b.tree, b.inner->id, survivor));
}

void activation_and_deactivation_revoke_content() {
    Fixture f;
    const auto inactive = publish(f.tree, f.inner->id);
    f.tree.activate_focus(f.platform);
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, inactive));
    const auto active = publish(f.tree, f.inner->id);
    f.tree.deactivate_focus(f.platform);
    NUI_CHECK(!Access::reusable(f.tree, f.inner->id, active));
    (void)publish(f.tree, f.inner->id);
}

void failed_mount_retires_registration() {
    auto state = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(probe(state))};
    Access::Token attempted;
    state->on_mount = [&tree, &attempted](ui::NodeId id) {
        NUI_CHECK(Access::register_boundary(tree, id));
        attempted = ui::TreeTestAccess::lifetime(tree, id);
        throw std::runtime_error("injected mount failure");
    };
    bool caught = false;
    try { tree.mount(); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught);
    NUI_CHECK(attempted.expired());
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 0);
    state->on_mount = {};
    tree.mount();
    tree.layout({200, 100});
    NUI_CHECK(Access::register_boundary(tree, state->id));
    (void)publish(tree, state->id);
}

void dynamic_remove_reinsert() {
    auto outer = std::make_shared<ProbeState>();
    auto child = std::make_shared<ProbeState>();
    ui::State<bool> visible{true};
    auto conditional = ui::make_spec(ui::If{visible, probe(child)});
    ui::Tree tree{ui::compile(probe(outer, {std::move(conditional)}))};
    test::MockPlatform platform;
    SkCanvas canvas;
    tree.mount();
    tree.layout({200, 100});
    tree.paint(canvas, platform);
    NUI_CHECK(Access::register_boundary(tree, outer->id));
    NUI_CHECK(Access::register_boundary(tree, child->id));
    const auto old_id = child->id;
    const auto old_child = publish(tree, old_id);
    const auto old_outer = publish(tree, outer->id);
    const auto stale_callback = Access::invalidator(tree, old_id);
    visible.set(false);
    NUI_CHECK(!Access::reusable(tree, outer->id, old_outer));
    tree.paint(canvas, platform);
    NUI_CHECK(old_child.expired());
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 1);
    visible.set(true);
    tree.paint(canvas, platform);
    NUI_CHECK(child->id != old_id);
    NUI_CHECK(Access::register_boundary(tree, child->id));
    const auto next = publish(tree, child->id);
    stale_callback();
    NUI_CHECK(Access::reusable(tree, child->id, next));
    NUI_CHECK(!Access::register_boundary(tree, old_id));
    NUI_CHECK(!Access::register_boundary(tree, ui::kInvalidNodeId));
}
void dynamic_reconcile_failure_retires_boundary_identity() {
    auto outer = std::make_shared<ProbeState>();
    auto child = std::make_shared<ProbeState>();
    ui::State<bool> visible{false};
    auto conditional = ui::make_spec(ui::If{visible, probe(child)});
    ui::Tree tree{ui::compile(probe(outer, {std::move(conditional)}))};
    test::MockPlatform platform;
    SkCanvas canvas;

    tree.mount();
    tree.layout({200, 100});
    tree.paint(canvas, platform);
    NUI_CHECK(Access::register_boundary(tree, outer->id));
    const auto outer_before = publish(tree, outer->id);

    Access::Token failed_lifetime;
    ui::NodeId failed_id = ui::kInvalidNodeId;
    child->on_mount = [&](ui::NodeId id) {
        failed_id = id;
        NUI_CHECK(Access::register_boundary(tree, id));
        failed_lifetime = ui::TreeTestAccess::lifetime(tree, id);
        throw std::runtime_error("injected dynamic mount failure");
    };

    visible.set(true);
    NUI_CHECK(!Access::reusable(tree, outer->id, outer_before));

    bool caught = false;
    try {
        tree.paint(canvas, platform);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    NUI_CHECK(caught);
    NUI_CHECK(failed_id != ui::kInvalidNodeId);
    NUI_CHECK(failed_lifetime.expired());
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 1);
    NUI_CHECK(!Access::commit(tree, failed_id, failed_lifetime));

    child->on_mount = {};
    tree.paint(canvas, platform);
    NUI_CHECK(child->id != ui::kInvalidNodeId);
    NUI_CHECK(child->id != failed_id);
    NUI_CHECK(Access::register_boundary(tree, child->id));
    (void)publish(tree, child->id);
    (void)publish(tree, outer->id);
}

}

int main() {
    try {
        invalidation_ancestry_and_repaint();
        placement_size_and_relative_motion();
        reentrant_paint_and_throwing_redraw();
        failed_layout_and_recovery();
        dependency_callback_and_equal_state();
        availability_outset_and_theme();
        unmount_remount_and_survivor();
        activation_and_deactivation_revoke_content();
        failed_mount_retires_registration();
        dynamic_remove_reinsert();
        dynamic_reconcile_failure_retires_boundary_identity();
        std::cout << "11 retained raster cache boundary contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
