#pragma once

#include <functional>
#include <new>
#include <stdexcept>
#include <vector>

namespace ui::detail {

struct DynamicFaultAccess {
    static void fail_next_dynamic_enqueue(Tree& tree) noexcept {
        tree.fail_next_dynamic_enqueue_for_testing_ = true;
    }
};

} // namespace ui::detail

namespace t125_dynamic_recovery {

struct LifeState {
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
};

class LifeProbeComponent final : public ui::Component {
public:
    explicit LifeProbeComponent(std::shared_ptr<LifeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {72.0f, 28.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void activate(ui::LifecycleContext&) override { ++state_->activates; }
    void deactivate(ui::LifecycleContext&) override { ++state_->deactivates; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<LifeState> state_;
};

class LifeProbe {
public:
    explicit LifeProbe(std::shared_ptr<LifeState> state) : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<LifeProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<LifeState> state_;
};

class ReentrantThrowFactoryProbe {
public:
    ReentrantThrowFactoryProbe(
        std::shared_ptr<LifeState> state,
        std::shared_ptr<int> attempts,
        ui::State<bool>& reentrant_visible)
        : state_(std::move(state)),
          attempts_(std::move(attempts)),
          reentrant_visible_(&reentrant_visible) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        auto attempts = std::move(attempts_);
        auto* reentrant_visible = reentrant_visible_;
        return ui::Spec{
            [state = std::move(state), attempts = std::move(attempts), reentrant_visible] {
                ++*attempts;
                if (*attempts == 1) {
                    reentrant_visible->set(false);
                    throw std::runtime_error("t125 dynamic factory failure");
                }
                return std::make_unique<LifeProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<LifeState> state_;
    std::shared_ptr<int> attempts_;
    ui::State<bool>* reentrant_visible_{};
};

struct QuarantineState {
    int focus_in{};
    int focus_out{};
    int pointer_down{};
    int unmounts{};
    bool throw_blur_once{true};
    std::function<void()> on_blur;
};

class QuarantineProbeComponent final : public ui::Component {
public:
    explicit QuarantineProbeComponent(std::shared_ptr<QuarantineState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 32.0f};
    }
    void paint(ui::PaintContext&) const override {}

    void focus_changed(bool focused, ui::FocusContext&) override {
        if (focused) {
            ++state_->focus_in;
            return;
        }
        ++state_->focus_out;
        if (state_->on_blur) state_->on_blur();
        if (state_->throw_blur_once) {
            state_->throw_blur_once = false;
            throw std::runtime_error("t125 dynamic blur failure");
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;
        ++state_->pointer_down;
        context.capture_pointer();
        return ui::EventResult::Handled;
    }

    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }

private:
    std::shared_ptr<QuarantineState> state_;
};

class QuarantineProbe {
public:
    explicit QuarantineProbe(std::shared_ptr<QuarantineState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<QuarantineProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<QuarantineState> state_;
};

void enqueue_failure_preserves_all_dirty_owners() {
    ui::State<bool> first_visible{true};
    ui::State<bool> second_visible{true};
    auto first = std::make_shared<LifeState>();
    auto second = std::make_shared<LifeState>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Column{
            ui::If{first_visible, LifeProbe{first}},
            ui::If{second_visible, LifeProbe{second}}}
            .gap(4.0f)
            .padding(0.0f)))};
    tree.mount();
    tree.layout({200.0f, 96.0f});
    tree.activate_focus(platform);
    NUI_CHECK(first->mounts == 1 && first->activates == 1);
    NUI_CHECK(second->mounts == 1 && second->activates == 1);

    first_visible.set(false);
    ui::detail::DynamicFaultAccess::fail_next_dynamic_enqueue(tree);
    bool threw = false;
    try {
        second_visible.set(false);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    NUI_CHECK(threw);

    tree.layout({200.0f, 96.0f});
    NUI_CHECK(first->deactivates == 1 && first->unmounts == 1);
    NUI_CHECK(second->deactivates == 1 && second->unmounts == 1);

    first_visible.set(true);
    second_visible.set(true);
    tree.layout({200.0f, 96.0f});
    NUI_CHECK(first->mounts == 2 && first->activates == 2);
    NUI_CHECK(second->mounts == 2 && second->activates == 2);

    tree.deactivate_focus(platform);
    tree.unmount();
}

void factory_failure_preserves_captured_and_reentrant_work() {
    ui::State<int> selected{1};
    ui::State<bool> second_visible{true};
    ui::State<bool> third_visible{true};
    auto old_branch = std::make_shared<LifeState>();
    auto replacement = std::make_shared<LifeState>();
    auto second = std::make_shared<LifeState>();
    auto third = std::make_shared<LifeState>();
    auto attempts = std::make_shared<int>(0);
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Column{
            ui::Switch<int>{selected}
                .when(1, LifeProbe{old_branch})
                .when(2, ReentrantThrowFactoryProbe{replacement, attempts, third_visible}),
            ui::If{second_visible, LifeProbe{second}},
            ui::If{third_visible, LifeProbe{third}}}
            .gap(4.0f)
            .padding(0.0f)))};
    tree.mount();
    tree.layout({220.0f, 128.0f});
    tree.activate_focus(platform);

    selected.set(2);
    second_visible.set(false);

    bool threw = false;
    try {
        tree.layout({220.0f, 128.0f});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(*attempts == 1);
    NUI_CHECK(!third_visible.get());

    tree.layout({220.0f, 128.0f});
    NUI_CHECK(*attempts == 2);
    NUI_CHECK(replacement->mounts == 1 && replacement->activates == 1);
    NUI_CHECK(second->deactivates == 1 && second->unmounts == 1);
    NUI_CHECK(third->deactivates == 1 && third->unmounts == 1);

    second_visible.set(true);
    tree.layout({220.0f, 128.0f});
    NUI_CHECK(second->mounts == 2 && second->activates == 2);

    tree.deactivate_focus(platform);
    tree.unmount();
}

void quarantine_reuses_t130_desired_key_snapshot() {
    struct KeyedItem {
        int id{};
        bool operator==(const KeyedItem&) const = default;
    };

    ui::State<std::vector<KeyedItem>> items{{KeyedItem{1}}};
    int key_calls = 0;
    bool reject_extra_key_call = false;
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(ui::ForEach<KeyedItem>{
        items,
        [&](const KeyedItem& item) {
            ++key_calls;
            if (reject_extra_key_call && key_calls > 2) {
                throw std::runtime_error("duplicate desired-key snapshot");
            }
            return item.id;
        },
        [](const KeyedItem&) { return ui::Spacer{32.0f, 20.0f}; }}))};
    tree.mount();
    tree.layout({120.0f, 48.0f});
    tree.activate_focus(platform);

    key_calls = 0;
    reject_extra_key_call = true;
    items.set({KeyedItem{2}});

    bool threw = false;
    try {
        tree.layout({120.0f, 48.0f});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(!threw);
    NUI_CHECK(key_calls == 2);

    tree.deactivate_focus(platform);
    tree.unmount();
}

void teardown_quarantine_rejects_reentrant_pointer_publication() {
    ui::State<bool> visible{true};
    ui::State<bool> survivor_value{false};
    auto doomed = std::make_shared<QuarantineState>();
    test::MockPlatform platform;

    ui::Tree tree{ui::compile(ui::make_spec(
        ui::Row{
            ui::If{visible, QuarantineProbe{doomed}},
            ui::Toggle{"Survivor", survivor_value}}
            .gap(8.0f)))};
    tree.mount();
    tree.layout({220.0f, 64.0f});
    tree.activate_focus(platform);
    NUI_CHECK(doomed->focus_in == 1);

    ui::EventResult nested_result = ui::EventResult::Handled;
    doomed->on_blur = [&] {
        nested_result = tree.dispatch(
            test::pointer(ui::InputType::PointerDown, 20.0f, 16.0f), platform);
    };

    visible.set(false);
    bool threw = false;
    try {
        tree.layout({220.0f, 64.0f});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    NUI_CHECK(threw);
    NUI_CHECK(doomed->focus_out == 1);
    NUI_CHECK(doomed->pointer_down == 0);
    NUI_CHECK(nested_result == ui::EventResult::Ignored);

    tree.layout({220.0f, 64.0f});
    NUI_CHECK(doomed->focus_out == 1);
    NUI_CHECK(doomed->unmounts == 1);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 120.0f, 16.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerUp, 120.0f, 16.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(survivor_value.get());

    tree.deactivate_focus(platform);
    tree.unmount();
}

inline void run() {
    enqueue_failure_preserves_all_dirty_owners();
    factory_failure_preserves_captured_and_reentrant_work();
    quarantine_reuses_t130_desired_key_snapshot();
    teardown_quarantine_rejects_reentrant_pointer_publication();
}

} // namespace t125_dynamic_recovery
