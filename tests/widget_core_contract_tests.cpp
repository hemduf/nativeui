#include "test_support.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
struct Observation {
    std::vector<std::vector<ui::ChildMetrics>> metadata;
    std::vector<std::function<void()>> releases;
    int cancellations{};
    ui::NodeId id{};
    std::vector<ui::PointerCancelReason> reasons;
    bool cancel_read_only{};
    bool throw_cancel{};
};

class Probe final : public ui::Component {
public:
    explicit Probe(std::shared_ptr<Observation> state) : state_(std::move(state)) {}
    bool focusable() const noexcept override { return true; }
    bool cancel_capture_on_read_only() const noexcept override { return state_->cancel_read_only; }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40, 20}; }
    ui::Constraints child_constraints(const ui::Constraints& c, std::size_t,
                                     const std::vector<ui::ChildMetrics>& metadata) const override {
        state_->metadata.push_back(metadata);
        return c.loosen();
    }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerDown) {
            state_->releases.push_back(context.pointer_releaser());
            context.capture_pointer();
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerCancel) {
            ++state_->cancellations;
            state_->reasons.push_back(event.cancel_reason);
            if (state_->throw_cancel) throw std::runtime_error("cancel fault");
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }
    void mount(ui::MountContext& context) override { state_->id = context.node_id(); }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<Observation> state_;
};

ui::Spec probe(std::shared_ptr<Observation> state, std::vector<ui::Spec> children = {}) {
    return ui::Spec{[state] { return std::make_unique<Probe>(state); }, std::move(children)};
}

void metadata_is_complete_and_independent_of_measure_order() {
    auto state = std::make_shared<Observation>();
    ui::State<ui::VisibilityMode> collapsed{ui::VisibilityMode::Collapsed};
    ui::UI tree{probe(state, {
        ui::make_spec(ui::Spacer{10, 20}),
        ui::make_spec(ui::Visibility{collapsed, ui::Spacer{10, 20}}),
        ui::make_spec(ui::Spacer{0, 0})})};
    (void)tree.measure();
    NUI_CHECK(state->metadata.size() == 3);
    for (const auto& metadata : state->metadata) {
        NUI_CHECK(metadata.size() == 3);
        NUI_CHECK(metadata[0].participates_in_layout);
        NUI_CHECK(!metadata[1].participates_in_layout);
        NUI_CHECK(metadata[2].participates_in_layout);
        for (const auto& item : metadata) {
            NUI_CHECK(item.minimum.w == 0 && item.preferred.w == 0);
            NUI_CHECK(item.minimum.h == 0 && item.preferred.h == 0);
        }
    }
}

void read_only_cancellation_is_opt_in_and_recovers_after_throw() {
    for (const bool opt_in : {false, true}) {
        auto state = std::make_shared<Observation>();
        state->cancel_read_only = opt_in;
        ui::State<bool> locked{false};
        ui::UI tree{ui::ReadOnly{locked, probe(state)}};
        test::MockPlatform platform;
        tree.resize({80, 40});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
        NUI_CHECK(platform.pointer_capture_begin_count == 1);
        state->throw_cancel = true;
        bool caught = false;
        try { locked.set(true); } catch (const std::runtime_error& error) {
            caught = std::string_view{error.what()} == "cancel fault";
        }
        NUI_CHECK(caught == opt_in);
        NUI_CHECK(state->cancellations == (opt_in ? 1 : 0));
        NUI_CHECK(platform.pointer_capture_end_count == (opt_in ? 1 : 0));
        state->throw_cancel = false;
        tree.refresh_focus(platform);
        NUI_CHECK(tree.component_availability(state->id)->read_only);
        NUI_CHECK(state->cancellations == (opt_in ? 1 : 0));
        locked.set(false);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
        locked.set(true);
        NUI_CHECK(state->cancellations == (opt_in ? 2 : 0));
        tree.deactivate(platform);
        NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
    }
}

void retained_release_cannot_release_new_contacts_or_removed_nodes() {
    auto state = std::make_shared<Observation>();
    test::MockPlatform platform;
    ui::State<bool> present{true};
    auto tree = std::make_unique<ui::UI>(ui::If{present, probe(state)});
    tree->resize({80, 40});
    tree->activate(platform);
    auto down = [&] { tree->dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform); };
    down();
    const auto old_release = state->releases.back();
    old_release();
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    down();
    const auto current_release = state->releases.back();
    old_release();
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    current_release();
    NUI_CHECK(platform.pointer_capture_end_count == 2);
    down();
    const auto removed_release = state->releases.back();
    present.set(false);
    (void)tree->measure();
    NUI_CHECK(platform.pointer_capture_end_count == 3);
    present.set(true);
    (void)tree->measure();
    tree->resize({81, 41});
    down();
    removed_release();
    NUI_CHECK(platform.pointer_capture_end_count == 3);
    NUI_CHECK_NEAR(static_cast<float>(platform.pointer_capture_begin_count), 4.0f, 0.01f);
    tree.reset();
    NUI_CHECK_NEAR(static_cast<float>(platform.pointer_capture_end_count), 4.0f, 0.01f);
    for (const auto& release : state->releases) release();
    NUI_CHECK(platform.pointer_capture_end_count == 4);
}


class HistoricConstraints final : public ui::Component {
public:
    explicit HistoricConstraints(std::shared_ptr<std::vector<std::size_t>> counts) : counts_(std::move(counts)) {}
    ui::Constraints child_constraints(const ui::Constraints& constraints, std::size_t,
                                     std::size_t count) const override {
        counts_->push_back(count);
        return constraints.loosen();
    }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {}; }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<std::vector<std::size_t>> counts_;
};
void historic_virtual_constraints_delegate_through_the_tree() {
    auto counts = std::make_shared<std::vector<std::size_t>>();
    ui::Spec root{[counts] { return std::make_unique<HistoricConstraints>(counts); },
                  {ui::make_spec(ui::Spacer{10,20}), ui::make_spec(ui::Spacer{0,0})}};
    ui::UI tree{std::move(root)};
    (void)tree.measure();
    NUI_CHECK(counts->size() == 2 && (*counts)[0] == 2 && (*counts)[1] == 2);
}
void cancellation_reasons_distinguish_native_from_retained_cleanup() {
    auto state = std::make_shared<Observation>();
    state->cancel_read_only = true;
    ui::State<bool> present{true}, locked{false};
    test::MockPlatform platform;
    ui::UI tree{ui::ReadOnly{locked, ui::If{present, probe(state)}}};
    tree.resize({80,40}); tree.activate(platform);
    const auto down = [&] { tree.dispatch(test::pointer(ui::InputType::PointerDown,10,10),platform); };
    down(); tree.dispatch(test::pointer(ui::InputType::PointerCancel,10,10),platform);
    down(); down();
    locked.set(true);
    locked.set(false);
    down(); present.set(false); (void)tree.measure();
    present.set(true); tree.resize({81,41});
    down(); tree.deactivate(platform);
    NUI_CHECK(state->reasons == std::vector<ui::PointerCancelReason>({
        ui::PointerCancelReason::Native, ui::PointerCancelReason::Replaced,
        ui::PointerCancelReason::Unavailable, ui::PointerCancelReason::Removed,
        ui::PointerCancelReason::Teardown}));
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

void suite() {
    historic_virtual_constraints_delegate_through_the_tree();
    cancellation_reasons_distinguish_native_from_retained_cleanup();
    metadata_is_complete_and_independent_of_measure_order();
    read_only_cancellation_is_opt_in_and_recovers_after_throw();
    retained_release_cannot_release_new_contacts_or_removed_nodes();
}
} // namespace
int main() { return test::run("widget_core_contract", &suite); }
