#include "test_support.hpp"
#include <nativeui/split_view.hpp>

#include <limits>
#include <memory>
#include <stdexcept>

namespace {

struct PaneObservation {
    ui::Rect bounds{};
    ui::Rect clip{};
    bool focused{};
    int presses{};
    int mounts{};
    int unmounts{};
};

class PaneComponent final : public ui::Component {
public:
    explicit PaneComponent(std::shared_ptr<PaneObservation> state) : state_(std::move(state)) {}
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {160.0f,90.0f};
    }
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    void mount(ui::MountContext&) override { ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void focus_changed(bool value, ui::FocusContext&) override { state_->focused = value; }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;
        ++state_->presses;
        return ui::EventResult::Handled;
    }
    void paint(ui::PaintContext& context) const override {
        state_->bounds = context.bounds();
        state_->clip = context.clip_bounds();
    }
private:
    std::shared_ptr<PaneObservation> state_;
};

class Pane {
public:
    explicit Pane(std::shared_ptr<PaneObservation> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = state_;
        return {[state] { return std::make_unique<PaneComponent>(state); }, {}};
    }
private:
    std::shared_ptr<PaneObservation> state_;
};

void render(ui::UI& tree, ui::Size size) {
    ui::HeadlessRenderer renderer{size,1.0f};
    NUI_CHECK(renderer.render(tree));
}

void orientations_order_clipping_and_resize_leave_model_authoritative() {
    ui::State<double> extent{100.0};
    auto first = std::make_shared<PaneObservation>();
    auto second = std::make_shared<PaneObservation>();
    ui::UI tree{ui::SplitView{extent.binding(), Pane{first}, Pane{second}}};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f});
    tree.activate(platform);
    render(tree,{301.0f,100.0f});
    NUI_CHECK_NEAR(first->bounds.w,100.0f,0.001f);
    NUI_CHECK_NEAR(second->bounds.x,101.0f,0.001f);
    NUI_CHECK_NEAR(second->bounds.w,200.0f,0.001f);
    NUI_CHECK_NEAR(first->clip.w,100.0f,0.001f);
    NUI_CHECK_NEAR(second->clip.x,101.0f,0.001f);
    NUI_CHECK(first->focused);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(!first->focused && !second->focused);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(extent.get() == 110.0);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(second->focused);
    tree.resize({61.0f,100.0f});
    render(tree,{61.0f,100.0f});
    NUI_CHECK(extent.get() == 110.0); // A viewport clamp is never a model write.
    NUI_CHECK_NEAR(first->bounds.w,30.0f,0.001f);
    NUI_CHECK_NEAR(second->bounds.w,30.0f,0.001f);
    tree.resize({0.0f,0.0f});
    tree.resize({301.0f,100.0f});
    render(tree,{301.0f,100.0f});
    NUI_CHECK_NEAR(first->bounds.w,110.0f,0.001f);

    ui::State<double> vertical_extent{100.0};
    auto top = std::make_shared<PaneObservation>();
    auto bottom = std::make_shared<PaneObservation>();
    ui::UI vertical{ui::SplitView{vertical_extent, Pane{top}, Pane{bottom}}
        .orientation(ui::SplitOrientation::Vertical)};
    test::MockPlatform vertical_platform;
    vertical.resize({100.0f,301.0f});
    vertical.activate(vertical_platform);
    render(vertical,{100.0f,301.0f});
    NUI_CHECK_NEAR(top->bounds.h,100.0f,0.001f);
    NUI_CHECK_NEAR(bottom->bounds.y,101.0f,0.001f);
    vertical.dispatch(test::key(ui::Key::Tab),vertical_platform);
    vertical.dispatch(test::key(ui::Key::Down),vertical_platform);
    NUI_CHECK(vertical_extent.get() == 110.0);
    vertical.dispatch(test::key(ui::Key::Left),vertical_platform);
    NUI_CHECK(vertical_extent.get() == 110.0);
}

void overlapping_grip_wins_pointer_routing_and_commits_once() {
    ui::State<double> extent{100.0};
    int changes = 0;
    int commits = 0;
    auto first = std::make_shared<PaneObservation>();
    auto second = std::make_shared<PaneObservation>();
    ui::UI tree{ui::SplitView{extent, Pane{first}, Pane{second}}
        .on_change([&](double value) { NUI_CHECK(value == extent.get()); ++changes; })
        .on_commit([&](double value) { NUI_CHECK(value == extent.get()); ++commits; })};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f});
    tree.activate(platform);
    // x=103 overlaps the second pane and the six-DIP hit grip. The separator
    // takes precedence without changing the panes' one-DIP visual separation.
    tree.dispatch(test::pointer(ui::InputType::PointerDown,103.0f,40.0f),platform);
    NUI_CHECK(second->presses == 0);
    NUI_CHECK(platform.pointer_capture_begin_count == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,123.0f,40.0f),platform);
    NUI_CHECK(extent.get() == 120.0);
    NUI_CHECK(changes == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,123.0f,40.0f),platform);
    NUI_CHECK(commits == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,123.0f,40.0f),platform);
    NUI_CHECK(commits == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,120.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,140.5f,40.0f),platform);
    NUI_CHECK(extent.get() == 140.0);
    tree.dispatch(test::key(ui::Key::Escape),platform);
    NUI_CHECK(extent.get() == 120.0);
    NUI_CHECK(commits == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,140.5f,40.0f),platform);
    NUI_CHECK(commits == 1);
}

void external_writes_cancel_without_stale_rollback_and_reentrant_write_wins() {
    ui::State<double> extent{100.0};
    int commits = 0;
    bool replace = false;
    ui::UI tree{ui::SplitView{extent, ui::Spacer{80.0f,80.0f}, ui::Spacer{80.0f,80.0f}}
        .on_change([&](double) { if (replace) extent.set(75.0); })
        .on_commit([&](double) { ++commits; })};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f});
    tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),platform);
    extent.set(80.0);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerCancel,120.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,120.5f,40.0f),platform);
    NUI_CHECK(extent.get() == 80.0 && commits == 0);
    replace = true;
    tree.dispatch(test::pointer(ui::InputType::PointerDown,80.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,100.5f,40.0f),platform);
    NUI_CHECK(extent.get() == 75.0);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,130.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,130.5f,40.0f),platform);
    NUI_CHECK(extent.get() == 75.0 && commits == 0);
}

void throwing_commit_releases_capture_and_next_gesture_succeeds() {
    ui::State<double> extent{100.0};
    int commits = 0;
    ui::UI tree{ui::SplitView{extent, ui::Spacer{80.0f,80.0f}, ui::Spacer{80.0f,80.0f}}
        .on_commit([&](double) {
            ++commits;
            if (commits == 1) throw std::runtime_error("split commit failure");
        })};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f});
    tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),platform);
    bool threw = false;
    try { tree.dispatch(test::pointer(ui::InputType::PointerUp,120.5f,40.0f),platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && commits == 1);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,120.5f,40.0f),platform);
    NUI_CHECK(commits == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,120.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,140.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,140.5f,40.0f),platform);
    NUI_CHECK(commits == 2 && extent.get() == 140.0);
}

void bounds_keys_availability_and_invalid_options_are_explicit() {
    ui::State<double> extent{std::numeric_limits<double>::quiet_NaN()};
    ui::State<bool> locked{false};
    ui::State<bool> enabled{true};
    int commits = 0;
    ui::UI tree{ui::Enabled{enabled.binding(), ui::ReadOnly{locked.binding(),
        ui::SplitView{extent, ui::Spacer{80.0f,80.0f}, ui::Spacer{80.0f,80.0f}}
            .on_commit([&](double) { ++commits; })}}};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Home),platform);
    NUI_CHECK(extent.get() == 40.0);
    tree.dispatch(test::key(ui::Key::End),platform);
    NUI_CHECK(extent.get() == 260.0);
    tree.dispatch(test::key(ui::Key::Left,true),platform);
    NUI_CHECK(extent.get() == 160.0);
    locked.set(true);
    tree.dispatch(test::key(ui::Key::Left),platform);
    NUI_CHECK(extent.get() == 160.0);
    locked.set(false);
    enabled.set(false);
    tree.dispatch(test::key(ui::Key::Left),platform);
    NUI_CHECK(extent.get() == 160.0);
    NUI_CHECK(commits == 3);
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity()}) {
        bool threw = false;
        try {
            (void)ui::SplitView{extent,ui::Spacer{1.0f,1.0f},ui::Spacer{1.0f,1.0f}}
                .step(invalid).spec();
        } catch (const std::invalid_argument&) { threw = true; }
        NUI_CHECK(threw);
    }
}

void removed_capture_and_copied_specs_remain_instance_local() {
    ui::State<double> extent{100.0};
    ui::State<bool> present{true};
    int commits = 0;
    auto spec = ui::make_spec(ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_commit([&](double) { ++commits; }));
    ui::UI first{ui::If{present.binding(),ui::Spec{spec}}};
    ui::UI second{ui::Spec{spec}};
    test::MockPlatform first_platform;
    test::MockPlatform second_platform;
    first.resize({301.0f,100.0f});
    second.resize({301.0f,100.0f});
    first.activate(first_platform);
    second.activate(second_platform);
    first.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),first_platform);
    first.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),first_platform);
    second.dispatch(test::pointer(ui::InputType::PointerDown,120.5f,40.0f),second_platform);
    second.dispatch(test::pointer(ui::InputType::PointerMove,130.5f,40.0f),second_platform);
    first.dispatch(test::pointer(ui::InputType::PointerUp,130.5f,40.0f),first_platform);
    NUI_CHECK(commits == 0);
    second.dispatch(test::pointer(ui::InputType::PointerUp,130.5f,40.0f),second_platform);
    NUI_CHECK(commits == 1);
    first.dispatch(test::pointer(ui::InputType::PointerDown,130.5f,40.0f),first_platform);
    first.dispatch(test::pointer(ui::InputType::PointerMove,150.5f,40.0f),first_platform);
    NUI_CHECK(extent.get() == 150.0);
    present.set(false);
    first.resize({302.0f,100.0f});
    first.dispatch(test::pointer(ui::InputType::PointerUp,130.5f,40.0f),first_platform);
    NUI_CHECK(commits == 1 && extent.get() == 150.0);
    NUI_CHECK(first_platform.pointer_capture_begin_count == first_platform.pointer_capture_end_count);
}

void suite() {
    orientations_order_clipping_and_resize_leave_model_authoritative();
    overlapping_grip_wins_pointer_routing_and_commits_once();
    external_writes_cancel_without_stale_rollback_and_reentrant_write_wins();
    throwing_commit_releases_capture_and_next_gesture_succeeds();
    bounds_keys_availability_and_invalid_options_are_explicit();
    removed_capture_and_copied_specs_remain_instance_local();
}

} // namespace

int main() { return test::run("widget_split_view",&suite); }
