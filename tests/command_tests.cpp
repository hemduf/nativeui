#include "test_support.hpp"

#include <nativeui/command.hpp>

#include <array>
#include <cstdint>
#include <memory>

namespace {

static_assert(static_cast<int>(ui::Key::None) == 0);
static_assert(static_cast<int>(ui::Key::Tab) == 1);
static_assert(static_cast<int>(ui::Key::Left) == 2);
static_assert(static_cast<int>(ui::Key::Right) == 3);
static_assert(static_cast<int>(ui::Key::Up) == 4);
static_assert(static_cast<int>(ui::Key::Down) == 5);
static_assert(static_cast<int>(ui::Key::Home) == 6);
static_assert(static_cast<int>(ui::Key::End) == 7);
static_assert(static_cast<int>(ui::Key::Backspace) == 8);
static_assert(static_cast<int>(ui::Key::Delete) == 9);
static_assert(static_cast<int>(ui::Key::Space) == 10);
static_assert(static_cast<int>(ui::Key::Enter) == 11);
static_assert(static_cast<int>(ui::Key::Escape) == 12);
static_assert(static_cast<int>(ui::Key::A) == 13);
static_assert(static_cast<int>(ui::Key::C) == 14);
static_assert(static_cast<int>(ui::Key::V) == 15);
static_assert(static_cast<int>(ui::Key::X) == 16);
static_assert(static_cast<int>(ui::Key::Y) == 17);
static_assert(static_cast<int>(ui::Key::Z) == 18);
static_assert(static_cast<int>(ui::Key::Quit) == 19);

constexpr std::array<ui::Key, 26> kLetterKeys{
    ui::Key::A, ui::Key::B, ui::Key::C, ui::Key::D, ui::Key::E, ui::Key::F, ui::Key::G,
    ui::Key::H, ui::Key::I, ui::Key::J, ui::Key::K, ui::Key::L, ui::Key::M, ui::Key::N,
    ui::Key::O, ui::Key::P, ui::Key::Q, ui::Key::R, ui::Key::S, ui::Key::T, ui::Key::U,
    ui::Key::V, ui::Key::W, ui::Key::X, ui::Key::Y, ui::Key::Z};

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

void suite() {
    {
        for (std::size_t i = 0; i < kLetterKeys.size(); ++i) {
            const auto lower = static_cast<std::uint32_t>('a') + static_cast<std::uint32_t>(i);
            const auto upper = static_cast<std::uint32_t>('A') + static_cast<std::uint32_t>(i);
            NUI_CHECK(ui::detail::translate_ascii_key(lower, false) == kLetterKeys[i]);
            NUI_CHECK(ui::detail::translate_ascii_key(upper, false) == kLetterKeys[i]);
        }

        NUI_CHECK(ui::detail::translate_ascii_key('P', true) == ui::Key::P);
        NUI_CHECK(ui::detail::translate_ascii_key('p', true) == ui::Key::P);
        NUI_CHECK(ui::detail::translate_ascii_key('Q', false) == ui::Key::Q);
        NUI_CHECK(ui::detail::translate_ascii_key('q', false) == ui::Key::Q);
        NUI_CHECK(ui::detail::translate_ascii_key('Q', true) == ui::Key::Quit);
        NUI_CHECK(ui::detail::translate_ascii_key('q', true) == ui::Key::Quit);
        NUI_CHECK(ui::detail::translate_ascii_key(' ', false) == ui::Key::Space);
        NUI_CHECK(ui::detail::translate_ascii_key('0', false) == ui::Key::None);
        NUI_CHECK(ui::detail::translate_ascii_key('@', false) == ui::Key::None);
        NUI_CHECK(ui::detail::translate_ascii_key('[', false) == ui::Key::None);
        NUI_CHECK(ui::detail::translate_ascii_key(0x100U, false) == ui::Key::None);
    }

    {
        ui::InputEvent event{};
        event.type = ui::InputType::KeyDown;
        event.primary = true;

        event.key = ui::Key::A;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::SelectAll);
        event.key = ui::Key::C;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Copy);
        event.key = ui::Key::X;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Cut);
        event.key = ui::Key::V;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Paste);
        event.key = ui::Key::Y;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Redo);
        event.key = ui::Key::Z;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Undo);
        event.shift = true;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::Redo);

        event.shift = false;
        event.key = ui::Key::P;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::None);
        event.key = ui::Key::Q;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::None);
        event.key = ui::Key::W;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::None);

        event.type = ui::InputType::TextInput;
        event.key = ui::Key::C;
        NUI_CHECK(ui::command_from_shortcut(event) == ui::Command::None);

        event.type = ui::InputType::KeyDown;
        event.primary = false;
        event.ctrl = true;
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
}

} // namespace

int main() {
    return test::run("command", suite);
}
