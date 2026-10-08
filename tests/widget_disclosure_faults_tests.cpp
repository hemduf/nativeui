#include "test_support.hpp"
#include <nativeui/collapsible.hpp>
#include <nativeui/accordion.hpp>
#include "../src/detail/disclosure_kernel.hpp"

#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
struct ContentState {
    ui::NodeId id{};
    ui::Rect bounds{};
    bool focused{};
    bool fail_blur{};
    int mounts{};
    int unmounts{};
    int paints{};
};
class ContentComponent final : public ui::Component {
public:
    explicit ContentComponent(std::shared_ptr<ContentState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {100.0f,60.0f}; }
    void mount(ui::MountContext& context) override { state_->id = context.node_id(); ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void focus_changed(bool focused,ui::FocusContext&) override {
        state_->focused = focused;
        if (!focused && state_->fail_blur) throw std::runtime_error("disclosure blur");
    }
    void paint(ui::PaintContext& context) const override { state_->bounds = context.bounds(); ++state_->paints; }
private:
    std::shared_ptr<ContentState> state_;
};
class Content {
public:
    explicit Content(std::shared_ptr<ContentState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        const auto state = state_;
        return {[state] { return std::make_unique<ContentComponent>(state); },{}};
    }
private:
    std::shared_ptr<ContentState> state_;
};
void render(ui::UI& tree,ui::Size size={320.0f,240.0f}) {
    ui::HeadlessRenderer renderer{size,1.0f}; NUI_CHECK(renderer.render(tree));
}
void toggle(ui::UI& tree,test::MockPlatform& platform) {
    tree.dispatch(test::key(ui::Key::Space),platform);
    auto up = test::key(ui::Key::Space); up.type = ui::InputType::KeyUp; tree.dispatch(up,platform);
}
void collapsible_reads_external_commit_after_preceding_observer_failure() {
    ui::State<bool> open{false};
    bool fail = true;
    auto subscription = open.observe([&](bool) { if (fail) throw std::runtime_error("first bool observer"); });
    auto content = std::make_shared<ContentState>();
    int changes = 0;
    ui::UI tree{ui::Collapsible{"Source",open,Content{content}}
        .style(ui::CollapsibleStyle{.reduced_motion=true}).on_change([&](bool) { ++changes; })};
    test::MockPlatform platform;
    tree.resize({320.0f,240.0f}); tree.activate(platform); render(tree);
    const auto header_id = content->id-2;
    bool threw = false;
    try { open.set(true); } catch (const std::runtime_error&) { threw = true; }
    fail = false;
    NUI_CHECK(threw && open.get());
    render(tree);
    const auto semantic = tree.component_semantics(header_id);
    NUI_CHECK(semantic && semantic->expanded == ui::SemanticExpandedState::Expanded);
    NUI_CHECK(content->paints > 0 && changes == 0);
    toggle(tree,platform);
    NUI_CHECK(!open.get() && changes == 1);
}
void accordion_reads_external_commit_after_preceding_observer_failure() {
    ui::State<std::vector<std::string>> keys{{"a"}};
    bool fail = true;
    auto subscription = keys.observe([&](const auto&) { if (fail) throw std::runtime_error("first keys observer"); });
    auto a = std::make_shared<ContentState>(),c = std::make_shared<ContentState>();
    int changes = 0;
    ui::UI tree{ui::Accordion{keys}.mode(ui::AccordionMode::Single)
        .style(ui::AccordionStyle{.section=ui::CollapsibleStyle{.reduced_motion=true}})
        .section("a","A",Content{a}).section("c","C",Content{c})
        .on_change([&](const auto&) { ++changes; })};
    test::MockPlatform platform;
    tree.resize({320.0f,240.0f}); tree.activate(platform); render(tree);
    const auto a_header = a->id-2,c_header = c->id-2;
    const auto old_c_paints = c->paints;
    bool threw = false;
    try { keys.set({"c"}); } catch (const std::runtime_error&) { threw = true; }
    fail = false;
    NUI_CHECK(threw && keys.get() == std::vector<std::string>{"c"});
    render(tree);
    const auto a_semantic = tree.component_semantics(a_header),c_semantic = tree.component_semantics(c_header);
    NUI_CHECK(a_semantic && a_semantic->expanded == ui::SemanticExpandedState::Collapsed);
    NUI_CHECK(c_semantic && c_semantic->expanded == ui::SemanticExpandedState::Expanded);
    NUI_CHECK(c->paints > old_c_paints && changes == 0);
    toggle(tree,platform);
    NUI_CHECK(keys.get() == std::vector<std::string>{"a"} && changes == 1);
}
void exclusive_blur_failure_retains_unstarted_transitions_for_recovery() {
    ui::State<std::vector<std::string>> keys{{"a"}};
    auto a = std::make_shared<ContentState>(),c = std::make_shared<ContentState>();
    ui::UI tree{ui::Accordion{keys}.mode(ui::AccordionMode::Single)
        .content_policy(ui::DisclosureContentPolicy::UnmountWhenClosed)
        .style(ui::AccordionStyle{.section=ui::CollapsibleStyle{.reduced_motion=true}})
        .section("a","A",Content{a}).section("c","C",Content{c})};
    test::MockPlatform platform;
    tree.resize({320.0f,240.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(a->focused && c->mounts == 0);
    a->fail_blur = true;
    bool threw = false;
    try { keys.set({"c"}); } catch (const std::runtime_error&) { threw = true; }
    a->fail_blur = false;
    NUI_CHECK(threw && keys.get() == std::vector<std::string>{"c"});
    tree.resize({320.0f,240.0f}); render(tree);
    NUI_CHECK(c->mounts == 1 && c->paints > 0);
    NUI_CHECK(!a->focused && a->unmounts == 1);
    const auto semantic = tree.component_semantics(c->id-2);
    NUI_CHECK(semantic && semantic->expanded == ui::SemanticExpandedState::Expanded);
}
void hover_leave_restores_background_when_pointer_moves_to_neighbor() {
    ui::State<bool> open{false};
    auto content = std::make_shared<ContentState>(),neighbor = std::make_shared<ContentState>();
    ui::CollapsibleStyle style;
    style.background = ui::Color{1.0f,0.0f,0.0f,1.0f};
    style.hover_background = ui::Color{0.0f,1.0f,0.0f,1.0f};
    style.corner_radius = 0.0f;
    style.reduced_motion = true;
    ui::UI tree{ui::Row{ui::Collapsible{"Hover",open,Content{content}}.style(style),Content{neighbor}}
        .gap(0.0f)};
    test::MockPlatform platform;
    tree.resize({320.0f,80.0f}); tree.activate(platform);
    ui::HeadlessRenderer renderer{{320.0f,80.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    tree.dispatch(test::pointer(ui::InputType::PointerMove,5.0f,10.0f),platform);
    NUI_CHECK(renderer.render(tree));
    auto pixel = renderer.pixel(5,10);
    NUI_CHECK(pixel.g == 255 && pixel.r == 0);
    tree.dispatch(test::pointer(ui::InputType::PointerMove,neighbor->bounds.x+5.0f,10.0f),platform);
    NUI_CHECK(renderer.render(tree));
    pixel = renderer.pixel(5,10);
    NUI_CHECK(pixel.r == 255 && pixel.g == 0);
}
void release_invalidation_that_removes_header_cannot_toggle_retired_model(bool pointer) {
    ui::State<bool> present{true},open{false};
    auto content = std::make_shared<ContentState>();
    ui::UI tree{ui::If{present,ui::Collapsible{"Lifetime",open,Content{content}}
        .style(ui::CollapsibleStyle{.reduced_motion=true})}};
    test::MockPlatform platform;
    tree.resize({320.0f,160.0f}); tree.activate(platform);
    bool remove = false;
    tree.set_invalidation_callback([&] {
        if (!remove) return;
        remove = false;
        present.set(false);
        tree.resize({321.0f,160.0f});
    });
    if (pointer) tree.dispatch(test::pointer(ui::InputType::PointerDown,5.0f,10.0f),platform);
    else tree.dispatch(test::key(ui::Key::Space),platform);
    render(tree,{320.0f,160.0f});
    remove = true;
    if (pointer) tree.dispatch(test::pointer(ui::InputType::PointerUp,5.0f,10.0f),platform);
    else {
        auto up = test::key(ui::Key::Space); up.type = ui::InputType::KeyUp;
        tree.dispatch(up,platform);
    }
    NUI_CHECK(!present.get() && !open.get() && content->unmounts == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}
void key_release_retired_header() { release_invalidation_that_removes_header_cannot_toggle_retired_model(false); }
void pointer_release_retired_header() { release_invalidation_that_removes_header_cannot_toggle_retired_model(true); }
struct SetterCopyFault {
    bool fail{};
    int calls{};
    ui::State<bool>* source{};
};
struct ThrowingDisclosureSetter {
    explicit ThrowingDisclosureSetter(std::shared_ptr<SetterCopyFault> value) : fault(std::move(value)) {}
    ThrowingDisclosureSetter(const ThrowingDisclosureSetter& other) : fault(other.fault) {
        if (fault->fail) throw std::runtime_error("disclosure setter copy");
    }
    ThrowingDisclosureSetter(ThrowingDisclosureSetter&&) noexcept = default;
    void operator()(bool next) const { ++fault->calls; fault->source->set(next); }
    std::shared_ptr<SetterCopyFault> fault;
};
void setter_copy_failure_disarms_terminal_release_before_throw(bool pointer) {
    ui::State<bool> open{false};
    auto content = std::make_shared<ContentState>();
    auto state = std::make_shared<ui::detail::DisclosureState>();
    state->title = "Private setter fault"; state->style.reduced_motion = true;
    state->can_write = [] { return true; };
    auto fault = std::make_shared<SetterCopyFault>(); fault->source = &open;
    state->set_open = ThrowingDisclosureSetter{fault};
    const auto child = std::make_shared<const ui::Spec>(Content{content}.spec());
    ui::UI tree{ui::detail::disclosure_spec(state,child,{})};
    test::MockPlatform platform;
    tree.resize({320.0f,120.0f}); tree.activate(platform);
    const auto down = pointer ? test::pointer(ui::InputType::PointerDown,5.0f,10.0f) : test::key(ui::Key::Space);
    auto up = pointer ? test::pointer(ui::InputType::PointerUp,5.0f,10.0f) : test::key(ui::Key::Space);
    if (!pointer) up.type = ui::InputType::KeyUp;
    tree.dispatch(down,platform);
    fault->fail = true;
    bool threw = false;
    try { tree.dispatch(up,platform); } catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && !open.get() && fault->calls == 0);
    fault->fail = false;
    tree.dispatch(up,platform);
    NUI_CHECK(!open.get() && fault->calls == 0);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
    tree.dispatch(down,platform); tree.dispatch(up,platform);
    NUI_CHECK(open.get() && fault->calls == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}
void key_setter_copy_failure() { setter_copy_failure_disarms_terminal_release_before_throw(false); }
void pointer_setter_copy_failure() { setter_copy_failure_disarms_terminal_release_before_throw(true); }
void suite() {
    collapsible_reads_external_commit_after_preceding_observer_failure();
    accordion_reads_external_commit_after_preceding_observer_failure();
    exclusive_blur_failure_retains_unstarted_transitions_for_recovery();
    hover_leave_restores_background_when_pointer_moves_to_neighbor();
    key_release_retired_header(); pointer_release_retired_header();
    key_setter_copy_failure(); pointer_setter_copy_failure();
}
}
int main(int argc,char** argv) {
    if (argc == 2) {
        const std::string_view arg = argv[1];
        if (arg == "bool") return test::run("disclosure_bool_observer",&collapsible_reads_external_commit_after_preceding_observer_failure);
        if (arg == "keys") return test::run("disclosure_keys_observer",&accordion_reads_external_commit_after_preceding_observer_failure);
        if (arg == "blur") return test::run("disclosure_blur_recovery",&exclusive_blur_failure_retains_unstarted_transitions_for_recovery);
        if (arg == "hover") return test::run("disclosure_hover_leave",&hover_leave_restores_background_when_pointer_moves_to_neighbor);
        if (arg == "key") return test::run("disclosure_key_lifetime",&key_release_retired_header);
        if (arg == "pointer") return test::run("disclosure_pointer_lifetime",&pointer_release_retired_header);
        if (arg == "copy_key") return test::run("disclosure_key_setter_copy",&key_setter_copy_failure);
        if (arg == "copy_pointer") return test::run("disclosure_pointer_setter_copy",&pointer_setter_copy_failure);
    }
    return test::run("widget_disclosure_faults",&suite);
}
