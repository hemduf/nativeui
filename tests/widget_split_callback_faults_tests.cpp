#include "test_support.hpp"
#include <nativeui/split_view.hpp>

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
void suite() {
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
        if (selected == "copy") return test::run("split_callback_copy",&copy_failure_on_up_disarms_before_fallible_callback_copy);
        if (selected == "nested") return test::run("split_callback_nested",&key_change_cannot_commit_a_reentrant_drag);
        if (selected == "unmount") return test::run("split_callback_unmount",&key_change_that_removes_split_cannot_start_an_old_commit);
        if (selected == "retire") return test::run("split_source_retire",&source_destroyed_by_an_earlier_observer_suppresses_remaining_callbacks);
        if (selected == "retire_copy") return test::run("split_source_retire_copy",&source_destroyed_during_callback_copy_suppresses_the_not_started_callback);
        if (selected == "observer") return test::run("split_callback_observer",&observer_registered_first_can_throw_after_write_without_stale_geometry);
    }
    return test::run("split_callback_faults",&suite);
}
