#include "example_support.hpp"

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace {

constexpr std::array<ui::Key, 26> kLetterKeys{
    ui::Key::A, ui::Key::B, ui::Key::C, ui::Key::D, ui::Key::E, ui::Key::F, ui::Key::G,
    ui::Key::H, ui::Key::I, ui::Key::J, ui::Key::K, ui::Key::L, ui::Key::M, ui::Key::N,
    ui::Key::O, ui::Key::P, ui::Key::Q, ui::Key::R, ui::Key::S, ui::Key::T, ui::Key::U,
    ui::Key::V, ui::Key::W, ui::Key::X, ui::Key::Y, ui::Key::Z};

std::string letter_name(ui::Key key) {
    for (std::size_t i = 0; i < kLetterKeys.size(); ++i) {
        if (kLetterKeys[i] == key) {
            return std::string(1, static_cast<char>('A' + static_cast<int>(i)));
        }
    }
    return "?";
}

struct LetterState {
    int events{};
    ui::InputType last_type{ui::InputType::None};
    ui::Key last_key{ui::Key::None};
    bool primary{};
    bool shift{};
};

class LetterProbeComponent final : public ui::Component {
public:
    explicit LetterProbeComponent(std::shared_ptr<LetterState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {560.0f, 190.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type != ui::InputType::KeyDown && event.type != ui::InputType::KeyUp) {
            return ui::EventResult::Ignored;
        }

        ++state_->events;
        state_->last_type = event.type;
        state_->last_key = event.key;
        state_->primary = event.primary;
        state_->shift = event.shift;
        context.invalidate();
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 12.0f, ui::colors::panel);
        painter.stroke_rounded_rect(
            bounds,
            12.0f,
            context.focused() ? 2.0f : 1.0f,
            context.focused() ? ui::colors::accent : ui::colors::border);

        painter.text(
            {bounds.x + 18.0f, bounds.y + 34.0f},
            "T173 / LETTER KEYS A-Z",
            15.0f,
            ui::colors::text);
        painter.text(
            {bounds.x + 18.0f, bounds.y + 68.0f},
            "Click to focus, then press any letter. Primary+Shift+P stays a raw KeyDown.",
            11.0f,
            ui::colors::textMuted);

        const std::string event_name = state_->last_type == ui::InputType::KeyUp ? "KeyUp" : "KeyDown";
        const std::string status = state_->events == 0
            ? "Last event: none"
            : "Last event: " + event_name + " Key::" + letter_name(state_->last_key) +
                  "  primary=" + (state_->primary ? "true" : "false") +
                  "  shift=" + (state_->shift ? "true" : "false");
        painter.text(
            {bounds.x + 18.0f, bounds.y + 112.0f},
            status,
            13.0f,
            ui::colors::text);
        painter.text(
            {bounds.x + 18.0f, bounds.y + 148.0f},
            "Primary+A/C/V/X/Y/Z keep their existing Command semantics; Primary+Q keeps window close.",
            10.0f,
            ui::colors::textMuted);
    }

private:
    std::shared_ptr<LetterState> state_;
};

class LetterProbe {
public:
    explicit LetterProbe(std::shared_ptr<LetterState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<LetterProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<LetterState> state_;
};

int run_self_test() {
    auto state = std::make_shared<LetterState>();
    ui::UI tree{LetterProbe{state}};
    example::Platform platform;
    tree.resize({600.0f, 220.0f});
    tree.activate(platform);

    tree.dispatch(example::key(ui::Key::Tab), platform);

    for (const auto key : kLetterKeys) {
        ui::InputEvent event = example::key(key);
        tree.dispatch(event, platform);
        if (state->last_type != ui::InputType::KeyDown ||
            state->last_key != key || state->primary || state->shift) {
            return example::fail("focused component did not receive an unmodified letter KeyDown");
        }
    }

    if (state->events != static_cast<int>(kLetterKeys.size())) {
        return example::fail("not every A-Z KeyDown reached the focused component");
    }

    ui::InputEvent palette = example::key(ui::Key::P, true);
    palette.primary = true;
    tree.dispatch(palette, platform);
    if (state->last_type != ui::InputType::KeyDown ||
        state->last_key != ui::Key::P || !state->primary || !state->shift) {
        return example::fail("Primary+Shift+P was not preserved as a raw KeyDown");
    }

    ui::InputEvent release = palette;
    release.type = ui::InputType::KeyUp;
    tree.dispatch(release, platform);
    if (state->last_type != ui::InputType::KeyUp ||
        state->last_key != ui::Key::P || !state->primary || !state->shift) {
        return example::fail("letter KeyUp did not preserve key and modifiers");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();

    auto state = std::make_shared<LetterState>();
    ui::UI tree{LetterProbe{state}};
    return example::run_window(tree, "NativeUI T173 - Letter keys A-Z", {620.0f, 260.0f});
}
