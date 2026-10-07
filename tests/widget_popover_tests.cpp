#include "test_support.hpp"
#include <nativeui/popover.hpp>
#include <nativeui/column.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/if.hpp>

#include <memory>
#include <stdexcept>

namespace {
struct Observation {
    ui::NodeId id{};
    ui::Rect bounds{};
    bool focused{};
    int mounts{};
    int unmounts{};
    int presses{};
};
class ProbeComponent final : public ui::Component {
  public:
    ProbeComponent(std::shared_ptr<Observation> state, ui::Size size)
        : state_(std::move(state)), size_(size) {}
    ui::Size measure(const std::vector<ui::ChildMetrics> &) const override { return size_; }
    bool focusable() const noexcept override { return true; }
    void mount(ui::MountContext &context) override {
        state_->id = context.node_id();
        ++state_->mounts;
    }
    void unmount(ui::LifecycleContext &) override { ++state_->unmounts; }
    void focus_changed(bool value, ui::FocusContext &) override { state_->focused = value; }
    ui::EventResult input(const ui::InputEvent &event, ui::InputContext &) override {
        if (event.type != ui::InputType::PointerDown)
            return ui::EventResult::Ignored;
        ++state_->presses;
        return ui::EventResult::Handled;
    }
    ui::SemanticInfo semantics() const override {
        ui::SemanticInfo result;
        result.role = ui::SemanticRole::Button;
        result.name = "Probe";
        return result;
    }
    void paint(ui::PaintContext &context) const override { state_->bounds = context.bounds(); }

  private:
    std::shared_ptr<Observation> state_;
    ui::Size size_;
};
ui::Spec probe(std::shared_ptr<Observation> state, ui::Size size = {120.0f, 40.0f}) {
    return {
        [state = std::move(state), size] { return std::make_unique<ProbeComponent>(state, size); },
        {}};
}
void frame(ui::UI &tree, ui::Size size = {200.0f, 180.0f}) {
    tree.resize(size);
    ui::HeadlessRenderer renderer{size, 1.0f};
    NUI_CHECK(renderer.render(tree));
}
void retained_anchor_fresh_content_and_external_close() {
    auto anchor = std::make_shared<Observation>(), body = std::make_shared<Observation>();
    ui::State<bool> open{false};
    int notifications = 0;
    ui::UI tree{ui::Column{ui::Popover{open, probe(anchor), probe(body, {20.0f, 20.0f})}.on_close(
                               [&] { ++notifications; }),
                           ui::Spacer{0.0f, 100.0f}}
                    .padding(0.0f)
                    .gap(0.0f).align(ui::Align::Stretch)};
    test::MockPlatform platform;
    frame(tree);
    tree.activate(platform);
    const auto anchor_id = anchor->id;
    NUI_CHECK(anchor->mounts == 1 && body->mounts == 0);
    open.set(true);
    frame(tree);
    NUI_CHECK(anchor->id == anchor_id && anchor->mounts == 1 && body->mounts == 1);
    const auto first_body = body->id;
    auto anchor_info = tree.component_semantics(anchor_id);
    NUI_CHECK(anchor_info && anchor_info->role == ui::SemanticRole::Button &&
              anchor_info->expanded == ui::SemanticExpandedState::Expanded);
    const auto entries = tree.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK_NEAR(entries.front().bounds.w, 200.0f, 0.01f);
    open.set(false);
    frame(tree);
    NUI_CHECK(tree.overlay_entries().empty() && body->unmounts == 1 && notifications == 0);
    open.set(true);
    frame(tree);
    NUI_CHECK(body->mounts == 2 && body->id != first_body && anchor->id == anchor_id);
}
void outside_escape_are_immediate_once_and_do_not_click_through() {
    auto anchor = std::make_shared<Observation>(), outside = std::make_shared<Observation>(),
         body = std::make_shared<Observation>();
    ui::State<bool> open{false};
    int closes = 0;
    ui::UI tree{ui::Column{ui::Popover{open, probe(anchor), probe(body, {20.0f, 20.0f})}
                               .match_anchor_width(false)
                               .on_close([&] {
                                   NUI_CHECK(!open.get());
                                   ++closes;
                               }),
                           probe(outside, {120.0f, 80.0f})}
                    .padding(0.0f)
                    .gap(0.0f)};
    test::MockPlatform platform;
    frame(tree);
    tree.activate(platform);
    open.set(true);
    frame(tree);
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 180.0f, 100.0f), platform);
    NUI_CHECK(!open.get() && closes == 1 && outside->presses == 0 &&
              tree.overlay_entries().empty());
    open.set(true);
    frame(tree);
    tree.dispatch(test::key(ui::Key::Escape), platform);
    NUI_CHECK(!open.get() && closes == 2 && tree.overlay_entries().empty());
    tree.dispatch(test::key(ui::Key::Escape), platform);
    NUI_CHECK(closes == 2);
}
void nonmodal_focus_is_opt_in_and_restores_only_when_within() {
    auto anchor = std::make_shared<Observation>(), outside = std::make_shared<Observation>(),
         body = std::make_shared<Observation>();
    ui::State<bool> open{false};
    ui::UI tree{
        ui::Column{ui::Popover{open, probe(anchor), probe(body, {20.0f, 20.0f})}.focus_on_open(),
                   probe(outside)}
            .padding(0.0f)
            .gap(0.0f)};
    test::MockPlatform platform;
    frame(tree);
    tree.activate(platform);
    NUI_CHECK(anchor->focused);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(outside->focused);
    open.set(true);
    frame(tree);
    tree.refresh_focus(platform);
    NUI_CHECK(body->focused && !outside->focused);
    open.set(false);
    frame(tree);
    tree.refresh_focus(platform);
    NUI_CHECK(outside->focused && !anchor->focused);
    open.set(true);
    frame(tree);
    tree.refresh_focus(platform);
    NUI_CHECK(body->focused);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(anchor->focused);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(outside->focused);
    open.set(false);
    frame(tree);
    tree.refresh_focus(platform);
    NUI_CHECK(outside->focused);
    auto other_anchor = std::make_shared<Observation>(),
         other_body = std::make_shared<Observation>();
    ui::State<bool> other_open{false};
    ui::UI second{ui::Popover{other_open, probe(other_anchor), probe(other_body, {20.0f, 20.0f})}};
    frame(second);
    second.activate(platform);
    other_open.set(true);
    frame(second);
    second.refresh_focus(platform);
    NUI_CHECK(other_anchor->focused && !other_body->focused);
}
void source_expiry_disable_and_retirement_close_silently() {
    auto anchor = std::make_shared<Observation>(), body = std::make_shared<Observation>();
    auto source = std::make_unique<ui::State<bool>>(false);
    auto binding = source->binding();
    ui::State<bool> enabled{true}, shown{true};
    int closes = 0;
    ui::UI tree{ui::If{shown, ui::Popover{binding, ui::Enabled{enabled, probe(anchor)},
                                          probe(body, {20.0f, 20.0f})}
                                  .on_close([&] { ++closes; })}};
    test::MockPlatform platform;
    frame(tree);
    tree.activate(platform);
    source->set(true);
    frame(tree);
    enabled.set(false);
    frame(tree);
    NUI_CHECK(!source->get() && tree.overlay_entries().empty() && closes == 0);
    source->set(true);
    frame(tree);
    NUI_CHECK(!source->get() && tree.overlay_entries().empty());
    enabled.set(true);
    source->set(true);
    frame(tree);
    NUI_CHECK(tree.overlay_entries().size() == 1);
    source.reset();
    frame(tree);
    NUI_CHECK(!binding.valid() && binding.get() && tree.overlay_entries().empty() && closes == 0);
    shown.set(false);
    frame(tree);
    NUI_CHECK(tree.overlay_entries().empty());
}
void observers_and_callbacks_recover_without_replaying_started_work() {
    ui::State<bool> open{false};
    bool observer_fault = false;
    int closes = 0;
    auto first = open.observe([&](bool value) {
        if (observer_fault && !value)
            throw std::runtime_error("false observer");
    });
    auto anchor = std::make_shared<Observation>(), body = std::make_shared<Observation>();
    bool callback_fault = false;
    ui::UI tree{ui::Popover{open, probe(anchor), probe(body, {20.0f, 20.0f})}.on_close([&] {
        ++closes;
        if (callback_fault)
            throw std::runtime_error("close callback");
    })};
    test::MockPlatform platform;
    frame(tree);
    tree.activate(platform);
    open.set(true);
    frame(tree);
    observer_fault = true;
    bool threw = false;
    try {
        tree.dispatch(test::key(ui::Key::Escape), platform);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    NUI_CHECK(threw && !open.get() && closes == 0);
    observer_fault = false;
    frame(tree);
    NUI_CHECK(closes == 1 && tree.overlay_entries().empty());
    open.set(true);
    frame(tree);
    callback_fault = true;
    threw = false;
    try {
        tree.dispatch(test::key(ui::Key::Escape), platform);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    NUI_CHECK(threw && !open.get() && closes == 2);
    callback_fault = false;
    frame(tree);
    NUI_CHECK(closes == 2);
    open.set(true);
    frame(tree);
    tree.dispatch(test::key(ui::Key::Escape), platform);
    NUI_CHECK(closes == 3);
}
void reentrant_reopen_and_copied_specs_are_isolated() {
    auto anchor = std::make_shared<Observation>(), body = std::make_shared<Observation>();
    ui::State<bool> open{false};
    int closes = 0;
    auto specification = ui::Popover{open, probe(anchor), probe(body, {20.0f, 20.0f})}
                             .on_close([&] {
                                 ++closes;
                                 if (closes == 1)
                                     open.set(true);
                             })
                             .spec();
    ui::UI first{specification}, second{specification};
    test::MockPlatform a, b;
    frame(first);
    frame(second);
    first.activate(a);
    second.activate(b);
    open.set(true);
    frame(first);
    frame(second);
    NUI_CHECK(first.overlay_entries().size() == 1 && second.overlay_entries().size() == 1);
    first.dispatch(test::key(ui::Key::Escape), a);
    frame(first);
    frame(second);
    NUI_CHECK(open.get() && closes == 1 && first.overlay_entries().size() == 1 &&
              second.overlay_entries().size() == 1);
    first.deactivate(a);
    frame(second);
    NUI_CHECK(!open.get() && closes == 1 && second.overlay_entries().empty());
}
void invalid_recipes_are_rejected_before_mount() {
    ui::State<bool> open{false};
    bool threw = false;
    try {
        (void)ui::Popover{open, ui::Spec{}, ui::Label{"Body"}}.spec();
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    NUI_CHECK(threw);
}
struct CopyFault {
    std::shared_ptr<bool> fault;
    CopyFault(std::shared_ptr<bool> value) : fault(std::move(value)) {}
    CopyFault(const CopyFault &other) : fault(other.fault) {
        if (*fault)
            throw std::runtime_error("popover preparation copy");
    }
    CopyFault(CopyFault &&) noexcept = default;
    std::unique_ptr<ui::Component> operator()() const {
        return std::make_unique<ProbeComponent>(std::make_shared<Observation>(),
                                                ui::Size{20.0f, 20.0f});
    }
};
void preparation_failure_restores_false_without_a_ghost() {
    auto fault = std::make_shared<bool>(false);
    ui::Spec content{CopyFault{fault}, {}};
    ui::State<bool> open{false};
    int closes = 0;
    ui::UI tree{ui::Popover{open, ui::Button{"Anchor", [] {}}, std::move(content)}.on_close(
        [&] { ++closes; })};
    test::MockPlatform platform;
    frame(tree);
    tree.activate(platform);
    *fault = true;
    open.set(true);
    bool threw = false;
    try {
        frame(tree);
    } catch (const std::runtime_error &) {
        threw = true;
    }
    NUI_CHECK(threw && !open.get() && tree.overlay_entries().empty() && closes == 0);
    *fault = false;
    open.set(true);
    frame(tree);
    NUI_CHECK(tree.overlay_entries().size() == 1);
    tree.dispatch(test::key(ui::Key::Escape), platform);
    NUI_CHECK(!open.get() && closes == 1);
}

void suite() {
    retained_anchor_fresh_content_and_external_close();
    outside_escape_are_immediate_once_and_do_not_click_through();
    nonmodal_focus_is_opt_in_and_restores_only_when_within();
    source_expiry_disable_and_retirement_close_silently();
    observers_and_callbacks_recover_without_replaying_started_work();
    reentrant_reopen_and_copied_specs_are_isolated();
    invalid_recipes_are_rejected_before_mount();
    preparation_failure_restores_false_without_a_ghost();
}
} // namespace
int main() { return test::run("popover", &suite); }
