#include "example_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ui::DialogSpec confirm_spec(bool with_cancel = true) {
    ui::DialogSpec spec;
    spec.title = "Confirm operation";
    spec.backdrop_color = ui::Color{0.0f, 0.0f, 0.0f, 0.52f};
    spec.body = ui::make_spec(ui::Column{
        ui::Label{"This body is arbitrary retained NativeUI content."}.size(13.0f),
        ui::Label{"Enter confirms; Escape cancels."}.size(12.0f).color(ui::colors::textMuted),
    }.gap(8.0f).padding(0.0f));
    spec.actions.push_back(ui::DialogAction{
        "confirm", "Confirm", true, ui::DialogActionRole::Default});
    if (with_cancel) {
        spec.actions.push_back(ui::DialogAction{
            "cancel", "Cancel", true, ui::DialogActionRole::Cancel});
    }
    return spec;
}

struct DemoState {
    ui::Dialog* dialog{};
    std::string last_result{"No dialog result yet"};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T063 — Modal Dialog"},
            ui::Label{
                "One T061 modal dialog per UI. Default/Cancel actions, trapped focus, "
                "bounded scrolling and close-before-callback semantics."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::Button{"Open confirmation dialog", [&state] {
                if (!state.dialog) return;
                (void)state.dialog->show(confirm_spec(), [&state](ui::DialogResult result) {
                    if (result.kind == ui::DialogResultKind::Action) {
                        state.last_result = "Action: " + result.action_id;
                    } else {
                        state.last_result = "Dismissed";
                    }
                });
            }},
            ui::Label{state.last_result}.size(12.0f),
        }.gap(12.0f).padding(16.0f)};
}

struct KeySinkState {
    int key_down_count{};
    std::function<void()> on_key_down;
};

class KeySinkComponent final : public ui::Component {
public:
    explicit KeySinkComponent(std::shared_ptr<KeySinkState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {180.0f, 48.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown) {
            ++state_->key_down_count;
            auto callback = state_->on_key_down;
            if (callback) callback();
        }
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<KeySinkState> state_;
};

class KeySink {
public:
    explicit KeySink(std::shared_ptr<KeySinkState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)]() mutable {
                return std::make_unique<KeySinkComponent>(std::move(state));
            },
            {}};
    }

private:
    std::shared_ptr<KeySinkState> state_;
};

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

ui::InputEvent wheel(float x, float y, float dy) {
    ui::InputEvent event{};
    event.type = ui::InputType::PointerWheel;
    event.position = {x, y};
    event.delta = {0.0f, dy};
    return event;
}

bool pixels_equal_in_region(
    const std::vector<std::uint8_t>& a,
    const std::vector<std::uint8_t>& b,
    int width,
    int x0,
    int y0,
    int x1,
    int y1) {
    if (a.size() != b.size()) return false;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const auto index = static_cast<std::size_t>((y * width + x) * 4);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                if (a[index + channel] != b[index + channel]) return false;
            }
        }
    }
    return true;
}

bool pixels_differ_in_region(
    const std::vector<std::uint8_t>& a,
    const std::vector<std::uint8_t>& b,
    int width,
    int x0,
    int y0,
    int x1,
    int y1) {
    return !pixels_equal_in_region(a, b, width, x0, y0, x1, y1);
}

int default_action_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::Dialog dialog{tree};
    std::vector<ui::DialogResult> results;
    if (dialog.show(confirm_spec(), [&](ui::DialogResult result) {
            results.push_back(std::move(result));
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T063 default-action setup failed");
    }
    tree.resize({420.0f, 260.0f});

    if (tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
        results.size() != 1 || results.front().kind != ui::DialogResultKind::Action ||
        results.front().action_id != "confirm" || dialog.active()) {
        return example::fail("T063 Enter did not complete the enabled Default action exactly once");
    }

    (void)tree.dispatch(example::key(ui::Key::Enter), platform);
    if (results.size() != 1) {
        return example::fail("T063 repeated Enter completed the same dialog more than once");
    }
    return 0;
}

int cancel_action_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::Dialog dialog{tree};
    std::vector<ui::DialogResult> results;
    if (dialog.show(confirm_spec(), [&](ui::DialogResult result) {
            results.push_back(std::move(result));
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T063 cancel-action setup failed");
    }
    tree.resize({420.0f, 260.0f});

    if (tree.dispatch(example::key(ui::Key::Escape), platform) != ui::EventResult::Handled ||
        results.size() != 1 || results.front().kind != ui::DialogResultKind::Action ||
        results.front().action_id != "cancel" || dialog.active()) {
        return example::fail("T063 Escape did not complete the enabled Cancel action exactly once");
    }
    return 0;
}

int escape_dismiss_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::Dialog dialog{tree};
    std::vector<ui::DialogResult> results;
    if (dialog.show(confirm_spec(false), [&](ui::DialogResult result) {
            results.push_back(std::move(result));
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T063 Escape-dismiss setup failed");
    }
    tree.resize({420.0f, 260.0f});

    if (tree.dispatch(example::key(ui::Key::Escape), platform) != ui::EventResult::Handled ||
        results.size() != 1 || results.front().kind != ui::DialogResultKind::Dismissed ||
        !results.front().action_id.empty() || dialog.active()) {
        return example::fail("T063 Escape without Cancel did not produce one Dismissed result");
    }
    return 0;
}

int escape_preempts_focused_child_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    auto sink = std::make_shared<KeySinkState>();
    ui::DialogSpec spec;
    spec.title = "Escape contract";
    spec.backdrop_color = ui::Color{0.0f, 0.0f, 0.0f, 0.4f};
    spec.body = ui::make_spec(KeySink{sink});
    spec.actions.push_back(ui::DialogAction{
        "cancel", "Cancel", true, ui::DialogActionRole::Cancel});

    ui::Dialog dialog{tree};
    std::vector<ui::DialogResult> results;
    if (dialog.show(std::move(spec), [&](ui::DialogResult result) {
            results.push_back(std::move(result));
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T063 Escape-preemption setup failed");
    }
    tree.resize({420.0f, 260.0f});

    if (tree.dispatch(example::key(ui::Key::Escape), platform) != ui::EventResult::Handled ||
        sink->key_down_count != 0 || results.size() != 1 ||
        results.front().kind != ui::DialogResultKind::Action ||
        results.front().action_id != "cancel" || dialog.active()) {
        return example::fail("T063 Escape leaked to focused body before Dialog Cancel");
    }
    return 0;
}

int text_editor_enter_contract() {
    example::Platform platform;

    {
        ui::State<std::string> value{"single"};
        int submits = 0;
        std::vector<ui::DialogResult> results;
        ui::UI tree{ui::Spacer{320.0f, 180.0f}};
        tree.resize({420.0f, 260.0f});
        tree.activate(platform);

        ui::DialogSpec spec;
        spec.body = ui::make_spec(
            ui::TextInput{"Name", value}.on_submit([&](const std::string&) { ++submits; }));
        spec.actions.push_back(ui::DialogAction{
            "confirm", "Confirm", true, ui::DialogActionRole::Default});
        ui::Dialog dialog{tree};
        if (dialog.show(std::move(spec), [&](ui::DialogResult result) {
                results.push_back(std::move(result));
            }) != ui::DialogShowResult::Shown) {
            return example::fail("T063 TextInput Enter setup failed");
        }
        tree.resize({420.0f, 260.0f});
        (void)tree.dispatch(example::key(ui::Key::Tab), platform);
        if (tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
            submits != 1 || !results.empty() || !dialog.active()) {
            return example::fail("T063 TextInput Enter did not win before Default action");
        }
        if (!dialog.close()) return example::fail("T063 TextInput dialog did not close");
    }

    {
        ui::State<std::string> value{"multi"};
        std::vector<ui::DialogResult> results;
        ui::UI tree{ui::Spacer{320.0f, 180.0f}};
        tree.resize({420.0f, 260.0f});
        tree.activate(platform);

        ui::DialogSpec spec;
        spec.body = ui::make_spec(ui::TextArea{"Notes", value});
        spec.actions.push_back(ui::DialogAction{
            "confirm", "Confirm", true, ui::DialogActionRole::Default});
        ui::Dialog dialog{tree};
        if (dialog.show(std::move(spec), [&](ui::DialogResult result) {
                results.push_back(std::move(result));
            }) != ui::DialogShowResult::Shown) {
            return example::fail("T063 TextArea Enter setup failed");
        }
        tree.resize({420.0f, 260.0f});
        (void)tree.dispatch(example::key(ui::Key::Tab), platform);
        if (tree.dispatch(example::key(ui::Key::Enter), platform) != ui::EventResult::Handled ||
            value.get().find('\n') == std::string::npos || !results.empty() || !dialog.active()) {
            return example::fail("T063 TextArea Enter did not insert newline before Default action");
        }
        if (!dialog.close()) return example::fail("T063 TextArea dialog did not close");
    }

    return 0;
}

int reentrant_ui_destruction_contract() {
    example::Platform platform;
    auto tree = std::make_unique<ui::UI>(ui::Spacer{320.0f, 180.0f});
    tree->resize({420.0f, 260.0f});
    tree->activate(platform);

    ui::Dialog dialog{*tree};
    int completions = 0;
    if (dialog.show(confirm_spec(false), [&](ui::DialogResult result) {
            if (result.kind == ui::DialogResultKind::Action && result.action_id == "confirm") {
                ++completions;
            }
            tree.reset();
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T063 reentrant UI-destruction setup failed");
    }
    tree->resize({420.0f, 260.0f});

    auto* dispatch_target = tree.get();
    const auto result = dispatch_target->dispatch(example::key(ui::Key::Enter), platform);
    if (result != ui::EventResult::Handled || tree || completions != 1 || dialog.active()) {
        return example::fail("T063 completion could not safely destroy the invoking UI");
    }
    return 0;
}

int deactivation_suppresses_completion_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::Dialog dialog{tree};
    int completions = 0;
    if (dialog.show(confirm_spec(), [&](ui::DialogResult) { ++completions; }) !=
        ui::DialogShowResult::Shown) {
        return example::fail("T063 deactivation setup failed");
    }
    tree.resize({420.0f, 260.0f});
    tree.deactivate(platform);

    if (completions != 0 || dialog.active()) {
        return example::fail("T063 UI deactivation did not suppress and abandon the active Dialog");
    }

    tree.activate(platform);
    if (dialog.show(confirm_spec(), [&](ui::DialogResult) { ++completions; }) !=
        ui::DialogShowResult::Shown) {
        return example::fail("T063 Dialog controller was not reusable after UI reactivation");
    }
    tree.resize({420.0f, 260.0f});
    if (!dialog.close() || completions != 1) {
        return example::fail("T063 post-reactivation explicit close did not complete once");
    }
    return 0;
}

int close_failure_transaction_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::Dialog dialog{tree};
    std::vector<ui::DialogResult> results;
    if (dialog.show(confirm_spec(), [&](ui::DialogResult result) {
            results.push_back(std::move(result));
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T131 action-close failure setup failed");
    }
    tree.resize({420.0f, 260.0f});

    bool fail_invalidation = false;
    tree.set_invalidation_callback(std::function<void()>{[&] {
        if (fail_invalidation) throw std::runtime_error{"T131 injected close failure"};
    }});
    fail_invalidation = true;

    bool action_threw = false;
    try {
        (void)tree.dispatch(example::key(ui::Key::Enter), platform);
    } catch (const std::runtime_error&) {
        action_threw = true;
    }
    if (!action_threw || !dialog.active() || !results.empty() ||
        tree.overlay_entries().size() != 1) {
        fail_invalidation = false;
        tree.clear_invalidation_callback();
        return example::fail("T131 failed action close lost Dialog repair state");
    }

    fail_invalidation = false;
    if (!dialog.close() || dialog.active() || results.size() != 1 ||
        results.front().kind != ui::DialogResultKind::Action ||
        results.front().action_id != "confirm" || !tree.overlay_entries().empty()) {
        tree.clear_invalidation_callback();
        return example::fail("T131 action close did not recover with original completion result");
    }

    int dismissed = 0;
    if (dialog.show(confirm_spec(false), [&](ui::DialogResult result) {
            if (result.kind == ui::DialogResultKind::Dismissed) ++dismissed;
        }) != ui::DialogShowResult::Shown) {
        tree.clear_invalidation_callback();
        return example::fail("T131 programmatic-close retry setup failed");
    }
    tree.resize({420.0f, 260.0f});
    fail_invalidation = true;
    bool programmatic_threw = false;
    try {
        (void)dialog.close();
    } catch (const std::runtime_error&) {
        programmatic_threw = true;
    }
    if (!programmatic_threw || !dialog.active() || dismissed != 0 ||
        tree.overlay_entries().size() != 1) {
        fail_invalidation = false;
        tree.clear_invalidation_callback();
        return example::fail("T131 failed programmatic close lost Dialog repair state");
    }

    fail_invalidation = false;
    if (!dialog.close() || dialog.active() || dismissed != 1 ||
        !tree.overlay_entries().empty()) {
        tree.clear_invalidation_callback();
        return example::fail("T131 programmatic close did not recover exactly once");
    }
    tree.clear_invalidation_callback();
    return 0;
}

int destructor_failure_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    bool fail_invalidation = false;
    int dismissed = 0;
    {
        ui::Dialog dialog{tree};
        if (dialog.show(confirm_spec(false), [&](ui::DialogResult result) {
                if (result.kind == ui::DialogResultKind::Dismissed) ++dismissed;
            }) != ui::DialogShowResult::Shown) {
            return example::fail("T131 destructor-close setup failed");
        }
        tree.resize({420.0f, 260.0f});
        tree.set_invalidation_callback(std::function<void()>{[&] {
            if (fail_invalidation) {
                throw std::runtime_error{"T131 injected destructor close failure"};
            }
        }});
        fail_invalidation = true;
    }

    fail_invalidation = false;
    tree.clear_invalidation_callback();
    if (dismissed != 1 || !tree.overlay_entries().empty()) {
        return example::fail("T131 Dialog destructor did not contain close failure and finish terminal cleanup");
    }

    int recovered = 0;
    {
        ui::Dialog dialog{tree};
        if (dialog.show(confirm_spec(false), [&](ui::DialogResult) { ++recovered; }) !=
            ui::DialogShowResult::Shown) {
            return example::fail("T131 destructor close stranded the per-UI slot");
        }
        tree.resize({420.0f, 260.0f});
        if (!dialog.close()) return example::fail("T131 post-destructor recovery close failed");
    }
    if (recovered != 1 || !tree.overlay_entries().empty()) {
        return example::fail("T131 Dialog slot was not reusable after destructor close fault");
    }

    int throwing_completions = 0;
    {
        ui::Dialog dialog{tree};
        if (dialog.show(confirm_spec(false), [&](ui::DialogResult) {
                ++throwing_completions;
                throw std::runtime_error{"T131 destructor completion failure"};
            }) != ui::DialogShowResult::Shown) {
            return example::fail("T131 destructor throwing-completion setup failed");
        }
        tree.resize({420.0f, 260.0f});
    }
    if (throwing_completions != 1 || !tree.overlay_entries().empty()) {
        return example::fail("T131 Dialog destructor did not contain application completion failure");
    }
    return 0;
}

int deferred_destroyed_controller_recovery_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    auto sink = std::make_shared<KeySinkState>();
    std::unique_ptr<ui::Dialog> dialog;
    int dismissed = 0;
    sink->on_key_down = [&] {
        if (!dialog) return;
        (void)dialog->close();
        dialog.reset();
    };

    ui::DialogSpec spec;
    spec.title = "Deferred destruction repair";
    spec.body = ui::make_spec(KeySink{sink});
    dialog = std::make_unique<ui::Dialog>(tree);
    if (dialog->show(std::move(spec), [&](ui::DialogResult result) {
            if (result.kind == ui::DialogResultKind::Dismissed) ++dismissed;
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T131 deferred destroyed-controller setup failed");
    }
    tree.resize({420.0f, 260.0f});

    bool fail_invalidation = false;
    tree.set_invalidation_callback(std::function<void()>{[&] {
        if (fail_invalidation) {
            throw std::runtime_error{"T131 injected deferred destroyed-controller failure"};
        }
    }});
    fail_invalidation = true;

    bool threw = false;
    try {
        (void)tree.dispatch(example::key(ui::Key::Enter), platform);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    fail_invalidation = false;
    if (!threw || dialog || sink->key_down_count != 1 || dismissed != 0 ||
        tree.overlay_entries().size() != 1) {
        tree.clear_invalidation_callback();
        return example::fail("T131 deferred close lost UI-owned repair state after controller destruction");
    }

    try {
        (void)tree.dispatch(key_up(ui::Key::Enter), platform);
    } catch (...) {
        tree.clear_invalidation_callback();
        return example::fail("T131 deferred destroyed-controller retry propagated after fault cleared");
    }
    tree.clear_invalidation_callback();
    if (dismissed != 1 || !tree.overlay_entries().empty()) {
        return example::fail("T131 deferred destroyed-controller retry did not finish exactly once");
    }

    ui::Dialog recovered{tree};
    int recovered_completions = 0;
    if (recovered.show(confirm_spec(false), [&](ui::DialogResult) {
            ++recovered_completions;
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T131 deferred destroyed-controller failure stranded Dialog Busy");
    }
    tree.resize({420.0f, 260.0f});
    if (!recovered.close() || recovered_completions != 1 || recovered.active()) {
        return example::fail("T131 Dialog slot was not reusable after deferred destroyed-controller repair");
    }
    return 0;
}

int whole_ui_teardown_suppresses_completion_contract() {
    example::Platform platform;
    int completions = 0;
    auto tree = std::make_unique<ui::UI>(ui::Spacer{320.0f, 180.0f});
    tree->resize({420.0f, 260.0f});
    tree->activate(platform);
    auto dialog = std::make_unique<ui::Dialog>(*tree);
    if (dialog->show(confirm_spec(false), [&](ui::DialogResult) { ++completions; }) !=
        ui::DialogShowResult::Shown) {
        return example::fail("T131 whole-UI teardown setup failed");
    }
    tree->resize({420.0f, 260.0f});
    tree.reset();
    if (completions != 0 || dialog->active()) {
        return example::fail("T131 whole-UI teardown invoked Dialog completion or left controller active");
    }
    dialog.reset();
    if (completions != 0) {
        return example::fail("T131 outliving Dialog invoked completion after UI teardown");
    }
    return 0;
}

int throwing_completion_releases_slot_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{320.0f, 180.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::Dialog dialog{tree};
    int throwing_completions = 0;
    if (dialog.show(confirm_spec(false), [&](ui::DialogResult) {
            ++throwing_completions;
            throw std::runtime_error{"T131 injected completion failure"};
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T131 throwing-completion setup failed");
    }
    tree.resize({420.0f, 260.0f});

    bool threw = false;
    try {
        (void)dialog.close();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw || throwing_completions != 1 || dialog.active() ||
        !tree.overlay_entries().empty()) {
        return example::fail("T131 throwing completion did not leave Dialog terminal");
    }

    int recovered_completions = 0;
    if (dialog.show(confirm_spec(false), [&](ui::DialogResult) { ++recovered_completions; }) !=
        ui::DialogShowResult::Shown) {
        return example::fail("T131 throwing completion stranded the per-UI Dialog slot");
    }
    tree.resize({420.0f, 260.0f});
    if (!dialog.close() || recovered_completions != 1 || dialog.active()) {
        return example::fail("T131 Dialog was not reusable after throwing completion");
    }
    return 0;
}

int overlay_command_failure_recovery_contract() {
    example::Platform platform;

    {
        ui::State<int> selection{1};
        ui::UI tree{ui::ComboBox<int>{
            selection,
            {{1, "One", true}, {2, "Two", true}}}};
        tree.resize({240.0f, 180.0f});
        tree.activate(platform);

        bool fail_invalidation = false;
        tree.set_invalidation_callback(std::function<void()>{[&] {
            if (fail_invalidation) {
                throw std::runtime_error{"T131 injected popup transaction failure"};
            }
        }});
        fail_invalidation = true;

        bool show_threw = false;
        try {
            (void)tree.dispatch(example::key(ui::Key::Down), platform);
        } catch (const std::runtime_error&) {
            show_threw = true;
        }
        fail_invalidation = false;
        if (!show_threw || !tree.overlay_entries().empty() || selection.get() != 1) {
            tree.clear_invalidation_callback();
            return example::fail("T131 ComboBox failed show left overlay/opener state wedged");
        }

        (void)tree.dispatch(key_up(ui::Key::Down), platform);
        (void)tree.dispatch(example::key(ui::Key::Down), platform);
        (void)tree.dispatch(key_up(ui::Key::Down), platform);
        if (tree.overlay_entries().size() != 1) {
            tree.clear_invalidation_callback();
            return example::fail("T131 ComboBox could not reopen after failed show");
        }
        (void)tree.dispatch(example::key(ui::Key::Down), platform);

        fail_invalidation = true;
        bool close_threw = false;
        try {
            (void)tree.dispatch(example::key(ui::Key::Enter), platform);
        } catch (const std::runtime_error&) {
            close_threw = true;
        }
        fail_invalidation = false;
        if (!close_threw || selection.get() != 1 || tree.overlay_entries().size() != 1) {
            tree.clear_invalidation_callback();
            return example::fail("T131 ComboBox failed close lost pending commit/overlay state");
        }

        (void)tree.dispatch(key_up(ui::Key::Enter), platform);
        tree.clear_invalidation_callback();
        if (selection.get() != 2 || !tree.overlay_entries().empty()) {
            return example::fail("T131 ComboBox close retry did not commit exactly once");
        }
    }

    {
        int actions = 0;
        ui::UI tree{ui::PopupMenu{
            "Actions",
            {ui::PopupMenuItem::action("Run", [&] { ++actions; })}}};
        tree.resize({240.0f, 180.0f});
        tree.activate(platform);
        (void)tree.dispatch(example::key(ui::Key::Enter), platform);
        (void)tree.dispatch(key_up(ui::Key::Enter), platform);
        if (tree.overlay_entries().size() != 1) {
            return example::fail("T131 PopupMenu retry setup did not open");
        }

        bool fail_invalidation = false;
        tree.set_invalidation_callback(std::function<void()>{[&] {
            if (fail_invalidation) {
                throw std::runtime_error{"T131 injected menu close failure"};
            }
        }});
        fail_invalidation = true;
        bool close_threw = false;
        try {
            (void)tree.dispatch(example::key(ui::Key::Enter), platform);
        } catch (const std::runtime_error&) {
            close_threw = true;
        }
        fail_invalidation = false;
        if (!close_threw || actions != 0 || tree.overlay_entries().size() != 1) {
            tree.clear_invalidation_callback();
            return example::fail("T131 PopupMenu failed close lost pending action/overlay state");
        }

        (void)tree.dispatch(key_up(ui::Key::Enter), platform);
        tree.clear_invalidation_callback();
        if (actions != 1 || !tree.overlay_entries().empty()) {
            return example::fail("T131 PopupMenu close retry did not invoke action exactly once");
        }
    }

    return 0;
}

int tooltip_failure_transaction_contract() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner owner{
        std::shared_ptr<ui::detail::DispatcherWakeBackend>{}, clock};

    bool fail_show = true;
    bool fail_hide = false;
    int shows = 0;
    int hides = 0;
    ui::detail::TooltipController controller{
        owner.dispatcher(),
        ui::DispatcherDuration::zero(),
        [&] {
            ++shows;
            if (fail_show) throw std::runtime_error{"T131 injected tooltip show failure"};
        },
        [&] {
            ++hides;
            if (fail_hide) throw std::runtime_error{"T131 injected tooltip hide failure"};
        }};

    controller.set_hovered(true);
    bool show_threw = false;
    try {
        (void)owner.checkpoint();
    } catch (const std::runtime_error&) {
        show_threw = true;
    }
    if (!show_threw || controller.visible() || shows != 1) {
        return example::fail("T131 Tooltip show failure left controller visible");
    }

    controller.set_hovered(false);
    fail_show = false;
    controller.set_hovered(true);
    (void)owner.checkpoint();
    if (!controller.visible() || shows != 2) {
        return example::fail("T131 Tooltip could not recover after failed presentation");
    }

    fail_hide = true;
    bool hide_threw = false;
    try {
        controller.cancel();
    } catch (const std::runtime_error&) {
        hide_threw = true;
    }
    if (!hide_threw || !controller.visible() || hides != 1) {
        fail_hide = false;
        return example::fail("T131 Tooltip hide failure discarded visible retry state");
    }

    fail_hide = false;
    controller.cancel();
    if (controller.visible() || hides != 2) {
        return example::fail("T131 Tooltip hide retry did not reach coherent hidden state");
    }
    return 0;
}

bool is_opaque_red(ui::Rgba8 pixel) {
    return pixel.r >= 250 && pixel.g <= 4 && pixel.b <= 4 && pixel.a >= 250;
}

bool near_pixel(int actual, int expected) {
    return actual >= expected - 1 && actual <= expected + 1;
}

int scroll_and_pointer_action_contract() {
    example::Platform platform;
    ui::UI tree{ui::Spacer{420.0f, 260.0f}};
    tree.resize({420.0f, 260.0f});
    tree.activate(platform);

    ui::DialogSpec spec;
    spec.title = "Fixed title";
    spec.body = ui::make_spec(ui::Spacer{1000.0f, 1000.0f});
    spec.actions.push_back(ui::DialogAction{
        "confirm", "Confirm", true, ui::DialogActionRole::Default});

    std::vector<ui::DialogResult> results;
    ui::Dialog dialog{tree};
    if (dialog.show(std::move(spec), [&](ui::DialogResult result) {
            results.push_back(std::move(result));
        }) != ui::DialogShowResult::Shown) {
        return example::fail("T063 ScrollView/action setup failed");
    }
    tree.resize({420.0f, 260.0f});

    ui::HeadlessRenderer renderer{{420.0f, 260.0f}};
    if (!renderer.render(tree)) return example::fail("T063 pre-scroll headless render failed");
    const auto before = renderer.rgba_pixels();

    if (tree.dispatch(wheel(200.0f, 120.0f, 80.0f), platform) != ui::EventResult::Handled) {
        return example::fail("T063 oversized body did not route wheel through T034 ScrollView");
    }
    if (!renderer.render(tree)) return example::fail("T063 post-scroll headless render failed");
    const auto after = renderer.rgba_pixels();

    const int width = renderer.pixel_width();
    if (!pixels_equal_in_region(before, after, width, 24, 24, 396, 72) ||
        !pixels_equal_in_region(before, after, width, 24, 172, 396, 236) ||
        !pixels_differ_in_region(before, after, width, 44, 80, 376, 168)) {
        return example::fail("T063 scrolling moved fixed title/actions or failed to move body scrollbar");
    }

    if (tree.dispatch(
            example::pointer(ui::InputType::PointerDown, 340.0f, 198.0f), platform) !=
            ui::EventResult::Handled ||
        tree.dispatch(
            example::pointer(ui::InputType::PointerUp, 340.0f, 198.0f), platform) !=
            ui::EventResult::Handled ||
        results.size() != 1 || results.front().kind != ui::DialogResultKind::Action ||
        results.front().action_id != "confirm" || dialog.active()) {
        return example::fail("T063 fixed action row did not remain pointer-operable after body scroll");
    }

    return 0;
}

int headless_sizing_and_backdrop_contract() {
    ui::UI tree{ui::Spacer{700.0f, 240.0f}};
    ui::DialogSpec spec;
    spec.backdrop_color = ui::Color{1.0f, 0.0f, 0.0f, 1.0f};
    spec.body = ui::make_spec(ui::Spacer{1000.0f, 1000.0f});

    ui::Dialog dialog{tree};
    if (dialog.show(std::move(spec), [](ui::DialogResult) {}) !=
        ui::DialogShowResult::Shown) {
        return example::fail("T063 headless sizing setup failed");
    }

    ui::HeadlessRenderer renderer{{700.0f, 240.0f}};
    if (!renderer.render(tree)) return example::fail("T063 headless render failed");
    if (!is_opaque_red(renderer.pixel(0, 0))) {
        return example::fail("T063 styleable backdrop does not cover the viewport");
    }

    int left = 0;
    while (left < renderer.pixel_width() && is_opaque_red(renderer.pixel(left, 120))) ++left;
    int right = renderer.pixel_width() - 1;
    while (right >= 0 && is_opaque_red(renderer.pixel(right, 120))) --right;
    int top = 0;
    while (top < renderer.pixel_height() && is_opaque_red(renderer.pixel(350, top))) ++top;
    int bottom = renderer.pixel_height() - 1;
    while (bottom >= 0 && is_opaque_red(renderer.pixel(350, bottom))) --bottom;

    if (!near_pixel(left, 70) || !near_pixel(right, 629) ||
        !near_pixel(top, 24) || !near_pixel(bottom, 215)) {
        return example::fail("T063 outer bounds do not honor 560px max width / 24px viewport margins");
    }

    renderer.resize({320.0f, 240.0f});
    if (!renderer.render(tree)) return example::fail("T063 compact headless render failed");
    left = 0;
    while (left < renderer.pixel_width() && is_opaque_red(renderer.pixel(left, 120))) ++left;
    right = renderer.pixel_width() - 1;
    while (right >= 0 && is_opaque_red(renderer.pixel(right, 120))) --right;
    if (!near_pixel(left, 24) || !near_pixel(right, 295)) {
        return example::fail("T063 compact outer width does not preserve 24px margins");
    }

    if (!dialog.close()) return example::fail("T063 headless dialog did not close");
    return 0;
}

int self_test() {
    if (const int result = default_action_contract(); result != 0) return result;
    if (const int result = cancel_action_contract(); result != 0) return result;
    if (const int result = escape_dismiss_contract(); result != 0) return result;
    if (const int result = escape_preempts_focused_child_contract(); result != 0) return result;
    if (const int result = text_editor_enter_contract(); result != 0) return result;
    if (const int result = reentrant_ui_destruction_contract(); result != 0) return result;
    if (const int result = deactivation_suppresses_completion_contract(); result != 0) return result;
    if (const int result = close_failure_transaction_contract(); result != 0) return result;
    if (const int result = destructor_failure_contract(); result != 0) return result;
    if (const int result = deferred_destroyed_controller_recovery_contract(); result != 0) return result;
    if (const int result = whole_ui_teardown_suppresses_completion_contract(); result != 0) return result;
    if (const int result = throwing_completion_releases_slot_contract(); result != 0) return result;
    if (const int result = overlay_command_failure_recovery_contract(); result != 0) return result;
    if (const int result = tooltip_failure_transaction_contract(); result != 0) return result;
    if (const int result = scroll_and_pointer_action_contract(); result != 0) return result;
    if (const int result = headless_sizing_and_backdrop_contract(); result != 0) return result;
    return 0;
}

int platform_smoke() {
    const char* stage = "application";
    try {
        ui::Application application;
        if (!application.valid()) {
            return example::fail(application.last_error().empty()
                                     ? "T063 platform application is invalid"
                                     : application.last_error());
        }

        stage = "standalone";
        DemoState standalone_state;
        auto standalone_ui = make_ui(standalone_state);
        ui::Dialog standalone_dialog{standalone_ui};
        standalone_state.dialog = &standalone_dialog;
        ui::StandaloneWindow standalone{
            application,
            standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T063 platform smoke",
                .size = {560.0f, 360.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(standalone.last_error().empty()
                                     ? "T063 standalone window is invalid"
                                     : standalone.last_error());
        }

        stage = "embedded";
        DemoState embedded_state;
        auto embedded_ui = make_ui(embedded_state);
        ui::Dialog embedded_dialog{embedded_ui};
        embedded_state.dialog = &embedded_dialog;
        ui::EmbeddedView embedded{
            embedded_ui, standalone.native_handle(), {460.0f, 300.0f}};
        if (!embedded.native_handle()) {
            return example::fail(embedded.last_error().empty()
                                     ? "T063 embedded view is invalid"
                                     : embedded.last_error());
        }

        standalone_ui.activate(standalone);
        embedded_ui.activate(embedded);
        if (standalone_dialog.show(confirm_spec(), [](ui::DialogResult) {}) !=
                ui::DialogShowResult::Shown ||
            embedded_dialog.show(confirm_spec(), [](ui::DialogResult) {}) !=
                ui::DialogShowResult::Shown) {
            return example::fail("T063 native modal show failed");
        }

        stage = "native-paint";
        for (int i = 0; i < 8; ++i) {
            (void)application.poll(0.0);
            (void)embedded.poll();
        }
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());

        (void)standalone_dialog.close();
        (void)embedded_dialog.close();
        return 0;
    } catch (const std::exception& error) {
        return example::fail(std::string{"T063 platform smoke "} + stage + ": " + error.what());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    if (argc == 2 && std::string_view{argv[1]} == "--platform-smoke") {
        return platform_smoke();
    }

    DemoState state;
    auto tree = make_ui(state);
    ui::Dialog dialog{tree};
    state.dialog = &dialog;
    return example::run_window(tree, "NativeUI T063 Dialog", {620.0f, 420.0f});
}