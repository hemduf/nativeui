#include "test_support.hpp"

#include <nativeui/command.hpp>

#include <memory>

namespace {

struct CommandProbeState {
    int command_events{};
    int raw_key_events{};
    ui::Command last{ui::Command::None};
};

class CommandProbeComponent final : public ui::Component {
public:
    explicit CommandProbeComponent(std::shared_ptr<CommandProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 40.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::Command) {
            ++state_->command_events;
            state_->last = event.command;
            return ui::EventResult::Ignored;
        }
        if (event.type == ui::InputType::KeyDown) ++state_->raw_key_events;
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<CommandProbeState> state_;
};

class CommandProbe {
public:
    explicit CommandProbe(std::shared_ptr<CommandProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{[state = std::move(state)] {
            return std::make_unique<CommandProbeComponent>(state);
        }, {}};
    }

private:
    std::shared_ptr<CommandProbeState> state_;
};

struct ReconcileProbeState {
    int mounts{};
    int unmounts{};
};

class ReconcileProbeComponent final : public ui::Component {
public:
    explicit ReconcileProbeComponent(std::shared_ptr<ReconcileProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {1.0f, 1.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ReconcileProbeState> state_;
};

class ReconcileProbe {
public:
    explicit ReconcileProbe(std::shared_ptr<ReconcileProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{[state = std::move(state)] {
            return std::make_unique<ReconcileProbeComponent>(state);
        }, {}};
    }

private:
    std::shared_ptr<ReconcileProbeState> state_;
};

ui::InputEvent command_event(ui::Command command) {
    ui::InputEvent event{};
    event.type = ui::InputType::Command;
    event.command = command;
    return event;
}

void suite() {
    {
        ui::InputEvent event{};
        event.type = ui::InputType::KeyDown;
        event.key = ui::Key::C;
        event.ctrl = true;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::None);
        event.primary = true;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Copy);
        event.key = ui::Key::Z;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Undo);
        event.shift = true;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Redo);
        event.type = ui::InputType::TextInput;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::None);
    }

    {
        auto probe = std::make_shared<CommandProbeState>();
        int inner_calls = 0;
        int outer_calls = 0;
        int global_calls = 0;
        bool inner_handles = true;

        ui::UI tree{
            ui::CommandScope{[&](ui::Command command) {
                ++outer_calls;
                return command == ui::Command::Copy ? ui::EventResult::Handled
                                                     : ui::EventResult::Ignored;
            },
            ui::CommandScope{[&](ui::Command command) {
                ++inner_calls;
                return inner_handles && command == ui::Command::Copy
                    ? ui::EventResult::Handled
                    : ui::EventResult::Ignored;
            }, CommandProbe{probe}}}
        };
        tree.set_command_handler([&](ui::Command) {
            ++global_calls;
            return ui::EventResult::Handled;
        });

        test::MockPlatform platform;
        tree.resize({220.0f, 120.0f});
        tree.activate(platform);

        tree.dispatch(test::key(ui::Key::C, false, true), platform);
        NUI_CHECK(probe->command_events == 1);
        NUI_CHECK(probe->raw_key_events == 0);
        NUI_CHECK(probe->last == ui::Command::Copy);
        NUI_CHECK(inner_calls == 1);
        NUI_CHECK(outer_calls == 0);
        NUI_CHECK(global_calls == 0);

        inner_handles = false;
        tree.dispatch(test::key(ui::Key::C, false, true), platform);
        NUI_CHECK(inner_calls == 2);
        NUI_CHECK(outer_calls == 1);
        NUI_CHECK(global_calls == 0);

        tree.dispatch(test::key(ui::Key::V, false, true), platform);
        NUI_CHECK(inner_calls == 3);
        NUI_CHECK(outer_calls == 2);
        NUI_CHECK(global_calls == 1);
    }

    {
        ui::State<std::string> text{"NativeUI"};
        int global_copy = 0;
        ui::UI tree{ui::TextInput{"Text", text}};
        tree.set_command_handler([&](ui::Command command) {
            if (command == ui::Command::Copy) ++global_copy;
            return ui::EventResult::Handled;
        });
        test::MockPlatform platform;
        tree.resize({500.0f, 140.0f});
        tree.activate(platform);

        tree.dispatch(test::key(ui::Key::A, false, true), platform);
        tree.dispatch(test::key(ui::Key::C, false, true), platform);
        NUI_CHECK(platform.clipboard == "NativeUI");
        NUI_CHECK(global_copy == 0);
        NUI_CHECK(text.get() == "NativeUI");
    }

    // T125: nested dispatch must restore the exact previous dispatch depth when
    // the inner global command callback throws. A successful nested dispatch
    // after the caught fault must remain nested (and therefore must not run the
    // outermost structural checkpoint early), while the real outer dispatch
    // must still run that checkpoint before returning.
    {
        ui::State<bool> shown{false};
        auto lifecycle = std::make_shared<ReconcileProbeState>();
        ui::UI tree{ui::If{shown, ReconcileProbe{lifecycle}}};
        test::MockPlatform platform;
        tree.resize({120.0f, 80.0f});
        tree.activate(platform);

        bool inside_outer = false;
        bool nested_fault_caught = false;
        bool reconciled_too_early = false;
        tree.set_command_handler([&](ui::Command command) -> ui::EventResult {
            if (command == ui::Command::Copy && !inside_outer) {
                inside_outer = true;
                try {
                    (void)tree.dispatch(command_event(ui::Command::Paste), platform);
                } catch (const std::runtime_error&) {
                    nested_fault_caught = true;
                }

                // Queue structural work from a later successful nested dispatch.
                // Exact depth restoration keeps it pending until this outer
                // callback returns and the outermost dispatch reaches finish().
                (void)tree.dispatch(command_event(ui::Command::SelectAll), platform);
                reconciled_too_early = lifecycle->mounts != 0;
                inside_outer = false;
                return ui::EventResult::Handled;
            }
            if (command == ui::Command::Paste) {
                throw std::runtime_error("nested command fault");
            }
            if (command == ui::Command::SelectAll) {
                shown.set(true);
                return ui::EventResult::Handled;
            }
            if (command == ui::Command::Cut) {
                shown.set(false);
                return ui::EventResult::Handled;
            }
            return ui::EventResult::Ignored;
        });

        NUI_CHECK(tree.dispatch(command_event(ui::Command::Copy), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(nested_fault_caught);
        NUI_CHECK(!reconciled_too_early);
        NUI_CHECK(lifecycle->mounts == 1);
        NUI_CHECK(lifecycle->unmounts == 0);

        // A later top-level command remains a genuine outer dispatch and its
        // reconciliation checkpoint is still functional after the prior fault.
        NUI_CHECK(tree.dispatch(command_event(ui::Command::Cut), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(lifecycle->unmounts == 1);
    }
}

} // namespace

int main() {
    return test::run("command", suite);
}
