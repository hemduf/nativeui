#include "test_support.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LifecycleLog {
    std::vector<std::string> events;
    std::map<std::string, ui::NodeId> ids;
    int observed_changes{};
};

class LifecycleProbeComponent final : public ui::Component {
public:
    LifecycleProbeComponent(
        std::string name, std::shared_ptr<LifecycleLog> log, ui::State<int>& observed)
        : name_(std::move(name)), log_(std::move(log)), observed_(observed) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override {
        record("mount", context.node_id());
        subscription_ = observed_.observe([log = log_](const int&) { ++log->observed_changes; });
    }

    void activate(ui::LifecycleContext& context) override {
        record("activate", context.node_id());
    }

    void deactivate(ui::LifecycleContext& context) override {
        record("deactivate", context.node_id());
    }

    void unmount(ui::LifecycleContext& context) override {
        record("unmount", context.node_id());
    }

    ui::EventResult input(const ui::InputEvent&, ui::InputContext&) override {
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    void record(std::string_view phase, ui::NodeId id) {
        const auto [it, inserted] = log_->ids.emplace(name_, id);
        NUI_CHECK(inserted || it->second == id);
        log_->events.push_back(name_ + "." + std::string(phase));
    }

    std::string name_;
    std::shared_ptr<LifecycleLog> log_;
    ui::State<int>& observed_;
    ui::State<int>::Subscription subscription_;
};

class LifecycleProbe {
public:
    LifecycleProbe(std::string name, std::shared_ptr<LifecycleLog> log, ui::State<int>& observed)
        : name_(std::move(name)), log_(std::move(log)), observed_(&observed) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        auto* observed = observed_;
        return ui::Spec{
            [name = std::move(name), log = std::move(log), observed]() mutable {
                return std::make_unique<LifecycleProbeComponent>(
                    std::move(name), std::move(log), *observed);
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<LifecycleLog> log_;
    ui::State<int>* observed_{};
};

struct DynamicItem {
    std::string key;
    float extent{};

    bool operator==(const DynamicItem&) const = default;
};

struct DialogBodyState {
    int mounts{};
    int unmounts{};
};

class DialogBodyComponent final : public ui::Component {
public:
    explicit DialogBodyComponent(std::shared_ptr<DialogBodyState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 80.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<DialogBodyState> state_;
};

class DialogBody {
public:
    explicit DialogBody(std::shared_ptr<DialogBodyState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<DialogBodyComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<DialogBodyState> state_;
};

ui::DialogSpec dialog_spec() {
    ui::DialogSpec spec;
    spec.title = "Confirm";
    spec.body = ui::make_spec(ui::Spacer{120.0f, 80.0f});
    spec.actions.push_back(ui::DialogAction{
        "confirm", "Confirm", true, ui::DialogActionRole::Default});
    spec.actions.push_back(ui::DialogAction{
        "cancel", "Cancel", true, ui::DialogActionRole::Cancel});
    return spec;
}

ui::DialogSpec dialog_spec(std::shared_ptr<DialogBodyState> body) {
    auto spec = dialog_spec();
    spec.body = ui::make_spec(DialogBody{std::move(body)});
    return spec;
}

void suite() {
    // Existing focus/resize lifecycle regression.
    {
        auto probe = std::make_shared<test::ProbeState>();
        ui::UI tree{ui::Padding{10.0f, test::Probe{probe}}};
        test::MockPlatform platform;

        tree.resize({120.0f, 80.0f});
        NUI_CHECK(tree.dirty());

        tree.activate(platform);
        NUI_CHECK(probe->focus_in == 1);
        NUI_CHECK(probe->focus_out == 0);
        NUI_CHECK(probe->focus_bounds.size() == 1);
        NUI_CHECK_NEAR(probe->focus_bounds.back().x, 10.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().y, 10.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().w, 100.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().h, 60.0f, 0.0001f);

        tree.activate(platform);
        NUI_CHECK(probe->focus_in == 1);

        tree.resize({220.0f, 120.0f});
        tree.refresh_focus(platform);
        NUI_CHECK(probe->focus_in == 2);
        NUI_CHECK_NEAR(probe->focus_bounds.back().w, 200.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().h, 100.0f, 0.0001f);

        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK(probe->key_events == 1);

        tree.deactivate(platform);
        NUI_CHECK(probe->focus_out == 1);
        tree.deactivate(platform);
        NUI_CHECK(probe->focus_out == 1);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Ignored);
        NUI_CHECK(probe->key_events == 1);

        tree.activate(platform);
        NUI_CHECK(probe->focus_in == 3);
    }

    // Text-input platform service follows focus activation/deactivation.
    {
        test::MockPlatform platform;
        ui::State<std::string> text{"Init"};
        ui::UI text_tree{ui::TextInput{"Name", text}};
        text_tree.resize({320.0f, 90.0f});
        text_tree.activate(platform);
        NUI_CHECK(platform.text_input_active);
        text_tree.deactivate(platform);
        NUI_CHECK(!platform.text_input_active);
    }

    // Node IDs are unique within a compiled tree before any platform exists.
    {
        auto root = ui::compile(ui::make_spec(ui::Column{ui::Spacer{1.0f}, ui::Spacer{2.0f}}));
        NUI_CHECK(root->id != ui::kInvalidNodeId);
        NUI_CHECK(root->children.size() == 2);
        NUI_CHECK(root->children[0]->id != root->id);
        NUI_CHECK(root->children[1]->id != root->id);
        NUI_CHECK(root->children[0]->id != root->children[1]->id);
    }

    // A keyed snapshot whose keys are unchanged is structurally inert. The
    // state observer must request a future checkpoint without pre-emptively
    // dirtying layout; the retained child is deliberately not rebound when its
    // same-key item payload changes.
    {
        ui::State<std::vector<DynamicItem>> items{{DynamicItem{"stable", 10.0f}}};
        ui::UI tree{ui::ForEach<DynamicItem>{
            items,
            [](const DynamicItem& item) { return item.key; },
            [](const DynamicItem& item) { return ui::Spacer{item.extent, item.extent}; }}};

        tree.resize({120.0f, 80.0f});
        NUI_CHECK(!tree.layout_dirty());

        items.set({DynamicItem{"stable", 20.0f}});
        NUI_CHECK(!tree.layout_dirty());

        tree.resize({120.0f, 80.0f});
        NUI_CHECK(!tree.layout_dirty());
    }

    // Duplicate keys in the initial keyed snapshot are invalid as a whole.
    // No ambiguous retained child may mount; a later valid snapshot can recover
    // at the next structural checkpoint.
    {
        ui::State<int> observed{0};
        auto duplicate_log = std::make_shared<LifecycleLog>();
        ui::State<std::vector<DynamicItem>> items{{
            DynamicItem{"duplicate", 10.0f},
            DynamicItem{"duplicate", 20.0f},
        }};
        ui::UI tree{ui::ForEach<DynamicItem>{
            items,
            [](const DynamicItem& item) { return item.key; },
            [duplicate_log, &observed](const DynamicItem& item) {
                return LifecycleProbe{item.key, duplicate_log, observed};
            }}};

        NUI_CHECK(duplicate_log->events.empty());
        items.set({DynamicItem{"A", 10.0f}, DynamicItem{"B", 20.0f}});
        tree.resize({120.0f, 80.0f});
        NUI_CHECK((duplicate_log->events == std::vector<std::string>{"A.mount", "B.mount"}));
    }

    // T063 rejects malformed public specs without acquiring the per-UI slot.
    {
        ui::UI tree{ui::Spacer{160.0f, 100.0f}};
        ui::Dialog dialog{tree};

        ui::DialogSpec missing_body;
        NUI_CHECK(dialog.show(std::move(missing_body), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::InvalidSpec);

        auto duplicate_id = dialog_spec();
        duplicate_id.actions[1].id = duplicate_id.actions[0].id;
        NUI_CHECK(dialog.show(std::move(duplicate_id), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::InvalidSpec);

        auto empty_id = dialog_spec();
        empty_id.actions[0].id.clear();
        NUI_CHECK(dialog.show(std::move(empty_id), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::InvalidSpec);

        auto two_defaults = dialog_spec();
        two_defaults.actions[1].role = ui::DialogActionRole::Default;
        NUI_CHECK(dialog.show(std::move(two_defaults), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::InvalidSpec);

        auto two_cancels = dialog_spec();
        two_cancels.actions[0].role = ui::DialogActionRole::Cancel;
        NUI_CHECK(dialog.show(std::move(two_cancels), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::InvalidSpec);

        NUI_CHECK(dialog.show(dialog_spec(), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::Shown);
        NUI_CHECK(dialog.close());
    }

    // T063 owns exactly one active Dialog slot per UI, not per controller.
    // Closing the first controller releases that shared slot before invoking
    // its callback, so a second controller can become active immediately.
    {
        ui::UI tree{ui::Spacer{160.0f, 100.0f}};
        ui::Dialog first{tree};
        ui::Dialog second{tree};
        int callbacks = 0;

        NUI_CHECK(first.show(dialog_spec(), [&](ui::DialogResult result) {
            NUI_CHECK(result.kind == ui::DialogResultKind::Dismissed);
            ++callbacks;
        }) == ui::DialogShowResult::Shown);
        NUI_CHECK(second.show(dialog_spec(), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::Busy);
        NUI_CHECK(first.close());
        NUI_CHECK(!first.close());
        NUI_CHECK(callbacks == 1);

        NUI_CHECK(second.show(dialog_spec(), [&](ui::DialogResult result) {
            NUI_CHECK(result.kind == ui::DialogResultKind::Dismissed);
            ++callbacks;
        }) == ui::DialogShowResult::Shown);
        NUI_CHECK(second.close());
        NUI_CHECK(callbacks == 2);
    }

    // Dialog ownership is per UI: two simultaneous plugin/view instances are
    // independent and closing one cannot release or invalidate the other.
    {
        ui::UI first_tree{ui::Spacer{160.0f, 100.0f}};
        ui::UI second_tree{ui::Spacer{160.0f, 100.0f}};
        ui::Dialog first{first_tree};
        ui::Dialog second{second_tree};

        NUI_CHECK(first.show(dialog_spec(), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::Shown);
        NUI_CHECK(second.show(dialog_spec(), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::Shown);
        NUI_CHECK(first.close());
        NUI_CHECK(!first.active());
        NUI_CHECK(second.active());
        NUI_CHECK(second.close());
    }

    // Enabled Default is the initial logical action. Space has no panel-level
    // fallback, so completion here proves the Button itself owns initial focus.
    {
        test::MockPlatform platform;
        auto root = std::make_shared<test::ProbeState>();
        auto body = std::make_shared<test::ProbeState>();
        ui::UI tree{test::Probe{root}};
        tree.resize({360.0f, 260.0f});
        tree.activate(platform);

        auto spec = dialog_spec();
        spec.body = ui::make_spec(test::Probe{body});
        ui::Dialog dialog{tree};
        int callbacks = 0;
        NUI_CHECK(dialog.show(std::move(spec), [&](ui::DialogResult result) {
            NUI_CHECK(result.kind == ui::DialogResultKind::Action);
            NUI_CHECK(result.action_id == "confirm");
            ++callbacks;
        }) == ui::DialogShowResult::Shown);
        tree.resize({360.0f, 260.0f});
        NUI_CHECK(body->focus_in == 0);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
                  ui::EventResult::Handled);
        auto space_up = test::key(ui::Key::Space);
        space_up.type = ui::InputType::KeyUp;
        NUI_CHECK(tree.dispatch(space_up, platform) == ui::EventResult::Handled);
        NUI_CHECK(callbacks == 1);
        NUI_CHECK(!dialog.active());
    }

    // With no enabled Default, focus enters the first body descendant. Tab and
    // Shift+Tab remain trapped in the modal scope, and explicit close restores
    // the previously focused root.
    {
        test::MockPlatform platform;
        auto root = std::make_shared<test::ProbeState>();
        auto body = std::make_shared<test::ProbeState>();
        ui::UI tree{test::Probe{root}};
        tree.resize({360.0f, 260.0f});
        tree.activate(platform);
        NUI_CHECK(root->focus_in == 1);

        ui::DialogSpec spec;
        spec.body = ui::make_spec(test::Probe{body});
        spec.actions.push_back(ui::DialogAction{
            "cancel", "Cancel", true, ui::DialogActionRole::Cancel});
        ui::Dialog dialog{tree};
        NUI_CHECK(dialog.show(std::move(spec), [](ui::DialogResult) {}) ==
                  ui::DialogShowResult::Shown);
        tree.resize({360.0f, 260.0f});
        NUI_CHECK(root->focus_out == 1);
        NUI_CHECK(body->focus_in == 1);

        for (int i = 0; i < 4; ++i) {
            NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) ==
                      ui::EventResult::Handled);
        }
        NUI_CHECK(root->focus_in == 1);
        NUI_CHECK(!root->focused);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab, true), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(root->focus_in == 1);

        NUI_CHECK(dialog.close());
        NUI_CHECK(root->focus_in == 2);
        NUI_CHECK(root->focused);
    }

    // Focused body input gets first refusal on Enter. Only Ignored Enter may
    // bubble to the panel's Default fallback.
    {
        test::MockPlatform platform;
        auto body = std::make_shared<test::ProbeState>();
        body->input_result = ui::EventResult::Handled;
        ui::UI tree{ui::Spacer{360.0f, 260.0f}};
        tree.resize({360.0f, 260.0f});
        tree.activate(platform);

        auto spec = dialog_spec();
        spec.body = ui::make_spec(test::Probe{body});
        ui::Dialog dialog{tree};
        int actions = 0;
        NUI_CHECK(dialog.show(std::move(spec), [&](ui::DialogResult result) {
            if (result.kind == ui::DialogResultKind::Action) ++actions;
        }) == ui::DialogShowResult::Shown);
        tree.resize({360.0f, 260.0f});

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Tab), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(body->focus_in == 1);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Enter), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(actions == 0);
        NUI_CHECK(dialog.active());
        NUI_CHECK(dialog.close());
    }

    // Explicit controller destruction while the UI is alive is an ordinary
    // Dismissed close: teardown completes once and prior focus is restored.
    {
        test::MockPlatform platform;
        auto root = std::make_shared<test::ProbeState>();
        ui::UI tree{test::Probe{root}};
        tree.resize({360.0f, 260.0f});
        tree.activate(platform);
        int callbacks = 0;
        {
            ui::Dialog dialog{tree};
            NUI_CHECK(dialog.show(dialog_spec(), [&](ui::DialogResult result) {
                NUI_CHECK(result.kind == ui::DialogResultKind::Dismissed);
                ++callbacks;
            }) == ui::DialogShowResult::Shown);
            tree.resize({360.0f, 260.0f});
            NUI_CHECK(root->focus_out == 1);
        }
        NUI_CHECK(callbacks == 1);
        NUI_CHECK(root->focus_in == 2);
        NUI_CHECK(root->focused);
    }

    // UI deactivation is not an application dismissal. It suppresses the
    // callback and must not transiently restore focus into the deactivating UI.
    {
        test::MockPlatform platform;
        auto root = std::make_shared<test::ProbeState>();
        ui::UI tree{test::Probe{root}};
        tree.resize({360.0f, 260.0f});
        tree.activate(platform);
        NUI_CHECK(root->focus_in == 1);

        ui::Dialog dialog{tree};
        int callbacks = 0;
        NUI_CHECK(dialog.show(dialog_spec(), [&](ui::DialogResult) {
            ++callbacks;
        }) == ui::DialogShowResult::Shown);
        tree.resize({360.0f, 260.0f});
        NUI_CHECK(root->focus_out == 1);

        tree.deactivate(platform);
        NUI_CHECK(callbacks == 0);
        NUI_CHECK(!dialog.active());
        NUI_CHECK(root->focus_in == 1);
        NUI_CHECK(!root->focused);

        tree.activate(platform);
        NUI_CHECK(root->focus_in == 2);
        NUI_CHECK(root->focused);
    }

    // Completion callbacks run only after the T061/T058 subtree is actually
    // detached and unmounted, not merely after the OverlayHandle becomes stale.
    // The callback can therefore show the next Dialog immediately without the
    // old subtree still participating in focus/capture/lifecycle state.
    {
        test::MockPlatform platform;
        ui::UI tree{ui::Spacer{160.0f, 100.0f}};
        tree.resize({160.0f, 100.0f});
        tree.activate(platform);

        auto body = std::make_shared<DialogBodyState>();
        ui::Dialog first{tree};
        ui::Dialog second{tree};
        bool unmounted_before_callback = false;
        ui::DialogShowResult reentrant_show = ui::DialogShowResult::Unavailable;

        NUI_CHECK(first.show(dialog_spec(body), [&](ui::DialogResult result) {
            NUI_CHECK(result.kind == ui::DialogResultKind::Dismissed);
            unmounted_before_callback = body->unmounts == 1;
            reentrant_show = second.show(dialog_spec(), [](ui::DialogResult) {});
        }) == ui::DialogShowResult::Shown);
        tree.resize({160.0f, 100.0f});
        NUI_CHECK(body->mounts == 1);

        NUI_CHECK(first.close());
        NUI_CHECK(unmounted_before_callback);
        NUI_CHECK(body->unmounts == 1);
        NUI_CHECK(reentrant_show == ui::DialogShowResult::Shown);
        NUI_CHECK(second.close());
    }

    // Whole-UI teardown is observably different from explicit controller
    // destruction: a surviving controller must become inert and suppress its
    // application callback rather than reporting Dismissed after its UI died.
    {
        int callbacks = 0;
        std::unique_ptr<ui::Dialog> surviving_dialog;
        {
            ui::UI tree{ui::Spacer{160.0f, 100.0f}};
            surviving_dialog = std::make_unique<ui::Dialog>(tree);
            NUI_CHECK(surviving_dialog->show(dialog_spec(), [&](ui::DialogResult) {
                ++callbacks;
            }) == ui::DialogShowResult::Shown);
        }
        NUI_CHECK(callbacks == 0);
        NUI_CHECK(!surviving_dialog->active());
        surviving_dialog.reset();
        NUI_CHECK(callbacks == 0);
    }

    // Deterministic mount/activate/deactivate/unmount order and stable IDs.
    ui::State<int> observed{0};
    auto log = std::make_shared<LifecycleLog>();
    {
        test::MockPlatform platform;
        ui::UI tree{
            ui::Column{
                LifecycleProbe{"A", log, observed},
                LifecycleProbe{"B", log, observed},
            }.padding(0.0f).gap(0.0f)
        };

        NUI_CHECK((log->events == std::vector<std::string>{"A.mount", "B.mount"}));
        NUI_CHECK(log->ids.at("A") != log->ids.at("B"));

        tree.resize({200.0f, 100.0f});
        tree.activate(platform);
        NUI_CHECK((log->events == std::vector<std::string>{
            "A.mount", "B.mount", "A.activate", "B.activate"}));

        observed.set(1);
        NUI_CHECK(log->observed_changes == 2);

        tree.deactivate(platform);
        NUI_CHECK((log->events == std::vector<std::string>{
            "A.mount", "B.mount", "A.activate", "B.activate",
            "B.deactivate", "A.deactivate"}));
    }

    NUI_CHECK((log->events == std::vector<std::string>{
        "A.mount", "B.mount", "A.activate", "B.activate",
        "B.deactivate", "A.deactivate", "B.unmount", "A.unmount"}));

    // Component destruction releases State subscriptions; later changes cannot
    // call into removed runtime nodes.
    observed.set(2);
    NUI_CHECK(log->observed_changes == 2);
}

} // namespace

int main() { return test::run("lifecycle", &suite); }