#include "example_support.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

struct ShortcutState {
    int calls{};
    ui::Key last_key{ui::Key::None};
    bool primary{};
    bool shift{};
    bool alt{};
};

class ShortcutStatusComponent final : public ui::Component {
public:
    explicit ShortcutStatusComponent(std::shared_ptr<ShortcutState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {560.0f, 120.0f};
    }

    void paint(ui::PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 10.0f, ui::colors::panel);
        painter.stroke_rounded_rect(bounds, 10.0f, 1.0f, ui::colors::border);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 32.0f},
            "T174 / PER-UI KEYDOWN FALLBACK",
            14.0f,
            ui::colors::text);
        painter.text(
            {bounds.x + 16.0f, bounds.y + 62.0f},
            "Keep the text field focused and press Primary+G or Primary+P.",
            11.0f,
            ui::colors::textMuted);

        const std::string status = state_->calls == 0
            ? "Fallback: not triggered yet"
            : "Fallback calls: " + std::to_string(state_->calls) +
                  "  primary=" + (state_->primary ? "true" : "false") +
                  "  shift=" + (state_->shift ? "true" : "false") +
                  "  alt=" + (state_->alt ? "true" : "false");
        painter.text(
            {bounds.x + 16.0f, bounds.y + 94.0f},
            status,
            11.0f,
            ui::colors::text);
    }

private:
    std::shared_ptr<ShortcutState> state_;
};

class ShortcutStatus {
public:
    explicit ShortcutStatus(std::shared_ptr<ShortcutState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ShortcutStatusComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<ShortcutState> state_;
};

class ConstructionProbeComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {20.0f, 20.0f};
    }

    void paint(ui::PaintContext&) const override {}
};

class ConstructionProbe {
public:
    explicit ConstructionProbe(std::shared_ptr<int> constructions)
        : constructions_(std::move(constructions)) {}

    ui::Spec spec() && {
        auto constructions = std::move(constructions_);
        return ui::Spec{
            [constructions = std::move(constructions)] {
                ++*constructions;
                return std::make_unique<ConstructionProbeComponent>();
            },
            {}};
    }

private:
    std::shared_ptr<int> constructions_;
};

ui::InputEvent primary_key(ui::Key key) {
    auto event = example::key(key);
    event.ctrl = true;
    event.primary = true;
    return event;
}

int run_self_test() {
    auto state = std::make_shared<ShortcutState>();
    ui::State<std::string> text{"hello"};
    ui::UI tree{ui::Column{
        ui::TextInput{"Text", text},
        ShortcutStatus{state},
    }};
    example::Platform platform;
    tree.resize({600.0f, 220.0f});
    tree.activate(platform);

    tree.set_key_down_handler([&](const ui::InputEvent& event) {
        ++state->calls;
        state->last_key = event.key;
        state->primary = event.primary;
        state->shift = event.shift;
        state->alt = event.alt;
        return ui::EventResult::Handled;
    });

    if (tree.dispatch(primary_key(ui::Key::G), platform) != ui::EventResult::Handled) {
        return example::fail("Primary+G did not resolve through the KeyDown fallback");
    }
    if (state->calls != 1 || state->last_key != ui::Key::G || !state->primary) {
        return example::fail("fallback did not receive the original Primary+G event");
    }

    ui::InputEvent text_event{};
    text_event.type = ui::InputType::TextInput;
    text_event.text = "!";
    tree.dispatch(text_event, platform);
    if (text.get() != "hello!" || state->calls != 1) {
        return example::fail("TextInput delivery changed while the fallback was registered");
    }

    if (tree.dispatch(primary_key(ui::Key::A), platform) != ui::EventResult::Handled) {
        return example::fail("Primary+A did not remain on the Command route");
    }
    if (state->calls != 1) {
        return example::fail("Command chord was double-delivered to the KeyDown fallback");
    }

    tree.set_key_down_handler({});
    if (tree.dispatch(primary_key(ui::Key::G), platform) != ui::EventResult::Ignored) {
        return example::fail("clearing the fallback did not restore Ignored behavior");
    }

    // T125 composition regression: a throwing fallback must restore the exact
    // dispatch depth. The following successful fallback queues an If mutation;
    // the new branch must be reconciled before dispatch returns. A stale depth
    // would suppress the outermost reconciliation checkpoint and leave the
    // construction count at zero.
    ui::State<bool> reveal{false};
    auto constructions = std::make_shared<int>(0);
    ui::UI recovery_tree{ui::Column{
        ui::Spacer{20.0f, 20.0f},
        ui::If{reveal, ConstructionProbe{constructions}},
    }};
    example::Platform recovery_platform;
    recovery_tree.resize({120.0f, 80.0f});
    recovery_tree.activate(recovery_platform);

    bool inject_failure = true;
    recovery_tree.set_key_down_handler([&](const ui::InputEvent&) {
        if (inject_failure) {
            inject_failure = false;
            throw std::runtime_error("injected T174 fallback failure");
        }
        reveal.set(true);
        return ui::EventResult::Handled;
    });

    bool threw = false;
    try {
        (void)recovery_tree.dispatch(example::key(ui::Key::G), recovery_platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) {
        return example::fail("throwing KeyDown fallback did not propagate");
    }
    if (*constructions != 0) {
        return example::fail("failed fallback unexpectedly published dynamic work");
    }
    if (recovery_tree.dispatch(example::key(ui::Key::P), recovery_platform) !=
        ui::EventResult::Handled) {
        return example::fail("KeyDown fallback did not recover after exception unwind");
    }
    if (*constructions != 1) {
        return example::fail("dispatch depth was not restored before fallback recovery");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();

    auto state = std::make_shared<ShortcutState>();
    ui::State<std::string> text{"Keep focus here"};
    ui::UI tree{ui::Column{
        ui::TextInput{"Text", text},
        ShortcutStatus{state},
    }};
    tree.set_key_down_handler([&](const ui::InputEvent& event) {
        ++state->calls;
        state->last_key = event.key;
        state->primary = event.primary;
        state->shift = event.shift;
        state->alt = event.alt;
        tree.invalidate();
        return ui::EventResult::Handled;
    });

    return example::run_window(
        tree,
        "NativeUI T174 - Per-UI KeyDown fallback",
        {620.0f, 260.0f});
}
