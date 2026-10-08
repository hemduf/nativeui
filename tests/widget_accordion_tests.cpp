#include "test_support.hpp"
#include <nativeui/accordion.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;

struct PanelObservation {
    ui::Rect bounds{};
    bool focused{};
    int focus_gains{};
    int focus_losses{};
    int paints{};
};
class PanelComponent final : public ui::Component {
public:
    explicit PanelComponent(std::shared_ptr<PanelObservation> value) : state_(std::move(value)) {}
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f,60.0f};
    }
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    void focus_changed(bool value, ui::FocusContext&) override {
        state_->focused = value;
        if (value) ++state_->focus_gains; else ++state_->focus_losses;
    }
    void paint(ui::PaintContext& context) const override {
        ++state_->paints;
        state_->bounds = context.bounds();
    }
private:
    std::shared_ptr<PanelObservation> state_;
};
class Panel {
public:
    explicit Panel(std::shared_ptr<PanelObservation> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = state_;
        return {[state] { return std::make_unique<PanelComponent>(state); }, {}};
    }
private:
    std::shared_ptr<PanelObservation> state_;
};

void toggle(ui::UI& tree, test::MockPlatform& platform) {
    tree.dispatch(test::key(ui::Key::Space),platform);
    auto up = test::key(ui::Key::Space);
    up.type = ui::InputType::KeyUp;
    tree.dispatch(up,platform);
}

void multiple_and_single_policies_publish_one_canonical_vector() {
    ui::State<std::vector<std::string>> keys{{"unknown","c","a","c"}};
    int writes = 0;
    int callbacks = 0;
    auto observer = keys.observe([&](const auto&) { ++writes; });
    auto a = std::make_shared<PanelObservation>();
    auto c = std::make_shared<PanelObservation>();
    ui::UI tree{ui::Accordion{keys}.mode(ui::AccordionMode::Single)
        .section("a","A",Panel{a}).section("c","C",Panel{c})
        .on_change([&](const auto& value) { NUI_CHECK(value == keys.get()); ++callbacks; })};
    tree.resize({240.0f,240.0f});
    test::MockPlatform platform;
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{240.0f,240.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(a->paints > 0 && c->paints == 0);
    NUI_CHECK(keys.get() == std::vector<std::string>({"unknown","c","a","c"}));
    NUI_CHECK(writes == 0 && callbacks == 0);
    toggle(tree,platform);
    NUI_CHECK(keys.get().empty());
    NUI_CHECK(writes == 1 && callbacks == 1);
    tree.dispatch(test::key(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"c"}));
    NUI_CHECK(writes == 2 && callbacks == 2);

    ui::State<std::vector<std::string>> many{{"unknown","c","a","c"}};
    int many_callbacks = 0;
    ui::UI multiple{ui::Accordion{many}
        .section("a","A",ui::Spacer{120.0f,60.0f})
        .section("c","C",ui::Spacer{120.0f,60.0f})
        .on_change([&](const auto&) { ++many_callbacks; })};
    test::MockPlatform multiple_platform;
    multiple.resize({240.0f,240.0f});
    multiple.activate(multiple_platform);
    toggle(multiple,multiple_platform);
    NUI_CHECK(many.get() == std::vector<std::string>({"c"}) && many_callbacks == 1);
    toggle(multiple,multiple_platform);
    NUI_CHECK(many.get() == std::vector<std::string>({"a","c"}) && many_callbacks == 2);
}

void roving_navigation_skips_disabled_and_left_right_keep_disclosure_behavior() {
    ui::State<std::vector<std::string>> keys{{}};
    auto c = std::make_shared<PanelObservation>();
    int outside = 0;
    ui::UI tree{ui::Column{
        ui::Accordion{keys}.section("a","A",ui::Spacer{120.0f,60.0f})
            .section("b","B",ui::Spacer{120.0f,60.0f},false)
            .section("c","C",Panel{c}), ui::Button{"Outside",[&] { ++outside; }}}
        .padding(0.0f).gap(5.0f)};
    test::MockPlatform platform;
    tree.resize({240.0f,300.0f});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"c"}));
    tree.dispatch(test::key(ui::Key::Left),platform);
    NUI_CHECK(keys.get().empty());
    tree.dispatch(test::key(ui::Key::Home),platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"a"}));
    tree.dispatch(test::key(ui::Key::End),platform);
    toggle(tree,platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"a","c"}));
    tree.dispatch(test::key(ui::Key::Up),platform);
    toggle(tree,platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"c"}));
    tree.dispatch(test::key(ui::Key::Down),platform);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(c->focused);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    toggle(tree,platform);
    NUI_CHECK(outside == 1); // Remaining headers are not extra Tab stops.
    tree.dispatch(test::key(ui::Key::Tab,true),platform);
    NUI_CHECK(c->focused);
    tree.dispatch(test::key(ui::Key::Tab,true),platform);
    toggle(tree,platform);
    NUI_CHECK(keys.get().empty());
}

void exclusive_external_change_restores_the_header_of_the_closed_panel() {
    ui::State<std::vector<std::string>> keys{{"a"}};
    int callbacks = 0;
    auto a = std::make_shared<PanelObservation>();
    auto c = std::make_shared<PanelObservation>();
    ui::UI tree{ui::Accordion{keys}.mode(ui::AccordionMode::Single)
        .section("a","A",Panel{a}).section("c","C",Panel{c})
        .on_change([&](const auto&) { ++callbacks; })};
    test::MockPlatform platform;
    tree.resize({240.0f,240.0f});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(a->focused);
    keys.set({"c"});
    tree.resize({240.0f,240.0f});
    NUI_CHECK(!a->focused && !c->focused && a->focus_losses == 1);
    NUI_CHECK(callbacks == 0);
    toggle(tree,platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"a"}));
    NUI_CHECK(callbacks == 1 && c->focus_gains == 0);
}

void key_validation_and_reentrant_throwing_callbacks_recover() {
    ui::State<std::vector<std::string>> keys{{}};
    auto invalid = ui::Accordion{keys}
        .section("a","First",ui::Spacer{1.0f,1.0f})
        .section("a","Duplicate",ui::Spacer{1.0f,1.0f});
    for (int retry = 0; retry < 2; ++retry) {
        bool threw = false;
        try { (void)std::move(invalid).spec(); }
        catch (const std::invalid_argument&) { threw = true; }
        NUI_CHECK(threw);
    }
    bool empty_threw = false;
    try { (void)ui::Accordion{keys}.section("","Empty",ui::Spacer{1.0f,1.0f}).spec(); }
    catch (const std::invalid_argument&) { empty_threw = true; }
    NUI_CHECK(empty_threw);
    int callbacks = 0;
    ui::UI tree{ui::Accordion{keys}.mode(ui::AccordionMode::Single)
        .section("a","A",ui::Spacer{120.0f,60.0f})
        .section("c","C",ui::Spacer{120.0f,60.0f})
        .on_change([&](const auto&) {
            ++callbacks;
            if (callbacks == 1) keys.set({"c"});
            else if (callbacks == 2) throw std::runtime_error("accordion callback");
        })};
    test::MockPlatform platform;
    tree.resize({240.0f,240.0f});
    tree.activate(platform);
    toggle(tree,platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"c"}) && callbacks == 1);
    bool threw = false;
    try { toggle(tree,platform); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw && keys.get() == std::vector<std::string>({"a"}) && callbacks == 2);
    auto stale_up = test::key(ui::Key::Space);
    stale_up.type = ui::InputType::KeyUp;
    tree.dispatch(stale_up,platform);
    NUI_CHECK(callbacks == 2);
    toggle(tree,platform);
    NUI_CHECK(keys.get().empty() && callbacks == 3);
}

void copied_specs_keep_roving_focus_and_timers_owned_by_each_tree() {
    ui::State<std::vector<std::string>> keys{{}};
    auto spec = ui::make_spec(ui::Accordion{keys}
        .section("a","A",ui::Spacer{120.0f,60.0f})
        .section("c","C",ui::Spacer{120.0f,60.0f}));
    ui::UI first{ui::Spec{spec}};
    auto second = std::make_unique<ui::UI>(ui::Spec{spec});
    test::MockPlatform first_platform;
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{{},clock};
    test::MockPlatform second_platform;
    second_platform.dispatcher_value = owner.dispatcher();
    first.resize({240.0f,240.0f});
    second->resize({240.0f,240.0f});
    first.activate(first_platform);
    first.dispatch(test::key(ui::Key::Down),first_platform);
    second->activate(second_platform);
    second->dispatch(test::key(ui::Key::Right),second_platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"a"}));
    keys.set({});
    first.dispatch(test::key(ui::Key::Right),first_platform);
    NUI_CHECK(keys.get() == std::vector<std::string>({"c"}));
    NUI_CHECK(owner.active_timer_count() > 0);
    second.reset();
    NUI_CHECK(owner.active_timer_count() == 0);
    clock->advance(250ms);
    NUI_CHECK(owner.checkpoint() == 0);
}

void suite() {
    multiple_and_single_policies_publish_one_canonical_vector();
    roving_navigation_skips_disabled_and_left_right_keep_disclosure_behavior();
    exclusive_external_change_restores_the_header_of_the_closed_panel();
    key_validation_and_reentrant_throwing_callbacks_recover();
    copied_specs_keep_roving_focus_and_timers_owned_by_each_tree();
}

} // namespace

int main() { return test::run("widget_accordion",&suite); }
