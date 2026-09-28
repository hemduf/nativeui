#include "test_support.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ui {

struct TreeTestAccess {
    struct BoundaryState {
        NodeId node_id{kInvalidNodeId};
        std::uint64_t lifetime{};
        std::uint64_t generation{};
        std::uint64_t serial{};
        Size local_size{};
        bool stale{};
    };

    [[nodiscard]] static Node* find_mutable(Node& node, NodeId id) noexcept {
        if (node.id == id) return &node;
        for (auto& child : node.children) {
            if (auto* found = find_mutable(*child, id)) return found;
        }
        return nullptr;
    }

    [[nodiscard]] static Node* node(Tree& tree, NodeId id) noexcept {
        return tree.root_ ? find_mutable(*tree.root_, id) : nullptr;
    }

    [[nodiscard]] static NodeId root_id(const Tree& tree) noexcept {
        return tree.root_ ? tree.root_->id : kInvalidNodeId;
    }

    [[nodiscard]] static NodeId child_id(
        const Tree& tree, std::size_t index = 0) noexcept {
        return tree.root_ && index < tree.root_->children.size()
            ? tree.root_->children[index]->id
            : kInvalidNodeId;
    }

    [[nodiscard]] static bool register_boundary(Tree& tree, NodeId id) {
        auto* target = node(tree, id);
        return target && tree.register_raster_cache_boundary(*target);
    }

    [[nodiscard]] static bool registered(const Tree& tree, NodeId id) noexcept {
        return tree.raster_cache_boundary_record(id) != nullptr;
    }

    [[nodiscard]] static std::optional<BoundaryState> state(
        const Tree& tree, NodeId id) noexcept {
        const auto snapshot = tree.raster_cache_boundary_snapshot(id);
        if (!snapshot) return std::nullopt;
        return BoundaryState{
            snapshot->node_id,
            snapshot->lifetime_identity,
            snapshot->content_generation,
            snapshot->invalidation_serial,
            snapshot->local_size,
            snapshot->content_stale};
    }

    [[nodiscard]] static bool commit(Tree& tree, NodeId id) noexcept {
        const auto snapshot = tree.raster_cache_boundary_snapshot(id);
        return snapshot && tree.try_commit_raster_cache_boundary(*snapshot);
    }

    [[nodiscard]] static bool stale_during_refresh_rejects_commit(
        Tree& tree,
        NodeId id,
        const std::function<void()>& invalidate) {
        const auto snapshot = tree.raster_cache_boundary_snapshot(id);
        if (!snapshot || !snapshot->content_stale) return false;
        invalidate();
        return !tree.try_commit_raster_cache_boundary(*snapshot) &&
            tree.raster_cache_boundary_record(id) &&
            tree.raster_cache_boundary_record(id)->content_stale;
    }

    [[nodiscard]] static bool remount_rejects_previous_lifetime(
        Tree& tree,
        NodeId id) {
        const auto old_snapshot = tree.raster_cache_boundary_snapshot(id);
        if (!old_snapshot) return false;

        tree.unmount();
        if (tree.raster_cache_boundary_record(id)) return false;
        tree.mount();

        auto* remounted = node(tree, id);
        if (!remounted || !tree.register_raster_cache_boundary(*remounted)) return false;
        const auto next = tree.raster_cache_boundary_snapshot(id);
        if (!next || next->lifetime_identity == old_snapshot->lifetime_identity) {
            return false;
        }
        return !tree.try_commit_raster_cache_boundary(*old_snapshot);
    }

    static void force_serial_exhaustion(Tree& tree, NodeId id) noexcept {
        auto* record = tree.raster_cache_boundary_record(id);
        if (record) {
            record->invalidation_serial = std::numeric_limits<std::uint64_t>::max();
        }
    }

    [[nodiscard]] static Rect bounds(const Tree& tree, NodeId id) noexcept {
        const auto* target = tree.root_ ? Tree::find_node(*tree.root_, id) : nullptr;
        return target ? target->bounds : Rect{};
    }
};

} // namespace ui

namespace {

struct ProbeState {
    ui::NodeId node_id{ui::kInvalidNodeId};
    std::function<void()> invalidate_paint;
    std::function<void()> invalidate_layout;
};

class ProbeComponent final : public ui::Component {
public:
    explicit ProbeComponent(
        std::shared_ptr<ProbeState> state,
        ui::Size size = {40.0f, 30.0f})
        : state_(std::move(state)), size_(size) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return size_;
    }

    void mount(ui::MountContext& context) override {
        state_->node_id = context.node_id();
        state_->invalidate_paint = context.invalidator();
        state_->invalidate_layout = context.layout_invalidator();
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ProbeState> state_;
    ui::Size size_;
};

class Probe {
public:
    explicit Probe(
        std::shared_ptr<ProbeState> state,
        ui::Size size = {40.0f, 30.0f})
        : state_(std::move(state)), size_(size) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        const auto size = size_;
        return ui::Spec{
            [state = std::move(state), size] {
                return std::make_unique<ProbeComponent>(state, size);
            },
            {}};
    }

private:
    std::shared_ptr<ProbeState> state_;
    ui::Size size_;
};

struct PlacementState {
    float x{};
    float width{40.0f};
    bool throw_on_layout{};
};

class PlacementComponent final : public ui::Component {
public:
    explicit PlacementComponent(std::shared_ptr<PlacementState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 60.0f};
    }

    void layout_children(
        ui::Rect bounds,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (state_->throw_on_layout) throw std::runtime_error("layout failure");
        if (!placements.empty()) {
            placements[0].bounds =
                {bounds.x + state_->x, bounds.y + 5.0f, state_->width, 30.0f};
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<PlacementState> state_;
};

class Placement {
public:
    Placement(std::shared_ptr<PlacementState> state, ui::Spec child)
        : state_(std::move(state)), child_(std::move(child)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        std::vector<ui::Spec> children;
        children.push_back(std::move(child_));
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<PlacementComponent>(state);
            },
            std::move(children)};
    }

private:
    std::shared_ptr<PlacementState> state_;
    ui::Spec child_;
};

class PassthroughComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty()
            ? ui::Size{40.0f, 30.0f}
            : children.front().preferred;
    }

    void layout_children(
        ui::Rect bounds,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements[0].bounds = bounds;
    }

    void paint(ui::PaintContext&) const override {}
};

class Passthrough {
public:
    explicit Passthrough(ui::Spec child) : child_(std::move(child)) {}

    ui::Spec spec() && {
        std::vector<ui::Spec> children;
        children.push_back(std::move(child_));
        return ui::Spec{
            [] { return std::make_unique<PassthroughComponent>(); },
            std::move(children)};
    }

private:
    ui::Spec child_;
};

void prepare(ui::Tree& tree, ui::Size viewport = {160.0f, 80.0f}) {
    tree.mount();
    tree.layout(viewport);
}

void descendant_invalidation_and_idempotent_generation() {
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Passthrough{Probe{leaf}.spec()}.spec())};
    prepare(tree);

    const auto boundary = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, boundary));
    NUI_CHECK(ui::TreeTestAccess::commit(tree, boundary));

    const auto clean = ui::TreeTestAccess::state(tree, boundary);
    NUI_CHECK(clean && !clean->stale);

    leaf->invalidate_paint();
    const auto first = ui::TreeTestAccess::state(tree, boundary);
    NUI_CHECK(first && first->stale);
    NUI_CHECK(first->generation == clean->generation + 1U);
    NUI_CHECK(first->serial == clean->serial + 1U);
    NUI_CHECK(tree.paint_dirty());

    leaf->invalidate_paint();
    const auto repeated = ui::TreeTestAccess::state(tree, boundary);
    NUI_CHECK(repeated && repeated->stale);
    NUI_CHECK(repeated->generation == first->generation);
    NUI_CHECK(repeated->serial == first->serial + 1U);
}

void nested_boundaries_stale_together() {
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(
        Passthrough{Passthrough{Probe{leaf}.spec()}.spec()}.spec())};
    prepare(tree);

    const auto outer = ui::TreeTestAccess::root_id(tree);
    const auto inner = ui::TreeTestAccess::child_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, outer));
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, inner));
    NUI_CHECK(ui::TreeTestAccess::commit(tree, outer));
    NUI_CHECK(ui::TreeTestAccess::commit(tree, inner));

    leaf->invalidate_paint();
    const auto outer_state = ui::TreeTestAccess::state(tree, outer);
    const auto inner_state = ui::TreeTestAccess::state(tree, inner);
    NUI_CHECK(outer_state && outer_state->stale);
    NUI_CHECK(inner_state && inner_state->stale);
}

void ancestor_repaint_preserves_clean_content() {
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Passthrough{Probe{leaf}.spec()}.spec())};
    prepare(tree);

    const auto boundary = ui::TreeTestAccess::child_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, boundary));
    NUI_CHECK(ui::TreeTestAccess::commit(tree, boundary));
    const auto before = ui::TreeTestAccess::state(tree, boundary);
    NUI_CHECK(before && !before->stale);

    tree.invalidate();
    const auto after = ui::TreeTestAccess::state(tree, boundary);
    NUI_CHECK(after && !after->stale);
    NUI_CHECK(after->generation == before->generation);
    NUI_CHECK(after->serial == before->serial);
}

void translation_reuses_but_size_change_stales() {
    auto placement = std::make_shared<PlacementState>();
    auto child = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Placement{placement, Probe{child}.spec()}.spec())};
    prepare(tree);

    const auto boundary = ui::TreeTestAccess::child_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, boundary));
    NUI_CHECK(ui::TreeTestAccess::commit(tree, boundary));
    const auto initial = ui::TreeTestAccess::state(tree, boundary);
    const auto initial_bounds = ui::TreeTestAccess::bounds(tree, boundary);
    NUI_CHECK(initial && !initial->stale);

    placement->x = 25.0f;
    tree.invalidate_layout();
    tree.layout({160.0f, 80.0f});

    const auto translated = ui::TreeTestAccess::state(tree, boundary);
    const auto translated_bounds = ui::TreeTestAccess::bounds(tree, boundary);
    NUI_CHECK(translated && !translated->stale);
    NUI_CHECK(translated->generation == initial->generation);
    NUI_CHECK(translated->serial == initial->serial);
    NUI_CHECK(translated_bounds.x != initial_bounds.x);
    NUI_CHECK(translated_bounds.w == initial_bounds.w);

    placement->width = 55.0f;
    tree.invalidate_layout();
    tree.layout({160.0f, 80.0f});

    const auto resized = ui::TreeTestAccess::state(tree, boundary);
    NUI_CHECK(resized && resized->stale);
    NUI_CHECK(resized->generation == translated->generation + 1U);
    NUI_CHECK(resized->local_size.w == 55.0f);
}

void reentrant_invalidation_rejects_refresh_commit() {
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Passthrough{Probe{leaf}.spec()}.spec())};
    prepare(tree);

    const auto boundary = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, boundary));
    NUI_CHECK(ui::TreeTestAccess::stale_during_refresh_rejects_commit(
        tree, boundary, leaf->invalidate_paint));
}

void lifetime_identity_changes_on_remount() {
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Passthrough{Probe{leaf}.spec()}.spec())};
    prepare(tree);

    const auto boundary = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, boundary));
    NUI_CHECK(ui::TreeTestAccess::remount_rejects_previous_lifetime(tree, boundary));
}

void dynamic_replacement_cannot_inherit_boundary_identity() {
    ui::State<bool> visible{true};
    auto probe = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(ui::If{visible, Probe{probe}}.spec())};
    prepare(tree);

    const auto first = ui::TreeTestAccess::child_id(tree);
    NUI_CHECK(first != ui::kInvalidNodeId);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, first));
    const auto first_state = ui::TreeTestAccess::state(tree, first);
    NUI_CHECK(first_state);

    visible.set(false);
    tree.layout({160.0f, 80.0f});
    NUI_CHECK(!ui::TreeTestAccess::registered(tree, first));

    visible.set(true);
    tree.layout({160.0f, 80.0f});
    const auto second = ui::TreeTestAccess::child_id(tree);
    NUI_CHECK(second != ui::kInvalidNodeId);
    NUI_CHECK(second != first);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, second));
    const auto second_state = ui::TreeTestAccess::state(tree, second);
    NUI_CHECK(second_state);
    NUI_CHECK(second_state->lifetime != first_state->lifetime);
}

void failed_layout_cannot_leave_resized_boundary_clean() {
    auto placement = std::make_shared<PlacementState>();
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Placement{placement, Probe{leaf}.spec()}.spec())};
    prepare(tree);

    const auto root = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, root));
    NUI_CHECK(ui::TreeTestAccess::commit(tree, root));
    const auto clean = ui::TreeTestAccess::state(tree, root);
    NUI_CHECK(clean && !clean->stale);

    placement->throw_on_layout = true;
    bool failed = false;
    try {
        tree.layout({200.0f, 100.0f});
    } catch (const std::runtime_error&) {
        failed = true;
    }
    NUI_CHECK(failed);

    const auto after = ui::TreeTestAccess::state(tree, root);
    NUI_CHECK(after && after->stale);
    NUI_CHECK(after->generation == clean->generation + 1U);
}

void counter_exhaustion_fails_closed_without_wrap() {
    auto leaf = std::make_shared<ProbeState>();
    ui::Tree tree{ui::compile(Passthrough{Probe{leaf}.spec()}.spec())};
    prepare(tree);

    const auto boundary = ui::TreeTestAccess::root_id(tree);
    NUI_CHECK(ui::TreeTestAccess::register_boundary(tree, boundary));
    ui::TreeTestAccess::force_serial_exhaustion(tree, boundary);
    leaf->invalidate_paint();
    NUI_CHECK(!ui::TreeTestAccess::state(tree, boundary).has_value());
}

void suite() {
    descendant_invalidation_and_idempotent_generation();
    nested_boundaries_stale_together();
    ancestor_repaint_preserves_clean_content();
    translation_reuses_but_size_change_stales();
    reentrant_invalidation_rejects_refresh_commit();
    lifetime_identity_changes_on_remount();
    dynamic_replacement_cannot_inherit_boundary_identity();
    failed_layout_cannot_leave_resized_boundary_clean();
    counter_exhaustion_fails_closed_without_wrap();
}

} // namespace

int main() {
    return test::run("raster_cache_boundary", &suite);
}
