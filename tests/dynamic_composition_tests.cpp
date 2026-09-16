#include "test_support.hpp"

#include <functional>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui {
struct TreeTestAccess {
    static void fail_next_dynamic_enqueue(Tree& tree) noexcept {
        tree.fail_next_dynamic_enqueue_for_testing_ = true;
    }

    static void fail_next_focus_invalidation_publication(Tree& tree) noexcept {
        tree.fail_next_focus_invalidation_publication_for_testing_ = true;
    }

    static void fail_next_input_context_preparation(Tree& tree) noexcept {
        tree.fail_next_input_context_preparation_for_testing_ = true;
    }

    static void clear_paint_dirty(Tree& tree) noexcept {
        tree.paint_dirty_.clear();
    }

    static void seed_pending_focus(Tree& tree) noexcept {
        tree.pending_focus_target_ = kInvalidNodeId;
        tree.pending_focus_request_ = true;
    }

    static void seed_pending_hover(Tree& tree) {
        auto pending = std::make_unique<Tree::PointerHoverTransitionState>();
        pending->next = kInvalidNodeId;
        tree.pending_pointer_hover_transition_ = std::move(pending);
    }

    [[nodiscard]] static bool has_pending_focus(const Tree& tree) noexcept {
        return tree.pending_focus_request_;
    }

    [[nodiscard]] static bool has_pending_hover(const Tree& tree) noexcept {
        return static_cast<bool>(tree.pending_pointer_hover_transition_);
    }
};
} // namespace ui

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

class ThrowOnceDynamicProbe {
public:
    ThrowOnceDynamicProbe(
        std::string name,
        std::shared_ptr<DynamicLog> log,
        std::shared_ptr<int> attempts)
        : name_(std::move(name)), log_(std::move(log)), attempts_(std::move(attempts)) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        auto attempts = std::move(attempts_);
        return ui::Spec{
            [name = std::move(name), log = std::move(log), attempts = std::move(attempts)] {
                ++*attempts;
                if (*attempts == 1) throw std::runtime_error("dynamic factory failure");
                return std::make_unique<DynamicProbeComponent>(name, log);
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
    std::shared_ptr<int> attempts_;
};

class ReentrantThrowOnceDynamicProbe {
public:
    ReentrantThrowOnceDynamicProbe(
        std::string name,
        std::shared_ptr<DynamicLog> log,
        std::shared_ptr<int> attempts,
        ui::State<bool>& reentrant_visible)
        : name_(std::move(name)),
          log_(std::move(log)),
          attempts_(std::move(attempts)),
          reentrant_visible_(&reentrant_visible) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        auto attempts = std::move(attempts_);
        auto* reentrant_visible = reentrant_visible_;
        return ui::Spec{
            [name = std::move(name), log = std::move(log), attempts = std::move(attempts),
             reentrant_visible] {
                ++*attempts;
                if (*attempts == 1) {
                    reentrant_visible->set(false);
                    throw std::runtime_error("dynamic factory failure after reentrant mutation");
                }
                return std::make_unique<DynamicProbeComponent>(name, log);
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
    std::shared_ptr<int> attempts_;
    ui::State<bool>* reentrant_visible_{};
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

struct ReentrantBlurState {
    int focus_in{};
    int focus_out{};
    bool throw_on_focus_out{true};
    std::function<void()> on_focus_out;
};

class ReentrantBlurComponent final : public ui::Component {
public:
    explicit ReentrantBlurComponent(std::shared_ptr<ReentrantBlurState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }
    void paint(ui::PaintContext&) const override {}

    void focus_changed(bool focused, ui::FocusContext&) override {
        if (focused) {
            ++state_->focus_in;
            return;
        }
        ++state_->focus_out;
        if (state_->on_focus_out) state_->on_focus_out();
        if (state_->throw_on_focus_out) {
            throw std::runtime_error("persistent dynamic blur failure");
        }
    }

private:
    std::shared_ptr<ReentrantBlurState> state_;
};

class ReentrantBlurProbe {
public:
    explicit ReentrantBlurProbe(std::shared_ptr<ReentrantBlurState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ReentrantBlurComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<ReentrantBlurState> state_;
};

struct HoverLeaveState {
    int moves{};
    int leaves{};
    bool throw_on_leave{};
};

class HoverLeaveProbeComponent final : public ui::Component {
public:
    explicit HoverLeaveProbeComponent(std::shared_ptr<HoverLeaveState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }
    void paint(ui::PaintContext&) const override {}

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerMove) {
            ++state_->moves;
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerLeave) {
            ++state_->leaves;
            if (state_->throw_on_leave) {
                throw std::runtime_error("persistent pointer leave failure");
            }
        }
        return ui::EventResult::Ignored;
    }

private:
    std::shared_ptr<HoverLeaveState> state_;
};

class HoverLeaveProbe {
public:
    explicit HoverLeaveProbe(std::shared_ptr<HoverLeaveState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<HoverLeaveProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<HoverLeaveState> state_;
};

struct SemanticSuffixState {
    int focus_leave{};
    int hover_leave{};
    int key_down{};
    int pointer_moves{};
    int nested_caught{};
    bool throw_focus{};
    bool throw_hover{};
    std::function<void()> on_focus_leave;
    std::function<void()> on_hover_leave;
    std::function<void()> nested_dispatch;
};

class SemanticSuffixLeafComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit SemanticSuffixLeafComponent(std::shared_ptr<SemanticSuffixState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }
    void paint(ui::PaintContext&) const override {}

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown) {
            ++state_->key_down;
            if (state_->nested_dispatch) state_->nested_dispatch();
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerMove) {
            ++state_->pointer_moves;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void retained_focus_within_changed(bool focused, bool, ui::Dispatcher) override {
        if (focused) return;
        ++state_->focus_leave;
        if (state_->on_focus_leave) state_->on_focus_leave();
        if (state_->throw_focus) {
            throw std::runtime_error("persistent semantic focus leave failure");
        }
    }

    void retained_pointer_hover_changed(bool hovered, bool, ui::Dispatcher) override {
        if (hovered) return;
        ++state_->hover_leave;
        if (state_->on_hover_leave) state_->on_hover_leave();
        if (state_->throw_hover) {
            throw std::runtime_error("persistent semantic hover leave failure");
        }
    }

private:
    std::shared_ptr<SemanticSuffixState> state_;
};

class SemanticSuffixLeaf {
public:
    explicit SemanticSuffixLeaf(std::shared_ptr<SemanticSuffixState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<SemanticSuffixLeafComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<SemanticSuffixState> state_;
};

struct SemanticAncestorState {
    int focus_leave{};
    int hover_leave{};
};

class SemanticAncestorComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit SemanticAncestorComponent(std::shared_ptr<SemanticAncestorState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty() ? ui::Size{} : children.front().preferred;
    }

    void layout_children(
        ui::Rect bounds,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void paint(ui::PaintContext&) const override {}

    void retained_focus_within_changed(bool focused, bool, ui::Dispatcher) override {
        if (!focused) ++state_->focus_leave;
    }

    void retained_pointer_hover_changed(bool hovered, bool, ui::Dispatcher) override {
        if (!hovered) ++state_->hover_leave;
    }

private:
    std::shared_ptr<SemanticAncestorState> state_;
};

class SemanticAncestor {
public:
    template <class Child>
    SemanticAncestor(std::shared_ptr<SemanticAncestorState> state, Child&& child)
        : state_(std::move(state)), child_(ui::make_spec(std::forward<Child>(child))) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        std::vector<ui::Spec> children;
        children.push_back(std::move(child_));
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<SemanticAncestorComponent>(state);
            },
            std::move(children)};
    }

private:
    std::shared_ptr<SemanticAncestorState> state_;
    ui::Spec child_;
};

struct RecoveryTargetState {
    int key_down{};
    int pointer_moves{};
};

class RecoveryTargetComponent final : public ui::Component {
public:
    explicit RecoveryTargetComponent(std::shared_ptr<RecoveryTargetState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }
    void paint(ui::PaintContext&) const override {}

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown) {
            ++state_->key_down;
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerMove) {
            ++state_->pointer_moves;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

private:
    std::shared_ptr<RecoveryTargetState> state_;
};

class RecoveryTarget {
public:
    explicit RecoveryTarget(std::shared_ptr<RecoveryTargetState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<RecoveryTargetComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<RecoveryTargetState> state_;
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

void factory_failure_preserves_pending_dynamic_owners_contract() {
    ui::State<int> selected{1};
    ui::State<bool> second_visible{true};
    auto log = std::make_shared<DynamicLog>();
    auto attempts = std::make_shared<int>(0);
    test::MockPlatform platform;

    ui::UI tree{
        ui::Column{
            ui::Switch<int>{selected}
                .when(1, DynamicProbe{"old", log})
                .when(2, ThrowOnceDynamicProbe{"recovered", log, attempts}),
            ui::If{second_visible, DynamicProbe{"second", log}}
        }.gap(4.0f).padding(0.0f)};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK((log->events == std::vector<std::string>{
        "old.mount", "second.mount", "old.activate", "second.activate"}));

    selected.set(2);
    second_visible.set(false);

    bool threw = false;
    try {
        tree.resize({160.0f, 80.0f});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(*attempts == 1);
    NUI_CHECK((log->events == std::vector<std::string>{
        "old.mount", "second.mount", "old.activate", "second.activate",
        "old.deactivate", "old.unmount"}));

    tree.resize({160.0f, 80.0f});
    NUI_CHECK(*attempts == 2);
    NUI_CHECK((log->events == std::vector<std::string>{
        "old.mount", "second.mount", "old.activate", "second.activate",
        "old.deactivate", "old.unmount",
        "recovered.mount", "recovered.activate",
        "second.deactivate", "second.unmount"}));

    second_visible.set(true);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("second").size() == 2);
    NUI_CHECK(log->events[10] == "second.mount");
    NUI_CHECK(log->events[11] == "second.activate");
}

void factory_failure_preserves_reentrant_dynamic_mutation_contract() {
    ui::State<int> selected{1};
    ui::State<bool> second_visible{true};
    auto log = std::make_shared<DynamicLog>();
    auto attempts = std::make_shared<int>(0);
    test::MockPlatform platform;

    ui::UI tree{
        ui::Column{
            ui::Switch<int>{selected}
                .when(1, DynamicProbe{"old", log})
                .when(2, ReentrantThrowOnceDynamicProbe{
                    "recovered", log, attempts, second_visible}),
            ui::If{second_visible, DynamicProbe{"second", log}}
        }.gap(4.0f).padding(0.0f)};
    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK(log->mounted_ids.at("old").size() == 1);
    NUI_CHECK(log->mounted_ids.at("second").size() == 1);

    selected.set(2);

    bool threw = false;
    try {
        tree.resize({160.0f, 80.0f});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(*attempts == 1);
    NUI_CHECK(!second_visible.get());

    tree.resize({160.0f, 80.0f});
    NUI_CHECK(*attempts == 2);
    NUI_CHECK(log->mounted_ids.at("recovered").size() == 1);
    NUI_CHECK(log->events.back() == "second.unmount");

    second_visible.set(true);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("second").size() == 2);
    NUI_CHECK(log->events.back() == "second.activate");
}

void enqueue_allocation_failure_preserves_dynamic_work_contract() {
    ui::State<bool> visible{true};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(ui::If{visible, DynamicProbe{"child", log}}))};
    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.activate_focus(platform);
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount", "child.activate"}));

    ui::TreeTestAccess::fail_next_dynamic_enqueue(tree);
    bool threw = false;
    try {
        visible.set(false);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount", "child.activate"}));

    tree.layout({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount"}));

    visible.set(true);
    tree.layout({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("child").size() == 2);
    NUI_CHECK(log->events[4] == "child.mount");
    NUI_CHECK(log->events[5] == "child.activate");
}

void enqueue_failure_preserves_previously_dirty_owner_contract() {
    ui::State<bool> first_visible{true};
    ui::State<bool> second_visible{true};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Column{
            ui::If{first_visible, DynamicProbe{"first", log}},
            ui::If{second_visible, DynamicProbe{"second", log}}
        }.gap(4.0f).padding(0.0f)))};
    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.activate_focus(platform);
    NUI_CHECK(log->mounted_ids.at("first").size() == 1);
    NUI_CHECK(log->mounted_ids.at("second").size() == 1);

    first_visible.set(false);
    ui::TreeTestAccess::fail_next_dynamic_enqueue(tree);
    bool threw = false;
    try {
        second_visible.set(false);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);

    tree.layout({160.0f, 80.0f});
    NUI_CHECK(log->events.size() == 8);
    NUI_CHECK(log->events[4] == "first.deactivate");
    NUI_CHECK(log->events[5] == "first.unmount");
    NUI_CHECK(log->events[6] == "second.deactivate");
    NUI_CHECK(log->events[7] == "second.unmount");

    first_visible.set(true);
    tree.layout({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("first").size() == 2);
    second_visible.set(true);
    tree.layout({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.at("second").size() == 2);
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

void dynamic_focus_clear_preserves_newer_reentrant_request_contract() {
    ui::State<bool> visible{true};
    ui::State<bool> fallback{false};
    auto blur = std::make_shared<ReentrantBlurState>();
    test::MockPlatform platform;

    ui::UI tree{
        ui::Row{
            ui::If{visible, ReentrantBlurProbe{blur}},
            ui::Toggle{"Fallback", fallback}}
            .gap(4.0f)};
    tree.resize({220.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK(blur->focus_in == 1);

    blur->on_focus_out = [&] {
        (void)tree.dispatch(test::key(ui::Key::Tab), platform);
    };
    visible.set(false);

    bool first_threw = false;
    try {
        tree.resize({220.0f, 80.0f});
    } catch (const std::runtime_error&) {
        first_threw = true;
    }
    NUI_CHECK(first_threw);
    NUI_CHECK(blur->focus_out == 1);

    bool recovery_threw = false;
    try {
        (void)tree.dispatch(test::key(ui::Key::Space), platform);
    } catch (const std::runtime_error&) {
        recovery_threw = true;
    }
    NUI_CHECK(!recovery_threw);
    NUI_CHECK(blur->focus_out == 1);
    NUI_CHECK(fallback.get());

    auto space_up = test::key(ui::Key::Space);
    space_up.type = ui::InputType::KeyUp;
    (void)tree.dispatch(space_up, platform);

    (void)tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(!fallback.get());
    NUI_CHECK(blur->focus_out == 1);
}

void focus_invalidation_prepare_publish_commit_contract() {
    {
        auto blur = std::make_shared<ReentrantBlurState>();
        blur->throw_on_focus_out = false;
        ui::State<bool> fallback{false};
        test::MockPlatform platform;
        ui::Tree tree{ui::compile(ui::make_spec(
            ui::Row{ReentrantBlurProbe{blur}, ui::Toggle{"Fallback", fallback}}.gap(4.0f)))};
        tree.mount();
        tree.layout({220.0f, 80.0f});
        tree.activate_focus(platform);
        ui::TreeTestAccess::clear_paint_dirty(tree);

        int invalidations = 0;
        tree.set_invalidation_callback([&](ui::Rect) { ++invalidations; });
        ui::TreeTestAccess::fail_next_focus_invalidation_publication(tree);

        bool threw = false;
        try {
            (void)tree.dispatch(test::key(ui::Key::Tab), platform);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(blur->focus_out == 1);
        NUI_CHECK(invalidations == 0);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(blur->focus_out == 1);
        NUI_CHECK(fallback.get());
        NUI_CHECK(invalidations == 2);
    }

    {
        auto blur = std::make_shared<ReentrantBlurState>();
        blur->throw_on_focus_out = false;
        ui::State<bool> fallback{false};
        test::MockPlatform platform;
        ui::Tree tree{ui::compile(ui::make_spec(
            ui::Row{ReentrantBlurProbe{blur}, ui::Toggle{"Fallback", fallback}}.gap(4.0f)))};
        tree.mount();
        tree.layout({220.0f, 80.0f});
        tree.activate_focus(platform);
        ui::TreeTestAccess::clear_paint_dirty(tree);

        int invalidations = 0;
        bool throw_once = true;
        tree.set_invalidation_callback([&](ui::Rect) {
            ++invalidations;
            if (throw_once) {
                throw_once = false;
                throw std::runtime_error("invalidation callback failure");
            }
        });

        bool threw = false;
        try {
            (void)tree.dispatch(test::key(ui::Key::Tab), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(blur->focus_out == 1);
        NUI_CHECK(invalidations == 1);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(blur->focus_out == 1);
        NUI_CHECK(fallback.get());
        NUI_CHECK(invalidations == 2);
    }
}

void hover_input_preparation_boundary_contract() {
    auto first = std::make_shared<HoverLeaveState>();
    auto second = std::make_shared<HoverLeaveState>();
    test::MockPlatform platform;
    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Row{HoverLeaveProbe{first}, HoverLeaveProbe{second}}.gap(4.0f)))};
    tree.mount();
    tree.layout({220.0f, 80.0f});
    tree.activate_focus(platform);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 20.0f, 15.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(first->moves == 1);

    ui::TreeTestAccess::fail_next_input_context_preparation(tree);
    bool prepare_threw = false;
    try {
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 120.0f, 15.0f), platform);
    } catch (const std::bad_alloc&) {
        prepare_threw = true;
    }
    NUI_CHECK(prepare_threw);
    NUI_CHECK(first->leaves == 0);
    NUI_CHECK(second->moves == 0);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 120.0f, 15.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(first->leaves == 1);
    NUI_CHECK(second->moves == 1);

    second->throw_on_leave = true;
    bool callback_threw = false;
    try {
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 20.0f, 15.0f), platform);
    } catch (const std::runtime_error&) {
        callback_threw = true;
    }
    NUI_CHECK(callback_threw);
    NUI_CHECK(second->leaves == 1);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 20.0f, 15.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(second->leaves == 1);
    NUI_CHECK(first->moves == 2);
}

void semantic_suffix_survives_measure_and_layout_contract() {
    for (int checkpoint = 0; checkpoint < 2; ++checkpoint) {
        ui::State<bool> visible{true};
        auto leaf = std::make_shared<SemanticSuffixState>();
        auto ancestor = std::make_shared<SemanticAncestorState>();
        auto target = std::make_shared<RecoveryTargetState>();
        test::MockPlatform platform;

        ui::Tree tree{ui::compile(ui::make_spec(
            ui::Row{
                SemanticAncestor{
                    ancestor,
                    ui::If{visible, ui::Row{SemanticSuffixLeaf{leaf}}}},
                RecoveryTarget{target}}
                .gap(4.0f)))};
        tree.mount();
        tree.layout({220.0f, 80.0f});
        tree.activate_focus(platform);

        leaf->throw_focus = true;
        leaf->on_focus_leave = [&] {
            visible.set(false);
            if (checkpoint == 0) {
                (void)tree.measure(ui::Constraints::loose({220.0f, 80.0f}));
            } else {
                tree.layout({220.0f, 80.0f});
            }
        };

        bool threw = false;
        try {
            (void)tree.dispatch(test::key(ui::Key::Tab), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(leaf->focus_leave == 1);
        NUI_CHECK(ancestor->focus_leave == 0);
        NUI_CHECK(!visible.get());

        if (checkpoint == 0) {
            (void)tree.measure(ui::Constraints::loose({220.0f, 80.0f}));
            (void)tree.measure(ui::Constraints::loose({220.0f, 80.0f}));
        } else {
            tree.layout({220.0f, 80.0f});
            tree.layout({220.0f, 80.0f});
        }
        NUI_CHECK(leaf->focus_leave == 1);
        NUI_CHECK(ancestor->focus_leave == 1);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(target->key_down == 1);
        NUI_CHECK(leaf->focus_leave == 1);
        NUI_CHECK(ancestor->focus_leave == 1);
    }
}

void semantic_hover_suffix_survives_public_checkpoint_contract() {
    ui::State<bool> visible{true};
    auto leaf = std::make_shared<SemanticSuffixState>();
    auto ancestor = std::make_shared<SemanticAncestorState>();
    auto target = std::make_shared<RecoveryTargetState>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Row{
            SemanticAncestor{
                ancestor,
                ui::If{visible, ui::Row{SemanticSuffixLeaf{leaf}}}},
            RecoveryTarget{target}}
            .gap(4.0f)))};
    tree.mount();
    tree.layout({220.0f, 80.0f});
    tree.activate_focus(platform);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 20.0f, 15.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(leaf->pointer_moves == 1);

    leaf->throw_hover = true;
    leaf->on_hover_leave = [&] {
        visible.set(false);
        tree.layout({220.0f, 80.0f});
    };

    bool threw = false;
    try {
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 120.0f, 15.0f), platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(leaf->hover_leave == 1);
    NUI_CHECK(ancestor->hover_leave == 0);

    tree.refresh_focus(platform);
    tree.refresh_focus(platform);
    NUI_CHECK(leaf->hover_leave == 1);
    NUI_CHECK(ancestor->hover_leave == 1);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 120.0f, 15.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(target->pointer_moves == 1);
    NUI_CHECK(leaf->hover_leave == 1);
    NUI_CHECK(ancestor->hover_leave == 1);
}

void nested_dispatch_finish_preserves_semantic_suffix_contract() {
    ui::State<bool> visible{true};
    auto leaf = std::make_shared<SemanticSuffixState>();
    auto ancestor = std::make_shared<SemanticAncestorState>();
    auto target = std::make_shared<RecoveryTargetState>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Row{
            SemanticAncestor{
                ancestor,
                ui::If{visible, ui::Row{SemanticSuffixLeaf{leaf}}}},
            RecoveryTarget{target}}
            .gap(4.0f)))};
    tree.mount();
    tree.layout({220.0f, 80.0f});
    tree.activate_focus(platform);

    leaf->throw_focus = true;
    leaf->on_focus_leave = [&] { visible.set(false); };
    leaf->nested_dispatch = [&] {
        try {
            (void)tree.dispatch(test::key(ui::Key::Tab), platform);
        } catch (const std::runtime_error&) {
            ++leaf->nested_caught;
        }
    };

    NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(leaf->nested_caught == 1);
    NUI_CHECK(leaf->focus_leave == 1);
    NUI_CHECK(ancestor->focus_leave == 1);
    NUI_CHECK(!visible.get());

    NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(target->key_down == 1);
    NUI_CHECK(leaf->focus_leave == 1);
    NUI_CHECK(ancestor->focus_leave == 1);

    auto isolated = std::make_shared<RecoveryTargetState>();
    ui::Tree second{ui::compile(ui::make_spec(RecoveryTarget{isolated}))};
    second.mount();
    second.layout({100.0f, 40.0f});
    second.activate_focus(platform);
    NUI_CHECK(second.dispatch(test::key(ui::Key::Space), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(isolated->key_down == 1);
    NUI_CHECK(target->key_down == 1);
}

void lifecycle_terminalizes_pending_semantic_work_contract() {
    ui::State<bool> value{false};
    test::MockPlatform platform;
    ui::Tree tree{ui::compile(ui::make_spec(ui::Toggle{"Value", value}))};
    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.activate_focus(platform);

    ui::TreeTestAccess::seed_pending_focus(tree);
    NUI_CHECK(ui::TreeTestAccess::has_pending_focus(tree));
    tree.deactivate_focus(platform);
    NUI_CHECK(!ui::TreeTestAccess::has_pending_focus(tree));

    ui::TreeTestAccess::seed_pending_focus(tree);
    ui::TreeTestAccess::seed_pending_hover(tree);
    tree.unmount();
    NUI_CHECK(!ui::TreeTestAccess::has_pending_focus(tree));
    NUI_CHECK(!ui::TreeTestAccess::has_pending_hover(tree));

    ui::TreeTestAccess::seed_pending_focus(tree);
    ui::TreeTestAccess::seed_pending_hover(tree);
    tree.mount();
    NUI_CHECK(!ui::TreeTestAccess::has_pending_focus(tree));
    NUI_CHECK(!ui::TreeTestAccess::has_pending_hover(tree));

    tree.layout({160.0f, 80.0f});
    tree.activate_focus(platform);
    (void)tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(value.get());
    tree.deactivate_focus(platform);
    tree.unmount();
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
    conditional_contract();
    switch_contract();
    replacement_destroys_before_insert_contract();
    factory_failure_preserves_pending_dynamic_owners_contract();
    factory_failure_preserves_reentrant_dynamic_mutation_contract();
    enqueue_allocation_failure_preserves_dynamic_work_contract();
    enqueue_failure_preserves_previously_dirty_owner_contract();
    keyed_contract();
    coalesced_writes_contract();
    focus_capture_and_lifetime_contract();
    dynamic_focus_clear_preserves_newer_reentrant_request_contract();
    focus_invalidation_prepare_publish_commit_contract();
    hover_input_preparation_boundary_contract();
    semantic_suffix_survives_measure_and_layout_contract();
    semantic_hover_suffix_survives_public_checkpoint_contract();
    nested_dispatch_finish_preserves_semantic_suffix_contract();
    lifecycle_terminalizes_pending_semantic_work_contract();
    focus_scope_rehome_contract();
    nested_focus_scope_rehome_contract();
    bounded_reconciliation_contract();
}

} // namespace

int main() { return test::run("dynamic_composition", &suite); }
