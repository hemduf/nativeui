#include "test_support.hpp"

#include <nativeui/component_state.hpp>

#include <memory>
#include <vector>

namespace {

struct AvailabilityProbeState {
    ui::NodeId node_id{ui::kInvalidNodeId};
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
    int focus_in{};
    int focus_out{};
    int paints{};
    int pointer_down{};
    int pointer_move{};
    int pointer_wheel{};
    int pointer_cancel{};
    int key_down{};
    int text_input{};
    int commands{};
    ui::ComponentAvailability last_paint_availability{};
    ui::ComponentAvailability last_input_availability{};
};

class AvailabilityProbeComponent final : public ui::Component {
public:
    explicit AvailabilityProbeComponent(std::shared_ptr<AvailabilityProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }

    void mount(ui::MountContext& context) override {
        state_->node_id = context.node_id();
        ++state_->mounts;
    }

    void activate(ui::LifecycleContext&) override { ++state_->activates; }
    void deactivate(ui::LifecycleContext&) override { ++state_->deactivates; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }

    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (focused) {
            ++state_->focus_in;
            context.set_text_input(true, context.bounds());
        } else {
            ++state_->focus_out;
            context.set_text_input(false);
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        state_->last_input_availability = effective_availability();
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++state_->pointer_down;
            context.capture_pointer();
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->pointer_move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerWheel:
            ++state_->pointer_wheel;
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            ++state_->pointer_cancel;
            return ui::EventResult::Handled;
        case ui::InputType::KeyDown:
            ++state_->key_down;
            return ui::EventResult::Handled;
        case ui::InputType::TextInput:
            ++state_->text_input;
            return ui::EventResult::Handled;
        case ui::InputType::Command:
            ++state_->commands;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {
        ++state_->paints;
        state_->last_paint_availability = effective_availability();
    }

private:
    std::shared_ptr<AvailabilityProbeState> state_;
};

class AvailabilityProbe {
public:
    explicit AvailabilityProbe(std::shared_ptr<AvailabilityProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<AvailabilityProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<AvailabilityProbeState> state_;
};

ui::InputEvent command(ui::Command value) {
    ui::InputEvent event{};
    event.type = ui::InputType::Command;
    event.command = value;
    return event;
}

void visibility_preserves_identity_and_lifecycle() {
    test::MockPlatform platform;
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    auto probe = std::make_shared<AvailabilityProbeState>();
    ui::UI tree{ui::Visibility{visibility, AvailabilityProbe{probe}}};

    auto metrics = tree.measure();
    NUI_CHECK_NEAR(metrics.preferred.w, 100.0f, 0.001f);
    NUI_CHECK_NEAR(metrics.preferred.h, 40.0f, 0.001f);
    NUI_CHECK(probe->mounts == 1);

    tree.resize({100.0f, 40.0f});
    tree.activate(platform);
    NUI_CHECK(probe->activates == 1);
    NUI_CHECK(probe->focus_in == 1);
    NUI_CHECK(platform.text_input_active);

    ui::HeadlessRenderer renderer{{100.0f, 40.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    const int visible_paints = probe->paints;
    NUI_CHECK(visible_paints > 0);
    NUI_CHECK(probe->last_paint_availability.visibility == ui::VisibilityMode::Visible);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    NUI_CHECK(probe->pointer_down == 1);

    visibility.set(ui::VisibilityMode::Hidden);
    NUI_CHECK(probe->pointer_cancel == 1);
    NUI_CHECK(probe->focus_out == 1);
    NUI_CHECK(!platform.text_input_active);
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(tree.paint_dirty());

    metrics = tree.measure();
    NUI_CHECK_NEAR(metrics.preferred.w, 100.0f, 0.001f);
    NUI_CHECK_NEAR(metrics.preferred.h, 40.0f, 0.001f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(probe->paints == visible_paints);
    NUI_CHECK(probe->mounts == 1);
    NUI_CHECK(probe->activates == 1);
    NUI_CHECK(probe->deactivates == 0);
    NUI_CHECK(probe->unmounts == 0);

    const auto hidden = tree.component_availability(probe->node_id);
    NUI_CHECK(hidden.has_value());
    NUI_CHECK(hidden->visibility == ui::VisibilityMode::Hidden);

    visibility.set(ui::VisibilityMode::Collapsed);
    NUI_CHECK(tree.layout_dirty());
    metrics = tree.measure();
    NUI_CHECK_NEAR(metrics.preferred.w, 0.0f, 0.001f);
    NUI_CHECK_NEAR(metrics.preferred.h, 0.0f, 0.001f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(probe->paints == visible_paints);

    visibility.set(ui::VisibilityMode::Visible);
    NUI_CHECK(tree.layout_dirty());
    metrics = tree.measure();
    NUI_CHECK_NEAR(metrics.preferred.w, 100.0f, 0.001f);
    NUI_CHECK_NEAR(metrics.preferred.h, 40.0f, 0.001f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(probe->paints == visible_paints + 1);
    NUI_CHECK(probe->mounts == 1);
    NUI_CHECK(probe->activates == 1);
}

void disabled_subtree_is_removed_from_normal_input_and_focus() {
    test::MockPlatform platform;
    ui::State<bool> enabled{true};
    auto probe = std::make_shared<AvailabilityProbeState>();
    ui::UI tree{ui::Enabled{enabled, AvailabilityProbe{probe}}};
    tree.resize({100.0f, 40.0f});
    tree.activate(platform);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    NUI_CHECK(probe->pointer_down == 1);
    NUI_CHECK(platform.text_input_active);

    enabled.set(false);
    NUI_CHECK(probe->pointer_cancel == 1);
    NUI_CHECK(probe->focus_out == 1);
    NUI_CHECK(!platform.text_input_active);
    NUI_CHECK(!tree.layout_dirty());

    const auto disabled = tree.component_availability(probe->node_id);
    NUI_CHECK(disabled.has_value());
    NUI_CHECK(!disabled->enabled);
    NUI_CHECK(disabled->visibility == ui::VisibilityMode::Visible);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), platform);
    auto wheel = test::pointer(ui::InputType::PointerWheel, 20.0f, 20.0f);
    wheel.delta = {0.0f, 1.0f};
    tree.dispatch(wheel, platform);
    tree.dispatch(test::key(ui::Key::A), platform);
    tree.dispatch(test::text("x"), platform);
    tree.dispatch(command(ui::Command::Copy), platform);

    NUI_CHECK(probe->pointer_down == 1);
    NUI_CHECK(probe->pointer_move == 0);
    NUI_CHECK(probe->pointer_wheel == 0);
    NUI_CHECK(probe->key_down == 0);
    NUI_CHECK(probe->text_input == 0);
    NUI_CHECK(probe->commands == 0);

    enabled.set(true);
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    NUI_CHECK(probe->pointer_down == 2);
    NUI_CHECK(probe->last_input_availability.enabled);
}

void inherited_state_is_monotonic_and_equal_effective_updates_are_noops() {
    test::MockPlatform platform;
    ui::State<bool> outer_enabled{false};
    ui::State<bool> inner_enabled{true};
    ui::State<bool> outer_read_only{true};
    ui::State<bool> inner_read_only{false};
    auto probe = std::make_shared<AvailabilityProbeState>();

    ui::UI tree{
        ui::Enabled{outer_enabled,
            ui::Enabled{inner_enabled,
                ui::ReadOnly{outer_read_only,
                    ui::ReadOnly{inner_read_only, AvailabilityProbe{probe}}}}}
    };
    tree.resize({100.0f, 40.0f});
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{100.0f, 40.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());

    auto availability = tree.component_availability(probe->node_id);
    NUI_CHECK(availability.has_value());
    NUI_CHECK(!availability->enabled);
    NUI_CHECK(availability->read_only);

    inner_enabled.set(false);
    NUI_CHECK(!tree.dirty());
    inner_enabled.set(true);
    NUI_CHECK(!tree.dirty());
    inner_read_only.set(true);
    NUI_CHECK(!tree.dirty());
    inner_read_only.set(false);
    NUI_CHECK(!tree.dirty());

    outer_enabled.set(true);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    availability = tree.component_availability(probe->node_id);
    NUI_CHECK(availability->enabled);
    NUI_CHECK(availability->read_only);
}

void two_trees_keep_availability_and_capture_isolated() {
    test::MockPlatform platform_a;
    test::MockPlatform platform_b;
    ui::State<bool> enabled_a{true};
    ui::State<bool> enabled_b{true};
    auto probe_a = std::make_shared<AvailabilityProbeState>();
    auto probe_b = std::make_shared<AvailabilityProbeState>();

    ui::UI tree_a{ui::Enabled{enabled_a, AvailabilityProbe{probe_a}}};
    ui::UI tree_b{ui::Enabled{enabled_b, AvailabilityProbe{probe_b}}};
    tree_a.resize({100.0f, 40.0f});
    tree_b.resize({100.0f, 40.0f});
    tree_a.activate(platform_a);
    tree_b.activate(platform_b);

    tree_a.dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform_a);
    tree_b.dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform_b);
    enabled_a.set(false);

    NUI_CHECK(probe_a->pointer_cancel == 1);
    NUI_CHECK(probe_b->pointer_cancel == 0);
    NUI_CHECK(!tree_a.component_availability(probe_a->node_id)->enabled);
    NUI_CHECK(tree_b.component_availability(probe_b->node_id)->enabled);

    tree_b.dispatch(test::pointer(ui::InputType::PointerMove, 12.0f, 12.0f), platform_b);
    NUI_CHECK(probe_b->pointer_move == 1);
}

void suite() {
    visibility_preserves_identity_and_lifecycle();
    disabled_subtree_is_removed_from_normal_input_and_focus();
    inherited_state_is_monotonic_and_equal_effective_updates_are_noops();
    two_trees_keep_availability_and_capture_isolated();
}

} // namespace

int main() { return test::run("component_state", &suite); }
