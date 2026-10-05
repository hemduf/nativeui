#include "test_support.hpp"
#include <nativeui/split_view.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/visibility.hpp>

#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
struct CommitFault {
    bool copy_throws{};
    int calls{};
};
struct CommitCallback {
    std::shared_ptr<CommitFault> state;
    explicit CommitCallback(std::shared_ptr<CommitFault> value) : state(std::move(value)) {}
    CommitCallback(const CommitCallback& other) : state(other.state) {
        if (state->copy_throws) throw std::runtime_error("split callback copy");
    }
    void operator()(double) const { ++state->calls; }
};
void copy_failure_on_up_disarms_before_fallible_callback_copy() {
    ui::State<double> extent{100.0};
    auto fault = std::make_shared<CommitFault>();
    ui::UI tree{ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_commit(CommitCallback{fault})};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),platform);
    fault->copy_throws = true;
    bool threw = false;
    try { tree.dispatch(test::pointer(ui::InputType::PointerUp,120.5f,40.0f),platform); }
    catch (const std::runtime_error&) { threw = true; }
    fault->copy_throws = false;
    NUI_CHECK(threw && fault->calls == 0);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,160.5f,40.0f),platform);
    NUI_CHECK(extent.get() == 120.0); // No stale drag without a new Down.
    tree.dispatch(test::pointer(ui::InputType::PointerDown,120.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,140.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,140.5f,40.0f),platform);
    NUI_CHECK(fault->calls == 1 && extent.get() == 140.0);
}
void key_change_cannot_commit_a_reentrant_drag() {
    ui::State<double> extent{100.0};
    ui::UI* owner = nullptr;
    test::MockPlatform platform;
    bool nested = false;
    int commits = 0;
    ui::UI tree{ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_change([&](double value) {
            if (nested) return;
            nested = true;
            owner->dispatch(test::pointer(ui::InputType::PointerDown,static_cast<float>(value)+0.5f,40.0f),platform);
            owner->dispatch(test::pointer(ui::InputType::PointerMove,static_cast<float>(value)+20.5f,40.0f),platform);
        })
        .on_commit([&](double) { ++commits; })};
    owner = &tree;
    tree.resize({301.0f,100.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(extent.get() == 130.0 && commits == 0);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,130.5f,40.0f),platform);
    NUI_CHECK(commits == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}
void key_change_that_removes_split_cannot_start_an_old_commit() {
    ui::State<double> extent{100.0};
    ui::State<bool> present{true};
    ui::UI* owner = nullptr;
    int commits = 0;
    ui::UI tree{ui::If{present,ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_change([&](double) { present.set(false); owner->resize({302.0f,100.0f}); })
        .on_commit([&](double) { ++commits; })}};
    owner = &tree;
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(!present.get() && extent.get() == 110.0 && commits == 0);
    present.set(true); tree.resize({301.0f,100.0f});
}
struct PaneObservation { ui::Rect bounds{}; };
class ObservedPaneComponent final : public ui::Component {
public:
    explicit ObservedPaneComponent(std::shared_ptr<PaneObservation> state) : state_(std::move(state)) {}
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {80.0f,80.0f}; }
    void paint(ui::PaintContext& context) const override { state_->bounds = context.bounds(); }
private:
    std::shared_ptr<PaneObservation> state_;
};
class ObservedPane {
public:
    explicit ObservedPane(std::shared_ptr<PaneObservation> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        const auto state = state_;
        return {[state] { return std::make_unique<ObservedPaneComponent>(state); },{}};
    }
private:
    std::shared_ptr<PaneObservation> state_;
};
void observer_registered_first_can_throw_after_write_without_stale_geometry() {
    ui::State<double> extent{100.0};
    bool fail = true;
    auto subscription = extent.observe([&](double) {
        if (fail) throw std::runtime_error("first external observer");
    });
    auto first = std::make_shared<PaneObservation>();
    int commits = 0;
    ui::UI tree{ui::SplitView{extent,ObservedPane{first},ui::Spacer{80.0f,80.0f}}
        .on_commit([&](double) { ++commits; })};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),platform);
    bool threw = false;
    try { tree.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && extent.get() == 120.0 && commits == 0);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    ui::HeadlessRenderer renderer{{301.0f,100.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK_NEAR(first->bounds.w,120.0f,0.001f);
    fail = false;
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(extent.get() == 130.0 && commits == 1);
}
void source_destroyed_by_an_earlier_observer_suppresses_remaining_callbacks() {
    auto source = std::make_unique<ui::State<double>>(100.0);
    const auto binding = source->binding();
    auto subscription = source->observe([&](double) { source.reset(); });
    int changes = 0,commits = 0;
    ui::UI tree{ui::SplitView{binding,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_change([&](double) { ++changes; }).on_commit([&](double) { ++commits; })};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),platform);
    NUI_CHECK(!source && !binding.valid() && binding.get() == 120.0);
    NUI_CHECK(changes == 0 && commits == 0);
    NUI_CHECK(platform.pointer_capture_end_count == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,120.5f,40.0f),platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(binding.get() == 120.0 && changes == 0 && commits == 0);
}
struct SourceCopyFault {
    std::unique_ptr<ui::State<double>>* source{};
    bool retire_on_copy{};
    int calls{};
};
struct SourceCopyCallback {
    std::shared_ptr<SourceCopyFault> fault;
    explicit SourceCopyCallback(std::shared_ptr<SourceCopyFault> value) : fault(std::move(value)) {}
    SourceCopyCallback(const SourceCopyCallback& other) : fault(other.fault) {
        if (fault->retire_on_copy) fault->source->reset();
    }
    void operator()(double) const { ++fault->calls; }
};
void source_destroyed_during_callback_copy_suppresses_the_not_started_callback() {
    for (bool commit : {false,true}) {
        auto source = std::make_unique<ui::State<double>>(100.0);
        const auto binding = source->binding();
        auto fault = std::make_shared<SourceCopyFault>(); fault->source = &source;
        auto builder = ui::SplitView{binding,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}};
        if (commit) (void)std::move(builder).on_commit(SourceCopyCallback{fault});
        else (void)std::move(builder).on_change(SourceCopyCallback{fault});
        ui::UI tree{std::move(builder)};
        test::MockPlatform platform;
        tree.resize({301.0f,100.0f}); tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown,100.5f,40.0f),platform);
        if (!commit) fault->retire_on_copy = true;
        tree.dispatch(test::pointer(ui::InputType::PointerMove,120.5f,40.0f),platform);
        if (commit) {
            fault->retire_on_copy = true;
            tree.dispatch(test::pointer(ui::InputType::PointerUp,120.5f,40.0f),platform);
        }
        NUI_CHECK(!source && !binding.valid() && fault->calls == 0);
        NUI_CHECK(platform.pointer_capture_end_count == 1);
    }
}
void keyboard_write_preserves_source_replaced_at_exposure() {
    ui::State<double> extent{100.0};
    int changes = 0, commits = 0;
    ui::UI tree{ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
        .on_change([&](double) { ++changes; }).on_commit([&](double) { ++commits; })};
    test::MockPlatform platform;
    tree.resize({301.0f,100.0f}); tree.activate(platform);
    ui::HeadlessRenderer renderer{{301.0f,100.0f},1};
    NUI_CHECK(renderer.render(tree));
    bool armed = false;
    tree.set_invalidation_callback([&](ui::Rect) {
        if (std::exchange(armed,false)) extent.set(150.0);
    });
    armed = true;
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(!armed && extent.get() == 150.0 && changes == 0 && commits == 0);
    tree.clear_invalidation_callback();
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(extent.get() == 160.0 && changes == 1 && commits == 1);
    tree.deactivate(platform);
}
void keyboard_write_rechecks_permission_at_exposure() {
    for (bool disable : {false,true}) {
        ui::State<double> extent{100.0};
        ui::State<bool> enabled{true}, read_only{false};
        int changes = 0, commits = 0;
        ui::UI tree{ui::Enabled{enabled,ui::ReadOnly{read_only,
            ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
                .on_change([&](double) { ++changes; }).on_commit([&](double) { ++commits; })}}};
        test::MockPlatform platform;
        tree.resize({301.0f,100.0f}); tree.activate(platform);
        ui::HeadlessRenderer renderer{{301.0f,100.0f},1};
        NUI_CHECK(renderer.render(tree));
        bool armed = false;
        tree.set_invalidation_callback([&](ui::Rect) {
            if (!std::exchange(armed,false)) return;
            if (disable) enabled.set(false);
            else read_only.set(true);
        });
        armed = true;
        tree.dispatch(test::key(ui::Key::Right),platform);
        NUI_CHECK(!armed && extent.get() == 100.0 && changes == 0 && commits == 0);
        tree.clear_invalidation_callback();
        enabled.set(true); read_only.set(false);
        tree.dispatch(test::key(ui::Key::Right),platform);
        NUI_CHECK(extent.get() == 110.0 && changes == 1 && commits == 1);
        tree.deactivate(platform);
    }
}
struct ExposureCopyFault {
    bool armed{};
    int calls{};
    std::function<void()> expose;
};
struct ExposureCopyCallback {
    std::shared_ptr<ExposureCopyFault> fault;
    explicit ExposureCopyCallback(std::shared_ptr<ExposureCopyFault> value) : fault(std::move(value)) {}
    ExposureCopyCallback(const ExposureCopyCallback& other) : fault(other.fault) {
        if (std::exchange(fault->armed,false)) fault->expose();
    }
    void operator()(double) const { ++fault->calls; }
};
void change_callback_copy_rechecks_authoritative_source_and_permission() {
    for (bool revoke_permission : {false,true}) {
        ui::State<double> extent{100.0};
        ui::State<bool> read_only{false};
        auto fault = std::make_shared<ExposureCopyFault>();
        fault->expose = [&] {
            if (revoke_permission) read_only.set(true);
            else extent.set(150.0);
        };
        int commits = 0;
        ui::UI tree{ui::ReadOnly{read_only,
            ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
                .on_change(ExposureCopyCallback{fault}).on_commit([&](double) { ++commits; })}};
        test::MockPlatform platform;
        tree.resize({301.0f,100.0f}); tree.activate(platform);
        fault->armed = true;
        tree.dispatch(test::key(ui::Key::Right),platform);
        NUI_CHECK(extent.get() == (revoke_permission ? 110.0 : 150.0));
        NUI_CHECK(fault->calls == 0 && commits == 0);
        read_only.set(false);
        tree.dispatch(test::key(ui::Key::Right),platform);
        NUI_CHECK(extent.get() == (revoke_permission ? 120.0 : 160.0));
        NUI_CHECK(fault->calls == 1 && commits == 1);
        tree.deactivate(platform);
    }
}
struct SplitLayoutFault { bool armed{}; };
class SplitLayoutFaultComponent final : public ui::Component {
public:
    explicit SplitLayoutFaultComponent(std::shared_ptr<SplitLayoutFault> fault)
        : fault_(std::move(fault)) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {1,1}; }
    bool pointer_targetable() const noexcept override { return false; }
    void layout_children(ui::Rect, const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>&) const override {
        if (fault_->armed) throw std::runtime_error("split neighbour layout");
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<SplitLayoutFault> fault_;
};
void failed_geometry_is_discarded_when_collapsed_layout_skips_preparation() {
    ui::State<double> extent{200.0};
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    auto fault = std::make_shared<SplitLayoutFault>();
    int changes = 0, commits = 0;
    auto neighbour = ui::Spec{[fault] {
        return std::make_unique<SplitLayoutFaultComponent>(fault);
    },{}};
    ui::UI tree{ui::Stack{ui::Visibility{visibility,
        ui::SplitView{extent,ui::Spacer{80.0f,80.0f},ui::Spacer{80.0f,80.0f}}
            .on_change([&](double) { ++changes; }).on_commit([&](double) { ++commits; })},
        std::move(neighbour)}};
    test::MockPlatform platform;
    tree.resize({600.0f,100.0f}); tree.activate(platform);
    ui::NodeId handle = ui::kInvalidNodeId;
    for (ui::NodeId id = 1; id < 64; ++id) {
        const auto info = tree.component_semantics(id);
        if (info && info->name == "Splitter") handle = id;
    }
    NUI_CHECK(handle != ui::kInvalidNodeId);
    const auto initial = tree.component_semantics(handle);
    NUI_CHECK(initial && initial->numeric_value == 200.0);

    fault->armed = true;
    bool caught = false;
    try { tree.resize({100.0f,100.0f}); }
    catch (const std::runtime_error& error) {
        caught = std::string_view{error.what()} == "split neighbour layout";
    }
    NUI_CHECK(caught);
    const auto rolled_back = tree.component_semantics(handle);
    NUI_CHECK(rolled_back && rolled_back->numeric_value == 200.0);
    fault->armed = false;
    visibility.set(ui::VisibilityMode::Collapsed);
    tree.resize({100.0f,100.0f});
    const auto skipped = tree.component_semantics(handle);
    // Collapsed layout commits retained zero bounds without preparing geometry.
    // It must not publish the width-100 candidate from the failed transaction.
    NUI_CHECK(skipped && skipped->numeric_value == 200.0);
    NUI_CHECK(extent.get() == 200.0 && changes == 0 && commits == 0);

    visibility.set(ui::VisibilityMode::Visible);
    tree.resize({100.0f,100.0f});
    const auto narrow = tree.component_semantics(handle);
    NUI_CHECK(narrow && narrow->numeric_value == 59.0);
    NUI_CHECK(extent.get() == 200.0 && changes == 0 && commits == 0);
    tree.resize({301.0f,100.0f});
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(extent.get() == 210.0 && changes == 1 && commits == 1);
    tree.deactivate(platform);
}
void suite() {
    failed_geometry_is_discarded_when_collapsed_layout_skips_preparation();
    keyboard_write_preserves_source_replaced_at_exposure();
    keyboard_write_rechecks_permission_at_exposure();
    change_callback_copy_rechecks_authoritative_source_and_permission();
    copy_failure_on_up_disarms_before_fallible_callback_copy();
    key_change_cannot_commit_a_reentrant_drag();
    key_change_that_removes_split_cannot_start_an_old_commit();
    observer_registered_first_can_throw_after_write_without_stale_geometry();
    source_destroyed_by_an_earlier_observer_suppresses_remaining_callbacks();
    source_destroyed_during_callback_copy_suppresses_the_not_started_callback();
}
}
int main(int argc,char** argv) {
    if (argc == 2) {
        const std::string_view selected = argv[1];
        if (selected == "collapsed_geometry") return test::run("split_collapsed_geometry",&failed_geometry_is_discarded_when_collapsed_layout_skips_preparation);
        if (selected == "change_copy_exposure") return test::run("split_change_copy_exposure",&change_callback_copy_rechecks_authoritative_source_and_permission);
        if (selected == "source_exposure") return test::run("split_source_exposure",&keyboard_write_preserves_source_replaced_at_exposure);
        if (selected == "permission_exposure") return test::run("split_permission_exposure",&keyboard_write_rechecks_permission_at_exposure);
        if (selected == "copy") return test::run("split_callback_copy",&copy_failure_on_up_disarms_before_fallible_callback_copy);
        if (selected == "nested") return test::run("split_callback_nested",&key_change_cannot_commit_a_reentrant_drag);
        if (selected == "unmount") return test::run("split_callback_unmount",&key_change_that_removes_split_cannot_start_an_old_commit);
        if (selected == "retire") return test::run("split_source_retire",&source_destroyed_by_an_earlier_observer_suppresses_remaining_callbacks);
        if (selected == "retire_copy") return test::run("split_source_retire_copy",&source_destroyed_during_callback_copy_suppresses_the_not_started_callback);
        if (selected == "observer") return test::run("split_callback_observer",&observer_registered_first_can_throw_after_write_without_stale_geometry);
    }
    return test::run("split_callback_faults",&suite);
}
