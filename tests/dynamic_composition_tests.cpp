#include "test_support.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct DynamicLog {
    std::vector<std::string> events;
    std::map<std::string, std::vector<ui::NodeId>> mounted_ids;
    int pointer_cancel_count{};
    int focus_in_count{};
    int focus_out_count{};
    int observed_changes{};
};

class DynamicProbeComponent final : public ui::Component {
public:
    DynamicProbeComponent(std::string name, std::shared_ptr<DynamicLog> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override {
        log_->events.push_back(name_ + ".mount");
        log_->mounted_ids[name_].push_back(context.node_id());
    }

    void activate(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".activate");
    }

    void deactivate(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".deactivate");
    }

    void unmount(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".unmount");
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
};

class DynamicProbe {
public:
    DynamicProbe(std::string name, std::shared_ptr<DynamicLog> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        return ui::Spec{
            [name = std::move(name), log = std::move(log)]() mutable {
                return std::make_unique<DynamicProbeComponent>(std::move(name), std::move(log));
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
};

class LifetimeOrderProbeComponent final : public ui::Component {
public:
    LifetimeOrderProbeComponent(std::string name, std::shared_ptr<DynamicLog> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    ~LifetimeOrderProbeComponent() override {
        log_->events.push_back(name_ + ".destroy");
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext&) override { log_->events.push_back(name_ + ".mount"); }
    void activate(ui::LifecycleContext&) override { log_->events.push_back(name_ + ".activate"); }
    void deactivate(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".deactivate");
    }
    void unmount(ui::LifecycleContext&) override { log_->events.push_back(name_ + ".unmount"); }
    void paint(ui::PaintContext&) const override {}

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
};

class LifetimeOrderProbe {
public:
    LifetimeOrderProbe(std::string name, std::shared_ptr<DynamicLog> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        return ui::Spec{
            [name = std::move(name), log = std::move(log)]() mutable {
                return std::make_unique<LifetimeOrderProbeComponent>(
                    std::move(name), std::move(log));
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
};

class InteractiveDynamicProbeComponent final : public ui::Component {
public:
    InteractiveDynamicProbeComponent(std::shared_ptr<DynamicLog> log, ui::State<int>& observed)
        : log_(std::move(log)), observed_(&observed) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext&) override {
        subscription_ = observed_->observe([log = log_](const int&) { ++log->observed_changes; });
    }

    void unmount(ui::LifecycleContext&) override { subscription_.reset(); }

    void focus_changed(bool focused, ui::FocusContext&) override {
        focused ? ++log_->focus_in_count : ++log_->focus_out_count;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerDown) {
            context.capture_pointer();
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerCancel) {
            ++log_->pointer_cancel_count;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<DynamicLog> log_;
    ui::State<int>* observed_{};
    ui::State<int>::Subscription subscription_;
};

class InteractiveDynamicProbe {
public:
    InteractiveDynamicProbe(std::shared_ptr<DynamicLog> log, ui::State<int>& observed)
        : log_(std::move(log)), observed_(&observed) {}

    ui::Spec spec() && {
        auto log = std::move(log_);
        auto* observed = observed_;
        return ui::Spec{
            [log = std::move(log), observed] {
                return std::make_unique<InteractiveDynamicProbeComponent>(log, *observed);
            },
            {}};
    }

private:
    std::shared_ptr<DynamicLog> log_;
    ui::State<int>* observed_{};
};

struct DynamicItem {
    std::string key;
    std::string name;

    bool operator==(const DynamicItem&) const = default;
};

struct LoopState {
    bool armed{};
    int mounts{};
    int unmounts{};
};

class LoopProbeComponent final : public ui::Component {
public:
    LoopProbeComponent(ui::State<bool>& visible, std::shared_ptr<LoopState> state)
        : visible_(&visible), state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {1.0f, 1.0f};
    }

    void mount(ui::MountContext&) override {
        ++state_->mounts;
        if (state_->armed) visible_->set(false);
    }

    void unmount(ui::LifecycleContext&) override {
        ++state_->unmounts;
        if (state_->armed) visible_->set(true);
    }

    void paint(ui::PaintContext&) const override {}

private:
    ui::State<bool>* visible_{};
    std::shared_ptr<LoopState> state_;
};

class LoopProbe {
public:
    LoopProbe(ui::State<bool>& visible, std::shared_ptr<LoopState> state)
        : visible_(&visible), state_(std::move(state)) {}

    ui::Spec spec() && {
        auto* visible = visible_;
        auto state = std::move(state_);
        return ui::Spec{
            [visible, state = std::move(state)] {
                return std::make_unique<LoopProbeComponent>(*visible, state);
            },
            {}};
    }

private:
    ui::State<bool>* visible_{};
    std::shared_ptr<LoopState> state_;
};

void conditional_contract() {
    ui::State<bool> visible{true};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, DynamicProbe{"child", log}}};
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount"}));
    NUI_CHECK(log->mounted_ids.at("child").size() == 1);
    const auto first_id = log->mounted_ids.at("child").front();

    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount", "child.activate"}));

    visible.set(false);
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount", "child.activate"}));

    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount"}));

    visible.set(true);
    NUI_CHECK(log->mounted_ids.at("child").size() == 1);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("child").size() == 2);
    NUI_CHECK(log->mounted_ids.at("child").back() != first_id);
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount",
        "child.mount", "child.activate"}));

    visible.set(true);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("child").size() == 2);
}

void switch_contract() {
    ui::State<int> selected{1};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::Switch<int>{selected}
                    .when(1, DynamicProbe{"one", log})
                    .when(2, DynamicProbe{"two", log})
                    .otherwise(DynamicProbe{"fallback", log})};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK((log->events == std::vector<std::string>{"one.mount", "one.activate"}));

    selected.set(2);
    NUI_CHECK(log->events.size() == 2);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "one.mount", "one.activate", "one.deactivate", "one.unmount",
        "two.mount", "two.activate"}));

    selected.set(99);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "one.mount", "one.activate", "one.deactivate", "one.unmount",
        "two.mount", "two.activate", "two.deactivate", "two.unmount",
        "fallback.mount", "fallback.activate"}));
}

void replacement_destroys_before_insert_contract() {
    ui::State<int> selected{1};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::Switch<int>{selected}
                    .when(1, LifetimeOrderProbe{"old", log})
                    .when(2, LifetimeOrderProbe{"new", log})};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK((log->events == std::vector<std::string>{"old.mount", "old.activate"}));

    selected.set(2);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "old.mount", "old.activate", "old.deactivate", "old.unmount", "old.destroy",
        "new.mount", "new.activate"}));
}

void keyed_contract() {
    ui::State<std::vector<DynamicItem>> items{{
        DynamicItem{"A", "A"},
        DynamicItem{"B", "B"},
    }};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::ForEach<DynamicItem>{
        items,
        [](const DynamicItem& item) { return item.key; },
        [log](const DynamicItem& item) { return DynamicProbe{item.name, log}; }
    }};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK((log->events == std::vector<std::string>{
        "A.mount", "B.mount", "A.activate", "B.activate"}));
    const auto a_id = log->mounted_ids.at("A").front();
    const auto b_id = log->mounted_ids.at("B").front();

    items.set({DynamicItem{"B", "B"}, DynamicItem{"A", "A"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 4);
    NUI_CHECK(log->mounted_ids.at("A").front() == a_id);
    NUI_CHECK(log->mounted_ids.at("B").front() == b_id);

    items.set({DynamicItem{"B", "replacement-B"}, DynamicItem{"A", "A"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 4);
    NUI_CHECK(!log->mounted_ids.contains("replacement-B"));

    items.set({DynamicItem{"A", "A"}, DynamicItem{"A", "duplicate"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 4);
    NUI_CHECK(log->mounted_ids.at("A").front() == a_id);
    NUI_CHECK(log->mounted_ids.at("B").front() == b_id);

    items.set({
        DynamicItem{"B", "B"},
        DynamicItem{"C", "C"},
        DynamicItem{"A", "A"},
    });
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("A").front() == a_id);
    NUI_CHECK(log->mounted_ids.at("B").front() == b_id);
    NUI_CHECK(log->mounted_ids.at("C").size() == 1);
    NUI_CHECK((log->events == std::vector<std::string>{
        "A.mount", "B.mount", "A.activate", "B.activate", "C.mount", "C.activate"}));

    // Remove, prepend, append and middle-insert all preserve unchanged keys.
    items.set({DynamicItem{"C", "C"}, DynamicItem{"A", "A"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 8);
    NUI_CHECK(log->events[6] == "B.deactivate");
    NUI_CHECK(log->events[7] == "B.unmount");

    items.set({DynamicItem{"D", "D"}, DynamicItem{"C", "C"}, DynamicItem{"A", "A"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 10);
    NUI_CHECK(log->mounted_ids.at("D").size() == 1);

    items.set({
        DynamicItem{"D", "D"}, DynamicItem{"C", "C"}, DynamicItem{"A", "A"},
        DynamicItem{"E", "E"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 12);
    NUI_CHECK(log->mounted_ids.at("E").size() == 1);

    items.set({
        DynamicItem{"D", "D"}, DynamicItem{"C", "C"}, DynamicItem{"F", "F"},
        DynamicItem{"A", "A"}, DynamicItem{"E", "E"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 14);
    NUI_CHECK(log->mounted_ids.at("F").size() == 1);
    NUI_CHECK(log->mounted_ids.at("A").front() == a_id);
    NUI_CHECK(log->mounted_ids.at("C").size() == 1);

    // Changing a logical key forces one old teardown and one new retained node.
    items.set({
        DynamicItem{"G", "G"}, DynamicItem{"C", "C"}, DynamicItem{"F", "F"},
        DynamicItem{"A", "A"}, DynamicItem{"E", "E"}});
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 18);
    NUI_CHECK(log->events[14] == "D.deactivate");
    NUI_CHECK(log->events[15] == "D.unmount");
    NUI_CHECK(log->events[16] == "G.mount");
    NUI_CHECK(log->events[17] == "G.activate");
    NUI_CHECK(log->mounted_ids.at("G").size() == 1);
}

void coalesced_writes_contract() {
    ui::State<bool> visible{true};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, DynamicProbe{"child", log}}};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);

    visible.set(false);
    visible.set(true);
    visible.set(false);
    NUI_CHECK(log->events.size() == 2);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount"}));

    visible.set(true);
    visible.set(false);
    visible.set(true);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount",
        "child.mount", "child.activate"}));
}

void focus_capture_and_lifetime_contract() {
    ui::State<bool> visible{true};
    ui::State<int> observed{0};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, InteractiveDynamicProbe{log, observed}}};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK(log->focus_in_count == 1);

    NUI_CHECK(tree.dispatch(
        test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform) ==
        ui::EventResult::Handled);

    visible.set(false);
    NUI_CHECK(log->pointer_cancel_count == 0);
    NUI_CHECK(log->focus_out_count == 0);

    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->pointer_cancel_count == 1);
    NUI_CHECK(log->focus_out_count == 1);
    NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);

    observed.set(1);
    NUI_CHECK(log->observed_changes == 0);
}

void focus_scope_rehome_contract() {
    ui::State<bool> scope_active{true};
    ui::State<bool> dynamic_visible{true};
    ui::State<bool> outside{false};
    ui::State<bool> dynamic_value{false};
    ui::State<bool> fallback_value{false};
    test::MockPlatform platform;

    ui::UI tree{
        ui::Column{
            ui::Toggle{"Outside", outside},
            ui::FocusScope{
                scope_active,
                ui::Row{
                    ui::If{dynamic_visible, ui::Toggle{"Dynamic", dynamic_value}},
                    ui::Toggle{"Fallback", fallback_value}
                }.gap(4.0f)}
                .trap(true)
                .default_focus(0)
        }.gap(4.0f).padding(0.0f)};

    tree.resize({320.0f, 120.0f});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(dynamic_value.get());
    NUI_CHECK(!outside.get());
    NUI_CHECK(!fallback_value.get());

    dynamic_visible.set(false);
    tree.resize({320.0f, 120.0f});
    tree.dispatch(test::key(ui::Key::Space), platform);

    // Removing the focused dynamic child must obey the still-active trapping
    // FocusScope and rehome inside it rather than escaping to the first global
    // focusable node.
    NUI_CHECK(!outside.get());
    NUI_CHECK(fallback_value.get());
}

void nested_focus_scope_rehome_contract() {
    ui::State<bool> outer_active{true};
    ui::State<bool> inner_active{true};
    ui::State<bool> inner_visible{true};
    ui::State<bool> global_value{false};
    ui::State<bool> outer_fallback{false};
    ui::State<bool> inner_value{false};
    test::MockPlatform platform;

    ui::UI tree{
        ui::Column{
            ui::Toggle{"Global", global_value},
            ui::FocusScope{
                outer_active,
                ui::Row{
                    ui::Toggle{"Outer fallback", outer_fallback},
                    ui::If{
                        inner_visible,
                        ui::FocusScope{inner_active, ui::Toggle{"Inner", inner_value}}
                            .trap(true)
                            .default_focus(0)}
                }.gap(4.0f)}
                .trap(true)
                .default_focus(1)
        }.gap(4.0f).padding(0.0f)};

    tree.resize({360.0f, 120.0f});
    tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(inner_value.get());
    NUI_CHECK(!global_value.get());
    NUI_CHECK(!outer_fallback.get());

    // The nearest trap is removed with the focused child, but the surviving
    // outer trap still owns focus. Rehoming must climb to that scope rather
    // than escaping to the global first focusable.
    inner_visible.set(false);
    tree.resize({360.0f, 120.0f});
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(!global_value.get());
    NUI_CHECK(outer_fallback.get());
}

void bounded_reconciliation_contract() {
    ui::State<bool> visible{true};
    auto loop = std::make_shared<LoopState>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, LoopProbe{visible, loop}}};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK(loop->mounts == 1);
    NUI_CHECK(loop->unmounts == 0);

    loop->armed = true;
    visible.set(false);

    tree.resize({160.0f, 80.0f});
    NUI_CHECK(loop->mounts == 17);
    NUI_CHECK(loop->unmounts == 16);
    NUI_CHECK(!visible.get());

    tree.resize({160.0f, 80.0f});
    NUI_CHECK(loop->mounts == 33);
    NUI_CHECK(loop->unmounts == 32);
    NUI_CHECK(!visible.get());

    loop->armed = false;
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(loop->mounts == 33);
    NUI_CHECK(loop->unmounts == 33);
}

void suite() {
    conditional_contract();
    switch_contract();
    replacement_destroys_before_insert_contract();
    keyed_contract();
    coalesced_writes_contract();
    focus_capture_and_lifetime_contract();
    focus_scope_rehome_contract();
    nested_focus_scope_rehome_contract();
    bounded_reconciliation_contract();
}

} // namespace

int main() { return test::run("dynamic_composition", &suite); }
