#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

struct AnchorState {
    ui::NodeId id{ui::kInvalidNodeId};
    int pointer_downs{};
    bool focused{};
    std::string semantic_description;
};

class AnchorComponent final : public ui::Component {
public:
    AnchorComponent(bool focusable, std::shared_ptr<AnchorState> state)
        : focusable_(focusable), state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return focusable_; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override { state_->id = context.node_id(); }

    void focus_changed(bool focused, ui::FocusContext&) override {
        state_->focused = focused;
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Custom;
        info.description = state_->semantic_description;
        return info;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerDown) {
            ++state_->pointer_downs;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    bool focusable_{};
    std::shared_ptr<AnchorState> state_;
};

class Anchor {
public:
    Anchor(bool focusable, std::shared_ptr<AnchorState> state)
        : focusable_(focusable), state_(std::move(state)) {}

    ui::Spec spec() && {
        const bool focusable = focusable_;
        auto state = std::move(state_);
        return ui::Spec{
            [focusable, state = std::move(state)] {
                return std::make_unique<AnchorComponent>(focusable, state);
            },
            {}};
    }

private:
    bool focusable_{};
    std::shared_ptr<AnchorState> state_;
};

ui::Spec tooltip_spec(std::string text,
                      bool focusable,
                      std::shared_ptr<AnchorState> state,
                      std::chrono::milliseconds delay = ui::Tooltip::kDefaultDelay) {
    return ui::make_spec(ui::Tooltip{
        std::move(text), Anchor{focusable, std::move(state)}}.delay(delay));
}

class TooltipRootComponent final : public ui::Component {
public:
    explicit TooltipRootComponent(std::vector<ui::Rect> slots)
        : slots_(std::move(slots)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {360.0f, 240.0f};
    }

    void layout_children(
        ui::Rect,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        const auto count = std::min(placements.size(), slots_.size());
        for (std::size_t i = 0; i < count; ++i) placements[i].bounds = slots_[i];
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::vector<ui::Rect> slots_;
};

class TooltipRoot {
public:
    TooltipRoot(std::vector<ui::Rect> slots, std::vector<ui::Spec> children)
        : slots_(std::move(slots)), children_(std::move(children)) {}

    ui::Spec spec() && {
        auto slots = std::move(slots_);
        return ui::Spec{
            [slots = std::move(slots)]() mutable {
                return std::make_unique<TooltipRootComponent>(std::move(slots));
            },
            std::move(children_)};
    }

private:
    std::vector<ui::Rect> slots_;
    std::vector<ui::Spec> children_;
};

std::vector<ui::Rect> one_slot() {
    return {ui::Rect{20.0f, 20.0f, 100.0f, 30.0f}};
}

std::vector<ui::Rect> two_slots() {
    return {ui::Rect{20.0f, 20.0f, 100.0f, 30.0f},
            ui::Rect{20.0f, 80.0f, 100.0f, 30.0f}};
}

constexpr float kFirstCenterX = 70.0f;
constexpr float kFirstCenterY = 35.0f;
constexpr float kSecondCenterX = 70.0f;
constexpr float kSecondCenterY = 95.0f;
constexpr float kOutsideX = 320.0f;
constexpr float kOutsideY = 220.0f;

struct Harness {
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock{
        std::make_shared<ui::detail::ManualDispatcherClock>()};
    ui::detail::DispatcherOwner owner{{}, clock};
    test::MockPlatform platform{};

    Harness() { platform.dispatcher_value = owner.dispatcher(); }

    void advance(std::chrono::milliseconds delta) { clock->advance(delta); }
};

void move_pointer(ui::UI& ui, test::MockPlatform& platform, float x, float y) {
    (void)ui.dispatch(test::pointer(ui::InputType::PointerMove, x, y), platform);
}

void pointer_down(ui::UI& ui, test::MockPlatform& platform, float x, float y) {
    (void)ui.dispatch(test::pointer(ui::InputType::PointerDown, x, y), platform);
}

void exact_499_500_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Reset to default", false, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    harness.advance(1ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    const auto entries = ui.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK(entries.front().mode == ui::OverlayMode::NonModal);
    NUI_CHECK(entries.front().pointer_policy == ui::OverlayPointerPolicy::Ignore);
    NUI_CHECK(entries.front().anchor.has_value());
    NUI_CHECK(entries.front().placement == ui::OverlayPlacement::Auto);
}

void zero_delay_checkpoint_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        one_slot(), {tooltip_spec("Immediate at checkpoint", false, anchor, 0ms)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    // Zero delay must never run reentrantly inside the pointer callback.
    NUI_CHECK(ui.overlay_entries().empty());

    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);
}

void pointer_move_inside_does_not_restart_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Hover help", false, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(300ms);
    move_pointer(ui, harness.platform, kFirstCenterX + 10.0f, kFirstCenterY + 5.0f);
    harness.advance(199ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    harness.advance(1ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);
}

void leave_reenter_restarts_full_delay_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Hover help", false, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(400ms);
    move_pointer(ui, harness.platform, kOutsideX, kOutsideY);
    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());
    harness.advance(1ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);
}

void focus_uses_same_delay_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Focus help", true, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    NUI_CHECK(anchor->focused);
    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    harness.advance(1ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);
}

void focus_loss_regain_restarts_full_delay_contract() {
    Harness harness;
    auto first = std::make_shared<AnchorState>();
    auto second = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        two_slots(),
        {tooltip_spec("First help", true, first), tooltip_spec("Second help", true, second)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);
    NUI_CHECK(first->focused);
    NUI_CHECK(!second->focused);

    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    NUI_CHECK(ui::handled(ui.dispatch(test::key(ui::Key::Tab), harness.platform)));
    NUI_CHECK(second->focused);
    NUI_CHECK(ui.overlay_entries().empty());

    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    NUI_CHECK(ui::handled(ui.dispatch(test::key(ui::Key::Tab, true), harness.platform)));
    NUI_CHECK(first->focused);
    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());
    harness.advance(1ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);
}

void switching_anchors_never_reuses_elapsed_delay_contract() {
    Harness harness;
    auto first = std::make_shared<AnchorState>();
    auto second = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        two_slots(),
        {tooltip_spec("First help", false, first), tooltip_spec("Second help", false, second)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(400ms);
    move_pointer(ui, harness.platform, kSecondCenterX, kSecondCenterY);
    harness.advance(499ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    harness.advance(1ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    const auto entries = ui.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK(entries.front().anchor.has_value());
    const auto info = ui.component_semantics(*entries.front().anchor);
    NUI_CHECK(info.has_value());
    NUI_CHECK(info->description == "Second help");
}

void hover_plus_focus_combined_eligibility_contract() {
    Harness harness;
    auto first = std::make_shared<AnchorState>();
    auto second = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        two_slots(),
        {tooltip_spec("Combined help", true, first), tooltip_spec("Other help", true, second)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);
    NUI_CHECK(first->focused);

    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    move_pointer(ui, harness.platform, kOutsideX, kOutsideY);
    harness.advance(2000ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    NUI_CHECK(ui::handled(ui.dispatch(test::key(ui::Key::Tab), harness.platform)));
    NUI_CHECK(second->focused);
    NUI_CHECK(ui.overlay_entries().empty());
}

void pointer_down_suppresses_until_new_transition_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Press help", false, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    pointer_down(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    NUI_CHECK(anchor->pointer_downs == 1);
    NUI_CHECK(ui.overlay_entries().empty());

    // Stationary hover must not re-arm after the dismissal.
    harness.advance(600ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    move_pointer(ui, harness.platform, kFirstCenterX + 5.0f, kFirstCenterY);
    harness.advance(600ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    // A complete leave -> enter transition is a new eligibility lifetime.
    move_pointer(ui, harness.platform, kOutsideX, kOutsideY);
    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);
}

void pointer_down_elsewhere_cancels_contract() {
    Harness harness;
    auto first = std::make_shared<AnchorState>();
    auto second = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        two_slots(),
        {tooltip_spec("First help", false, first), ui::make_spec(Anchor{false, second})}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    // A PointerDown anywhere in the same UI dismisses the transient
    // presentation immediately, not only a press on the anchor itself.
    pointer_down(ui, harness.platform, kSecondCenterX, kSecondCenterY);
    NUI_CHECK(second->pointer_downs == 1);
    NUI_CHECK(ui.overlay_entries().empty());

    harness.advance(600ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());
}

void tooltip_is_not_hit_testable_contract() {
    Harness harness;
    auto tooltip_anchor = std::make_shared<AnchorState>();
    auto control = std::make_shared<AnchorState>();
    const ui::Rect control_bounds{20.0f, 60.0f, 240.0f, 140.0f};
    ui::UI ui{TooltipRoot{
        {ui::Rect{20.0f, 20.0f, 100.0f, 30.0f}, control_bounds},
        {tooltip_spec("A long tooltip surface that overlaps the interactive control below it",
                      false,
                      tooltip_anchor),
         ui::make_spec(Anchor{false, control})}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    ui.resize({360.0f, 240.0f});

    const auto entries = ui.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK(entries.front().resolved);
    const auto bounds = entries.front().bounds;
    const ui::Point point{bounds.x + 10.0f,
                          std::max(control_bounds.y + 1.0f, bounds.y + 1.0f)};
    NUI_CHECK(bounds.contains(point));
    NUI_CHECK(control_bounds.contains(point));

    NUI_CHECK(control->pointer_downs == 0);
    pointer_down(ui, harness.platform, point.x, point.y);
    // The non-hit-test surface passes the press through and still dismisses.
    NUI_CHECK(control->pointer_downs == 1);
    NUI_CHECK(tooltip_anchor->pointer_downs == 0);
    NUI_CHECK(ui.overlay_entries().empty());
}

void tooltip_does_not_take_focus_contract() {
    Harness harness;
    auto first = std::make_shared<AnchorState>();
    auto second = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        two_slots(),
        {tooltip_spec("Focus help", true, first), ui::make_spec(Anchor{true, second})}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);
    NUI_CHECK(first->focused);

    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    // Tab traversal must land on the next real control, never on the
    // non-focusable tooltip presentation.
    NUI_CHECK(ui::handled(ui.dispatch(test::key(ui::Key::Tab), harness.platform)));
    NUI_CHECK(second->focused);
    NUI_CHECK(!first->focused);

    NUI_CHECK(ui::handled(ui.dispatch(test::key(ui::Key::Tab), harness.platform)));
    NUI_CHECK(first->focused);
}


void unavailability_cancels_contract() {
    // Hidden and Collapsed both make the anchor unavailable while a timer is
    // pending and while a surface is visible.
    for (const auto mode : {ui::VisibilityMode::Hidden, ui::VisibilityMode::Collapsed}) {
        Harness harness;
        ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{
            one_slot(),
            {ui::make_spec(ui::Visibility{
                visibility, ui::Tooltip{"Hidden help", Anchor{false, anchor}}})}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        harness.advance(100ms);
        visibility.set(mode);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 0);
        NUI_CHECK(ui.overlay_entries().empty());

        visibility.set(ui::VisibilityMode::Visible);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 0);
        NUI_CHECK(ui.overlay_entries().empty());

        move_pointer(ui, harness.platform, kOutsideX, kOutsideY);
        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 1);
        NUI_CHECK(ui.overlay_entries().size() == 1);
    }
}

void disabled_cancels_contract() {
    {
        Harness harness;
        ui::State<bool> enabled{true};
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{
            one_slot(),
            {ui::make_spec(ui::Enabled{
                enabled, ui::Tooltip{"Enabled help", Anchor{false, anchor}}})}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        enabled.set(false);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 0);
        NUI_CHECK(ui.overlay_entries().empty());

        enabled.set(true);
        move_pointer(ui, harness.platform, kOutsideX, kOutsideY);
        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 1);
        NUI_CHECK(ui.overlay_entries().size() == 1);

        enabled.set(false);
        NUI_CHECK(ui.overlay_entries().empty());
    }

    // A disabled decorated child is not an eligible anchor even though the
    // Tooltip decorator itself remains enabled.
    {
        Harness harness;
        ui::State<bool> enabled{true};
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{
            one_slot(),
            {ui::make_spec(ui::Tooltip{
                "Child-disabled help",
                ui::Enabled{enabled, Anchor{false, anchor}}})}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        enabled.set(false);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 0);
        NUI_CHECK(ui.overlay_entries().empty());
    }
}

void subtree_removal_cancels_without_late_callback_contract() {
    Harness harness;
    ui::State<bool> present{true};
    auto anchor = std::make_shared<AnchorState>();
    ui::UI ui{TooltipRoot{
        one_slot(),
        {ui::make_spec(ui::If{
            present, ui::Tooltip{"Removed help", Anchor{false, anchor}}})}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    present.set(false);
    ui.resize({360.0f, 240.0f});
    harness.advance(600ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    // Same path after the surface is already visible.
    present.set(true);
    ui.resize({360.0f, 240.0f});
    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    NUI_CHECK(ui.overlay_entries().size() == 1);

    present.set(false);
    ui.resize({360.0f, 240.0f});
    harness.advance(600ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());
}

void view_deactivation_cancels_contract() {
    {
        Harness harness;
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Deactivate help", false, anchor)}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        ui.deactivate(harness.platform);
        harness.advance(600ms);
        NUI_CHECK(harness.owner.checkpoint() == 0);
        NUI_CHECK(ui.overlay_entries().empty());
    }

    {
        Harness harness;
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Deactivate help", false, anchor)}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 1);
        NUI_CHECK(ui.overlay_entries().size() == 1);

        ui.deactivate(harness.platform);
        NUI_CHECK(ui.overlay_entries().empty());
    }
}

void overlay_opening_cancels_pending_and_visible_contract() {
    // Pending tooltip.
    {
        Harness harness;
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Menu help", false, anchor)}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        harness.advance(100ms);

        ui::OverlaySpec modal;
        modal.mode = ui::OverlayMode::Modal;
        modal.content = ui::make_spec(ui::Spacer{40.0f, 40.0f});
        NUI_CHECK(ui.show_overlay(std::move(modal)).valid());

        harness.advance(600ms);
        NUI_CHECK(harness.owner.checkpoint() == 0);
        const auto entries = ui.overlay_entries();
        NUI_CHECK(entries.size() == 1);
        NUI_CHECK(entries.front().mode == ui::OverlayMode::Modal);
    }

    // Visible tooltip.
    {
        Harness harness;
        auto anchor = std::make_shared<AnchorState>();
        ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("Menu help", false, anchor)}}};
        ui.resize({360.0f, 240.0f});
        ui.activate(harness.platform);

        move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
        harness.advance(500ms);
        NUI_CHECK(harness.owner.checkpoint() == 1);
        NUI_CHECK(ui.overlay_entries().size() == 1);

        ui::OverlaySpec modal;
        modal.mode = ui::OverlayMode::Modal;
        modal.content = ui::make_spec(ui::Spacer{40.0f, 40.0f});
        NUI_CHECK(ui.show_overlay(std::move(modal)).valid());

        const auto entries = ui.overlay_entries();
        NUI_CHECK(entries.size() == 1);
        NUI_CHECK(entries.front().mode == ui::OverlayMode::Modal);
    }
}

void t061_owns_placement_and_clamping_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    const ui::Rect anchor_bounds{20.0f, 70.0f, 80.0f, 20.0f};
    ui::UI ui{TooltipRoot{{anchor_bounds}, {tooltip_spec("Tiny", false, anchor)}}};
    const ui::Size viewport{200.0f, 110.0f};
    ui.resize(viewport);
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, anchor_bounds.x + 10.0f, anchor_bounds.y + 10.0f);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);

    ui.resize(viewport);
    const auto entries = ui.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK(entries.front().placement == ui::OverlayPlacement::Auto);
    NUI_CHECK(entries.front().resolved);
    const auto bounds = entries.front().bounds;
    // T061 Auto must flip the surface above an anchor that cannot fit below.
    NUI_CHECK(bounds.y + bounds.h <= anchor_bounds.y);
    NUI_CHECK(bounds.x >= 0.0f);
    NUI_CHECK(bounds.y >= 0.0f);
    NUI_CHECK(bounds.x + bounds.w <= viewport.w);
    NUI_CHECK(bounds.y + bounds.h <= viewport.h);
}

void semantic_help_is_independent_of_presentation_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    anchor->semantic_description = "Explicit anchor help";
    ui::UI ui{TooltipRoot{
        one_slot(), {tooltip_spec("Reset to default", false, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    const auto anchor_info = ui.component_semantics(anchor->id);
    NUI_CHECK(anchor_info.has_value());
    NUI_CHECK(anchor_info->description == "Explicit anchor help");

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(500ms);
    NUI_CHECK(harness.owner.checkpoint() == 1);
    const auto entries = ui.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK(entries.front().anchor.has_value());

    const auto tooltip_info = ui.component_semantics(*entries.front().anchor);
    NUI_CHECK(tooltip_info.has_value());
    NUI_CHECK(tooltip_info->role == ui::SemanticRole::None);
    NUI_CHECK(tooltip_info->description == "Reset to default");

    // The semantic contribution survives the visual presentation lifetime.
    move_pointer(ui, harness.platform, kOutsideX, kOutsideY);
    NUI_CHECK(ui.overlay_entries().empty());
    const auto retained = ui.component_semantics(*entries.front().anchor);
    NUI_CHECK(retained.has_value());
    NUI_CHECK(retained->description == "Reset to default");
}

void empty_text_does_not_erase_explicit_semantics_contract() {
    Harness harness;
    auto anchor = std::make_shared<AnchorState>();
    anchor->semantic_description = "Explicit anchor help";
    ui::UI ui{TooltipRoot{one_slot(), {tooltip_spec("", false, anchor)}}};
    ui.resize({360.0f, 240.0f});
    ui.activate(harness.platform);

    move_pointer(ui, harness.platform, kFirstCenterX, kFirstCenterY);
    harness.advance(600ms);
    NUI_CHECK(harness.owner.checkpoint() == 0);
    NUI_CHECK(ui.overlay_entries().empty());

    const auto info = ui.component_semantics(anchor->id);
    NUI_CHECK(info.has_value());
    NUI_CHECK(info->description == "Explicit anchor help");
}

void two_ui_instances_are_isolated_contract() {
    Harness first;
    Harness second;
    auto first_anchor = std::make_shared<AnchorState>();
    auto second_anchor = std::make_shared<AnchorState>();
    ui::UI first_ui{TooltipRoot{
        one_slot(), {tooltip_spec("First UI help", false, first_anchor)}}};
    ui::UI second_ui{TooltipRoot{
        one_slot(), {tooltip_spec("Second UI help", false, second_anchor)}}};
    first_ui.resize({360.0f, 240.0f});
    second_ui.resize({360.0f, 240.0f});
    first_ui.activate(first.platform);
    second_ui.activate(second.platform);

    move_pointer(first_ui, first.platform, kFirstCenterX, kFirstCenterY);
    move_pointer(second_ui, second.platform, kFirstCenterX, kFirstCenterY);

    first.advance(500ms);
    second.advance(300ms);
    NUI_CHECK(first.owner.checkpoint() == 1);
    NUI_CHECK(second.owner.checkpoint() == 0);

    NUI_CHECK(first_ui.overlay_entries().size() == 1);
    NUI_CHECK(second_ui.overlay_entries().empty());

    second.advance(200ms);
    NUI_CHECK(second.owner.checkpoint() == 1);
    NUI_CHECK(first_ui.overlay_entries().size() == 1);
    NUI_CHECK(second_ui.overlay_entries().size() == 1);

    // Destroying one UI leaves the sibling fully functional.
    ui::UI replacement{TooltipRoot{
        one_slot(), {tooltip_spec("Replacement help", false, first_anchor)}}};
    (void)replacement;
    move_pointer(second_ui, second.platform, kOutsideX, kOutsideY);
    move_pointer(second_ui, second.platform, kFirstCenterX, kFirstCenterY);
    second.advance(500ms);
    NUI_CHECK(second.owner.checkpoint() == 1);
    NUI_CHECK(second_ui.overlay_entries().size() == 1);
}

void suite() {
    exact_499_500_contract();
    zero_delay_checkpoint_contract();
    pointer_move_inside_does_not_restart_contract();
    leave_reenter_restarts_full_delay_contract();
    focus_uses_same_delay_contract();
    focus_loss_regain_restarts_full_delay_contract();
    switching_anchors_never_reuses_elapsed_delay_contract();
    hover_plus_focus_combined_eligibility_contract();
    pointer_down_suppresses_until_new_transition_contract();
    pointer_down_elsewhere_cancels_contract();
    tooltip_is_not_hit_testable_contract();
    tooltip_does_not_take_focus_contract();
    unavailability_cancels_contract();
    disabled_cancels_contract();
    subtree_removal_cancels_without_late_callback_contract();
    view_deactivation_cancels_contract();
    overlay_opening_cancels_pending_and_visible_contract();
    t061_owns_placement_and_clamping_contract();
    semantic_help_is_independent_of_presentation_contract();
    empty_text_does_not_erase_explicit_semantics_contract();
    two_ui_instances_are_isolated_contract();
}

} // namespace

int main() { return test::run("t062_tooltip", &suite); }
