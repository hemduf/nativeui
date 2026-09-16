#include "test_support.hpp"

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

struct DynamicReconcileFaultAccess {
    static void arm(Tree& tree, int stage) noexcept {
        tree.dynamic_reconcile_fault_stage_ = stage;
    }

    [[nodiscard]] static int armed_stage(const Tree& tree) noexcept {
        return tree.dynamic_reconcile_fault_stage_;
    }

    [[nodiscard]] static bool focus_rebuild_pending(const Tree& tree) noexcept {
        return tree.dynamic_focus_rebuild_pending_;
    }

    static Node* first_dynamic(Tree& tree, Node& node) noexcept {
        if (tree.dynamic_source(node)) return &node;
        for (auto& child : node.children) {
            if (auto* found = first_dynamic(tree, *child)) return found;
        }
        return nullptr;
    }

    static bool reconcile_first(Tree& tree) {
        if (!tree.root_) return false;
        auto* owner = first_dynamic(tree, *tree.root_);
        return owner ? tree.reconcile_dynamic_node(*owner) : false;
    }
};

} // namespace ui::detail

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

struct DynamicFaultPlan {
    bool throw_mount{};
    bool throw_activate{};
    bool throw_deactivate{};
    bool throw_unmount{};
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
    int destroyed{};
};

class FaultingDynamicProbeComponent final : public ui::Component {
public:
    FaultingDynamicProbeComponent(
        std::string name,
        std::shared_ptr<DynamicLog> log,
        std::shared_ptr<DynamicFaultPlan> faults)
        : name_(std::move(name)), log_(std::move(log)), faults_(std::move(faults)) {}

    ~FaultingDynamicProbeComponent() override {
        ++faults_->destroyed;
        log_->events.push_back(name_ + ".destroy");
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override {
        ++faults_->mounts;
        log_->events.push_back(name_ + ".mount");
        log_->mounted_ids[name_].push_back(context.node_id());
        maybe_throw(faults_->throw_mount, "mount");
    }

    void activate(ui::LifecycleContext&) override {
        ++faults_->activates;
        log_->events.push_back(name_ + ".activate");
        maybe_throw(faults_->throw_activate, "activate");
    }

    void deactivate(ui::LifecycleContext&) override {
        ++faults_->deactivates;
        log_->events.push_back(name_ + ".deactivate");
        maybe_throw(faults_->throw_deactivate, "deactivate");
    }

    void unmount(ui::LifecycleContext&) override {
        ++faults_->unmounts;
        log_->events.push_back(name_ + ".unmount");
        maybe_throw(faults_->throw_unmount, "unmount");
    }

    void paint(ui::PaintContext&) const override {}

private:
    void maybe_throw(bool& armed, const char* phase) {
        if (!armed) return;
        armed = false;
        throw std::runtime_error(name_ + "." + phase);
    }

    std::string name_;
    std::shared_ptr<DynamicLog> log_;
    std::shared_ptr<DynamicFaultPlan> faults_;
};

class FaultingDynamicProbe {
public:
    FaultingDynamicProbe(
        std::string name,
        std::shared_ptr<DynamicLog> log,
        std::shared_ptr<DynamicFaultPlan> faults,
        std::vector<ui::Spec> children = {})
        : name_(std::move(name)),
          log_(std::move(log)),
          faults_(std::move(faults)),
          children_(std::move(children)) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        auto faults = std::move(faults_);
        auto children = std::move(children_);
        return ui::Spec{
            [name = std::move(name), log = std::move(log), faults = std::move(faults)]() mutable {
                return std::make_unique<FaultingDynamicProbeComponent>(
                    std::move(name), std::move(log), std::move(faults));
            },
            std::move(children)};
    }

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
    std::shared_ptr<DynamicFaultPlan> faults_;
    std::vector<ui::Spec> children_;
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

FaultingDynamicProbe faulting_subtree(
    const std::shared_ptr<DynamicLog>& log,
    const std::shared_ptr<DynamicFaultPlan>& parent,
    const std::shared_ptr<DynamicFaultPlan>& first,
    const std::shared_ptr<DynamicFaultPlan>& second,
    const std::shared_ptr<DynamicFaultPlan>& third) {
    std::vector<ui::Spec> children;
    children.push_back(FaultingDynamicProbe{"first", log, first}.spec());
    children.push_back(FaultingDynamicProbe{"second", log, second}.spec());
    children.push_back(FaultingDynamicProbe{"third", log, third}.spec());
    return FaultingDynamicProbe{"parent", log, parent, std::move(children)};
}

void partial_lifecycle_rollback_contract() {
    auto log = std::make_shared<DynamicLog>();
    auto parent = std::make_shared<DynamicFaultPlan>();
    auto first = std::make_shared<DynamicFaultPlan>();
    auto second = std::make_shared<DynamicFaultPlan>();
    auto third = std::make_shared<DynamicFaultPlan>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(faulting_subtree(log, parent, first, second, third).spec())};

    // RED before T130 exact-progress rollback: the recursive cleanup used to
    // call third.unmount even though third.mount had never begun.
    second->throw_mount = true;
    bool mount_threw = false;
    try {
        tree.mount();
    } catch (const std::runtime_error& error) {
        mount_threw = std::string{error.what()} == "second.mount";
    }
    NUI_CHECK(mount_threw);
    NUI_CHECK(parent->mounts == 1 && first->mounts == 1 && second->mounts == 1);
    NUI_CHECK(third->mounts == 0);
    NUI_CHECK(parent->unmounts == 1 && first->unmounts == 1 && second->unmounts == 1);
    NUI_CHECK(third->unmounts == 0);
    NUI_CHECK((log->events == std::vector<std::string>{
        "parent.mount", "first.mount", "second.mount",
        "second.unmount", "first.unmount", "parent.unmount"}));

    log->events.clear();
    tree.mount();
    NUI_CHECK((log->events == std::vector<std::string>{
        "parent.mount", "first.mount", "second.mount", "third.mount"}));

    // The same exact-progress rule applies to activation. The untouched third
    // sibling must not receive deactivate when second.activate aborts traversal.
    log->events.clear();
    second->throw_activate = true;
    bool activate_threw = false;
    try {
        tree.activate_focus(platform);
    } catch (const std::runtime_error& error) {
        activate_threw = std::string{error.what()} == "second.activate";
    }
    NUI_CHECK(activate_threw);
    NUI_CHECK(parent->activates == 1 && first->activates == 1 && second->activates == 1);
    NUI_CHECK(third->activates == 0);
    NUI_CHECK(parent->deactivates == 1 && first->deactivates == 1 && second->deactivates == 1);
    NUI_CHECK(third->deactivates == 0);
    NUI_CHECK((log->events == std::vector<std::string>{
        "parent.activate", "first.activate", "second.activate",
        "second.deactivate", "first.deactivate", "parent.deactivate"}));

    log->events.clear();
    tree.activate_focus(platform);
    tree.deactivate_focus(platform);
    tree.unmount();
}

void dynamic_partial_lifecycle_rollback_contract() {
    // Dynamic insertion delegates to the same lifecycle helpers but previously
    // tracked progress only at inserted-root granularity. A fault in the second
    // descendant therefore over-rolled the untouched third sibling.
    {
        ui::State<bool> visible{false};
        auto log = std::make_shared<DynamicLog>();
        auto parent = std::make_shared<DynamicFaultPlan>();
        auto first = std::make_shared<DynamicFaultPlan>();
        auto second = std::make_shared<DynamicFaultPlan>();
        auto third = std::make_shared<DynamicFaultPlan>();
        test::MockPlatform platform;

        ui::UI tree{ui::If{visible, faulting_subtree(log, parent, first, second, third)}};
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        second->throw_mount = true;
        visible.set(true);
        bool threw = false;
        try {
            tree.resize({160.0f, 80.0f});
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()} == "second.mount";
        }
        NUI_CHECK(threw);
        NUI_CHECK(parent->mounts == 1 && first->mounts == 1 && second->mounts == 1);
        NUI_CHECK(third->mounts == 0);
        NUI_CHECK(parent->unmounts == 1 && first->unmounts == 1 && second->unmounts == 1);
        NUI_CHECK(third->unmounts == 0);
    }

    {
        ui::State<bool> visible{false};
        auto log = std::make_shared<DynamicLog>();
        auto parent = std::make_shared<DynamicFaultPlan>();
        auto first = std::make_shared<DynamicFaultPlan>();
        auto second = std::make_shared<DynamicFaultPlan>();
        auto third = std::make_shared<DynamicFaultPlan>();
        test::MockPlatform platform;

        ui::UI tree{ui::If{visible, faulting_subtree(log, parent, first, second, third)}};
        tree.resize({160.0f, 80.0f});
        tree.activate(platform);

        second->throw_activate = true;
        visible.set(true);
        bool threw = false;
        try {
            tree.resize({160.0f, 80.0f});
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()} == "second.activate";
        }
        NUI_CHECK(threw);
        NUI_CHECK(parent->mounts == 1 && first->mounts == 1 && second->mounts == 1 && third->mounts == 1);
        NUI_CHECK(parent->activates == 1 && first->activates == 1 && second->activates == 1);
        NUI_CHECK(third->activates == 0);
        NUI_CHECK(parent->deactivates == 1 && first->deactivates == 1 && second->deactivates == 1);
        NUI_CHECK(third->deactivates == 0);
        NUI_CHECK(parent->unmounts == 1 && first->unmounts == 1 && second->unmounts == 1 && third->unmounts == 1);
    }
}

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

void dynamic_removal_lifecycle_failure_contract() {
    ui::State<std::vector<DynamicItem>> items{{
        DynamicItem{"A", "A"},
        DynamicItem{"B", "B"},
    }};
    auto log = std::make_shared<DynamicLog>();
    auto a_faults = std::make_shared<DynamicFaultPlan>();
    auto b_faults = std::make_shared<DynamicFaultPlan>();
    test::MockPlatform platform;

    ui::UI tree{ui::ForEach<DynamicItem>{
        items,
        [](const DynamicItem& item) { return item.key; },
        [log, a_faults, b_faults](const DynamicItem& item) {
            return FaultingDynamicProbe{
                item.name, log, item.key == "A" ? a_faults : b_faults};
        }
    }};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);

    b_faults->throw_deactivate = true;
    b_faults->throw_unmount = true;
    items.set({});

    bool threw = false;
    try {
        tree.resize({160.0f, 80.0f});
    } catch (const std::runtime_error& error) {
        threw = true;
        NUI_CHECK(std::string{error.what()} == "B.deactivate");
    }
    NUI_CHECK(threw);

    const std::vector<std::string> expected_prefix{
        "A.mount", "B.mount", "A.activate", "B.activate",
        "B.deactivate", "B.unmount", "A.deactivate", "A.unmount"};
    NUI_CHECK(log->events.size() == expected_prefix.size() + 2);
    for (std::size_t i = 0; i < expected_prefix.size(); ++i) {
        NUI_CHECK(log->events[i] == expected_prefix[i]);
    }
    NUI_CHECK(a_faults->destroyed == 1);
    NUI_CHECK(b_faults->destroyed == 1);

    const auto event_count = log->events.size();
    tree.deactivate(platform);
    NUI_CHECK(log->events.size() == event_count);
}

void dynamic_insert_mount_failure_contract() {
    ui::State<bool> visible{false};
    auto log = std::make_shared<DynamicLog>();
    auto faults = std::make_shared<DynamicFaultPlan>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, FaultingDynamicProbe{"child", log, faults}}};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);

    faults->throw_mount = true;
    visible.set(true);
    bool threw = false;
    try {
        tree.resize({160.0f, 80.0f});
    } catch (const std::runtime_error& error) {
        threw = true;
        NUI_CHECK(std::string{error.what()} == "child.mount");
    }
    NUI_CHECK(threw);
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.unmount", "child.destroy"}));
    NUI_CHECK(faults->destroyed == 1);

    const auto event_count = log->events.size();
    tree.deactivate(platform);
    NUI_CHECK(log->events.size() == event_count);
}

void dynamic_insert_activate_failure_contract() {
    ui::State<bool> visible{false};
    auto log = std::make_shared<DynamicLog>();
    auto faults = std::make_shared<DynamicFaultPlan>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, FaultingDynamicProbe{"child", log, faults}}};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);

    faults->throw_activate = true;
    visible.set(true);
    bool threw = false;
    try {
        tree.resize({160.0f, 80.0f});
    } catch (const std::runtime_error& error) {
        threw = true;
        NUI_CHECK(std::string{error.what()} == "child.activate");
    }
    NUI_CHECK(threw);
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount",
        "child.destroy"}));
    NUI_CHECK(faults->destroyed == 1);

    const auto event_count = log->events.size();
    tree.deactivate(platform);
    NUI_CHECK(log->events.size() == event_count);
}

void dynamic_allocation_transaction_family_contract() {
    using Access = ui::detail::DynamicReconcileFaultAccess;

    // B1: allocation while preparing removal storage must happen before any
    // deactivate/unmount callback or retained ownership move.
    {
        ui::State<std::vector<DynamicItem>> items{{
            DynamicItem{"A", "A"}, DynamicItem{"B", "B"}}};
        auto log = std::make_shared<DynamicLog>();
        test::MockPlatform platform;

        ui::Tree tree{ui::compile(
            ui::ForEach<DynamicItem>{
                items,
                [](const DynamicItem& item) { return item.key; },
                [log](const DynamicItem& item) { return DynamicProbe{item.name, log}; }}
                .spec())};
        tree.mount();
        tree.layout({160.0f, 80.0f});
        tree.activate_focus(platform);
        NUI_CHECK(log->events.size() == 4);

        items.set({DynamicItem{"A", "A"}});
        Access::arm(tree, 1);
        bool threw = false;
        try {
            (void)Access::reconcile_first(tree);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(log->events.size() == 4);

        NUI_CHECK(Access::reconcile_first(tree));
        tree.layout({160.0f, 80.0f});
        NUI_CHECK(log->events.size() == 6);
        NUI_CHECK(log->events[4] == "B.deactivate");
        NUI_CHECK(log->events[5] == "B.unmount");
        tree.deactivate_focus(platform);
        tree.unmount();
    }

    // B2: the desired DynamicRecord key snapshot is fully owned before child
    // publication. Injected bad_alloc therefore leaves the old empty structure,
    // and a normal retry mounts/activates the child exactly once.
    {
        ui::State<bool> visible{false};
        auto log = std::make_shared<DynamicLog>();
        test::MockPlatform platform;

        ui::Tree tree{ui::compile(ui::If{visible, DynamicProbe{"child", log}}.spec())};
        tree.mount();
        tree.layout({160.0f, 80.0f});
        tree.activate_focus(platform);

        visible.set(true);
        Access::arm(tree, 2);
        bool threw = false;
        try {
            (void)Access::reconcile_first(tree);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(log->events.empty());

        NUI_CHECK(Access::reconcile_first(tree));
        tree.layout({160.0f, 80.0f});
        NUI_CHECK((log->events == std::vector<std::string>{
            "child.mount", "child.activate"}));
        tree.deactivate_focus(platform);
        tree.unmount();
    }

    // B3: focus-registry allocation may fail only after structure/lifecycle are
    // coherent. The failure records a durable repair checkpoint; retry repairs
    // focus state without remounting or reactivating the inserted child.
    {
        ui::State<bool> visible{false};
        auto log = std::make_shared<DynamicLog>();
        test::MockPlatform platform;

        ui::Tree tree{ui::compile(ui::If{visible, DynamicProbe{"child", log}}.spec())};
        tree.mount();
        tree.layout({160.0f, 80.0f});
        tree.activate_focus(platform);

        visible.set(true);
        Access::arm(tree, 3);
        bool threw = false;
        try {
            (void)Access::reconcile_first(tree);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(Access::focus_rebuild_pending(tree));
        NUI_CHECK((log->events == std::vector<std::string>{
            "child.mount", "child.activate"}));

        NUI_CHECK(!Access::reconcile_first(tree));
        NUI_CHECK(!Access::focus_rebuild_pending(tree));
        tree.layout({160.0f, 80.0f});
        NUI_CHECK((log->events == std::vector<std::string>{
            "child.mount", "child.activate"}));
        tree.deactivate_focus(platform);
        tree.unmount();
    }

    // Insertion callback rollback must itself stay out of the allocation/focus
    // preparation path. Keeping stage 3 armed proves rollback did not attempt a
    // focus rebuild that could replace the original callback exception.
    {
        ui::State<bool> visible{false};
        auto log = std::make_shared<DynamicLog>();
        auto faults = std::make_shared<DynamicFaultPlan>();
        test::MockPlatform platform;

        ui::Tree tree{ui::compile(
            ui::If{visible, FaultingDynamicProbe{"child", log, faults}}.spec())};
        tree.mount();
        tree.layout({160.0f, 80.0f});
        tree.activate_focus(platform);

        faults->throw_mount = true;
        visible.set(true);
        Access::arm(tree, 3);
        bool threw = false;
        try {
            (void)Access::reconcile_first(tree);
        } catch (const std::runtime_error& error) {
            threw = std::string{error.what()} == "child.mount";
        }
        NUI_CHECK(threw);
        NUI_CHECK(Access::armed_stage(tree) == 3);
        NUI_CHECK(!Access::focus_rebuild_pending(tree));

        Access::arm(tree, 0);
        NUI_CHECK(Access::reconcile_first(tree));
        tree.layout({160.0f, 80.0f});
        NUI_CHECK(faults->mounts == 2);
        NUI_CHECK(faults->activates == 1);
        tree.deactivate_focus(platform);
        tree.unmount();
    }
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
    partial_lifecycle_rollback_contract();
    dynamic_partial_lifecycle_rollback_contract();
    conditional_contract();
    switch_contract();
    replacement_destroys_before_insert_contract();
    keyed_contract();
    coalesced_writes_contract();
    dynamic_removal_lifecycle_failure_contract();
    dynamic_insert_mount_failure_contract();
    dynamic_insert_activate_failure_contract();
    dynamic_allocation_transaction_family_contract();
    focus_capture_and_lifetime_contract();
    focus_scope_rehome_contract();
    nested_focus_scope_rehome_contract();
    bounded_reconciliation_contract();
}

} // namespace

int main() { return test::run("dynamic_composition", &suite); }
