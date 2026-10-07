#include "test_support.hpp"
#include <nativeui/tabs.hpp>

#include <memory>
#include <stdexcept>

namespace {
struct PanelState {
    ui::NodeId id{};
    ui::Rect bounds{};
    int mounts{};
    int unmounts{};
    int paints{};
    bool focused{};
};
class PanelComponent final : public ui::Component {
public:
    explicit PanelComponent(std::shared_ptr<PanelState> value) : state_(std::move(value)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {100.0f,40.0f}; }
    void mount(ui::MountContext& context) override { state_->id = context.node_id(); ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void focus_changed(bool focused,ui::FocusContext&) override { state_->focused = focused; }
    void paint(ui::PaintContext& context) const override { state_->bounds = context.bounds(); ++state_->paints; }
private:
    std::shared_ptr<PanelState> state_;
};
class Panel {
public:
    explicit Panel(std::shared_ptr<PanelState> value) : state_(std::move(value)) {}
    ui::Spec spec() && { const auto state = state_; return {[state] { return std::make_unique<PanelComponent>(state); },{}}; }
private:
    std::shared_ptr<PanelState> state_;
};
void render(ui::UI& tree) { ui::HeadlessRenderer renderer{{300.0f,160.0f},1.0f}; NUI_CHECK(renderer.render(tree)); }
void panel_retention_disabled_external_and_unknown_selection() {
    ui::State<int> selected{1};
    auto first = std::make_shared<PanelState>(),last = std::make_shared<PanelState>();
    ui::UI tree{ui::Tabs{selected}.tab(1,"One",Panel{first}).tab(2,"Disabled",Panel{last},false)};
    test::MockPlatform platform;
    tree.resize({300.0f,160.0f}); tree.activate(platform); render(tree);
    const auto first_id = first->id,last_id = last->id;
    NUI_CHECK(first->mounts == 1 && last->mounts == 1 && first->paints > 0 && last->paints == 0);
    selected.set(2); render(tree);
    NUI_CHECK(last->paints > 0 && first->id == first_id && last->id == last_id);
    NUI_CHECK(first->mounts == 1 && last->mounts == 1 && first->unmounts == 0);
    const auto panel = tree.component_semantics(last_id-1);
    NUI_CHECK(panel && panel->role == ui::SemanticRole::TabPanel && panel->name == "Disabled");
    selected.set(99); const auto first_paints = first->paints,last_paints = last->paints;
    render(tree);
    NUI_CHECK(first->paints == first_paints && last->paints == last_paints && selected.get() == 99);
    NUI_CHECK_NEAR(tree.measure().preferred.h,40.0f,0.01f);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(selected.get() == 1);
}
void source_observer_failure_recovers_layout_and_the_next_gesture() {
    ui::State<int> selected{1}; bool fail = true;
    auto subscription = selected.observe([&](int) { if (fail) throw std::runtime_error("tabs first observer"); });
    auto first = std::make_shared<PanelState>(),last = std::make_shared<PanelState>();
    ui::UI tree{ui::Tabs{selected}.tab(1,"One",Panel{first}).tab(2,"Two",Panel{last})};
    test::MockPlatform platform;
    tree.resize({300.0f,160.0f}); tree.activate(platform); render(tree);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,225.0f,10.0f),platform);
    bool threw = false;
    try { tree.dispatch(test::pointer(ui::InputType::PointerUp,225.0f,10.0f),platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && selected.get() == 2);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
    fail = false; render(tree);
    NUI_CHECK(last->paints > 0);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,25.0f,10.0f),platform);
    NUI_CHECK(selected.get() == 2);
    tree.dispatch(test::key(ui::Key::Home),platform); NUI_CHECK(selected.get() == 1);
}
struct EqualityFault { bool fail{}; };
struct CustomKey {
    int value{};
    std::shared_ptr<EqualityFault> fault;
    bool operator==(const CustomKey& other) const {
        if (fault && fault->fail) throw std::runtime_error("tabs user equality");
        return value == other.value;
    }
};
void user_types_and_equality_failure_leave_no_half_selected_panel() {
    auto fault = std::make_shared<EqualityFault>();
    ui::State<CustomKey> selected{{1,fault}};
    auto first = std::make_shared<PanelState>(),last = std::make_shared<PanelState>();
    ui::UI tree{ui::Tabs{selected.binding()}.tab(CustomKey{1,fault},"One",Panel{first})
        .tab(CustomKey{2,fault},"Two",Panel{last})};
    test::MockPlatform platform;
    tree.resize({300.0f,160.0f}); tree.activate(platform); render(tree);
    fault->fail = true;
    bool threw = false;
    try { tree.dispatch(test::key(ui::Key::Right),platform); } catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && selected.get().value == 1 && last->paints == 0);
    fault->fail = false;
    tree.dispatch(test::key(ui::Key::Right),platform); render(tree);
    NUI_CHECK(selected.get().value == 2 && last->paints > 0 && first->mounts == 1 && last->mounts == 1);
    bool duplicate = false;
    try { (void)ui::Tabs{selected}.tab(CustomKey{1,fault},"One",ui::Spacer{0.0f})
        .tab(CustomKey{1,fault},"Duplicate",ui::Spacer{0.0f}); } catch (const std::invalid_argument&) { duplicate = true; }
    NUI_CHECK(duplicate);
}
void copied_recipe_keeps_contacts_isolated_and_expired_binding_never_writes() {
    auto source = std::make_unique<ui::State<int>>(1); const auto binding = source->binding();
    auto recipe = ui::Tabs{binding}.tab(1,"One",ui::Spacer{100.0f,40.0f})
        .tab(2,"Two",ui::Spacer{100.0f,40.0f}).spec();
    ui::UI first{recipe},second{recipe}; test::MockPlatform a,b;
    first.resize({300.0f,160.0f}); second.resize({300.0f,160.0f}); first.activate(a); second.activate(b);
    first.dispatch(test::pointer(ui::InputType::PointerDown,225.0f,10.0f),a);
    second.dispatch(test::pointer(ui::InputType::PointerUp,225.0f,10.0f),b);
    NUI_CHECK(source->get() == 1 && a.pointer_capture_end_count == 0);
    first.dispatch(test::pointer(ui::InputType::PointerUp,225.0f,10.0f),a); NUI_CHECK(source->get() == 2);
    source.reset(); NUI_CHECK(!binding.valid());
    first.dispatch(test::key(ui::Key::Home),a);
    first.dispatch(test::pointer(ui::InputType::PointerDown,25.0f,10.0f),a);
    first.dispatch(test::pointer(ui::InputType::PointerUp,25.0f,10.0f),a);
    NUI_CHECK(binding.get() == 2 && a.pointer_capture_begin_count == a.pointer_capture_end_count);
    render(first); render(second);
    ui::State<int> empty_selection{42}; ui::UI empty{ui::Tabs{empty_selection}};
    empty.resize({300.0f,160.0f}); render(empty); NUI_CHECK(empty_selection.get() == 42);
}
void suite() {
    panel_retention_disabled_external_and_unknown_selection();
    source_observer_failure_recovers_layout_and_the_next_gesture();
    user_types_and_equality_failure_leave_no_half_selected_panel();
    copied_recipe_keeps_contacts_isolated_and_expired_binding_never_writes();
}
}
int main() { return test::run("widget_tabs_extraction",&suite); }
