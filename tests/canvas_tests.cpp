#include "test_support.hpp"

#include <functional>

namespace {

struct SemanticFocusProbeState {
    int focus_in{};
    int focus_out{};
    int key_down{};
    std::function<void()> on_focus_out;
};

class SemanticFocusProbeComponent final : public ui::Component {
public:
    explicit SemanticFocusProbeComponent(std::shared_ptr<SemanticFocusProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override {}

    void focus_changed(bool focused, ui::FocusContext&) override {
        if (focused) {
            ++state_->focus_in;
            return;
        }
        ++state_->focus_out;
        if (state_->on_focus_out) state_->on_focus_out();
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown && event.key == ui::Key::Space) {
            ++state_->key_down;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

private:
    std::shared_ptr<SemanticFocusProbeState> state_;
};

class SemanticFocusProbe {
public:
    explicit SemanticFocusProbe(std::shared_ptr<SemanticFocusProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<SemanticFocusProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<SemanticFocusProbeState> state_;
};

struct SemanticHoverProbeState {
    std::string name;
    std::vector<std::string>* log{};
    std::function<void()> on_leave;
};

class SemanticHoverProbeComponent final
    : public ui::Component,
      public ui::detail::RetainedInteractionObserver {
public:
    explicit SemanticHoverProbeComponent(std::shared_ptr<SemanticHoverProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override {}

    ui::EventResult input(const ui::InputEvent&, ui::InputContext&) override {
        return ui::EventResult::Ignored;
    }

    void retained_pointer_hover_changed(bool hovered, bool, ui::Dispatcher) override {
        state_->log->push_back(state_->name + (hovered ? "+" : "-"));
        if (!hovered && state_->on_leave) {
            auto callback = std::move(state_->on_leave);
            callback();
        }
    }

private:
    std::shared_ptr<SemanticHoverProbeState> state_;
};

class SemanticHoverProbe {
public:
    explicit SemanticHoverProbe(std::shared_ptr<SemanticHoverProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<SemanticHoverProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<SemanticHoverProbeState> state_;
};

void suite() {
    test::MockPlatform platform;
    int down_count = 0;
    int move_count = 0;
    int up_count = 0;
    int key_count = 0;
    ui::Point down_local{};
    ui::Point captured_move_local{};

    ui::UI tree{
        ui::Padding{20.0f,
            ui::Canvas{
                ui::Size{200.0f, 100.0f},
                [](ui::CanvasContext2D&) {}}
                .on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    switch (event.type) {
                        case ui::InputType::PointerDown:
                            ++down_count;
                            down_local = event.position;
                            ctx.capture_pointer();
                            break;
                        case ui::InputType::PointerMove:
                            ++move_count;
                            captured_move_local = event.position;
                            break;
                        case ui::InputType::PointerUp:
                            ++up_count;
                            ctx.release_pointer();
                            break;
                        case ui::InputType::KeyDown:
                            if (event.key == ui::Key::Right) ++key_count;
                            break;
                        default:
                            break;
                    }
                })
        }
    };

    tree.resize({240.0f, 140.0f});
    tree.activate(platform);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 45.0f), platform);
    NUI_CHECK(down_count == 1);
    NUI_CHECK_NEAR(down_local.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(down_local.y, 25.0f, 0.0001f);

    // Pointer capture keeps routing to the canvas even far outside its bounds.
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    NUI_CHECK(move_count == 1);
    NUI_CHECK_NEAR(captured_move_local.x, 480.0f, 0.0001f);
    NUI_CHECK_NEAR(captured_move_local.y, 480.0f, 0.0001f);

    tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform);
    NUI_CHECK(up_count == 1);

    // Release ends capture: another outside motion is no longer delivered.
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    NUI_CHECK(move_count == 1);

    // PointerDown focused the interactive Canvas; keyboard now routes to it.
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(key_count == 1);

    // Deactivation must also clear toolkit-level capture.
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 45.0f), platform);
    NUI_CHECK(down_count == 2);
    tree.deactivate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    NUI_CHECK(move_count == 1);

    // T125 B1 family closeout: a focus callback may synchronously dispatch a
    // newer focus request. The nested request is coalesced instead of starting
    // a second semantic publisher while A->B is still inside A's blur callback.
    // The bounded recursion counter makes the old recursive failure observable
    // without hanging the test process.
    {
        auto a = std::make_shared<SemanticFocusProbeState>();
        auto b = std::make_shared<SemanticFocusProbeState>();
        auto c = std::make_shared<SemanticFocusProbeState>();
        test::MockPlatform focus_platform;
        ui::UI focus_tree{
            ui::Row{
                SemanticFocusProbe{a},
                SemanticFocusProbe{b},
                SemanticFocusProbe{c}}
                .gap(4.0f)};
        focus_tree.resize({340.0f, 60.0f});
        focus_tree.activate(focus_platform);
        NUI_CHECK(a->focus_in == 1);

        int nested_requests = 0;
        a->on_focus_out = [&] {
            ++nested_requests;
            if (nested_requests <= 8) {
                (void)focus_tree.dispatch(
                    test::pointer(ui::InputType::PointerDown, 220.0f, 20.0f),
                    focus_platform);
            }
        };

        (void)focus_tree.dispatch(test::key(ui::Key::Tab), focus_platform);
        NUI_CHECK(nested_requests == 1);
        NUI_CHECK(a->focus_out == 1);
        NUI_CHECK(b->focus_in == 1);
        NUI_CHECK(b->focus_out == 1);
        NUI_CHECK(c->focus_in == 1);
        NUI_CHECK(c->focus_out == 0);

        (void)focus_tree.dispatch(test::key(ui::Key::Space), focus_platform);
        NUI_CHECK(a->key_down == 0);
        NUI_CHECK(b->key_down == 0);
        NUI_CHECK(c->key_down == 1);
    }

    // T125 B2 family closeout: A->B hover notification reenters with a newer C
    // pointer move. B may finish the already-started outer suffix, but once C is
    // published authoritative no stale B observer-enter callback may follow.
    {
        std::vector<std::string> hover_log;
        auto a = std::make_shared<SemanticHoverProbeState>();
        auto b = std::make_shared<SemanticHoverProbeState>();
        auto c = std::make_shared<SemanticHoverProbeState>();
        a->name = "A";
        b->name = "B";
        c->name = "C";
        a->log = &hover_log;
        b->log = &hover_log;
        c->log = &hover_log;

        test::MockPlatform hover_platform;
        ui::UI hover_tree{
            ui::Row{
                SemanticHoverProbe{a},
                SemanticHoverProbe{b},
                SemanticHoverProbe{c}}
                .gap(4.0f)};
        hover_tree.resize({340.0f, 60.0f});
        hover_tree.activate(hover_platform);

        (void)hover_tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 20.0f, 20.0f), hover_platform);
        NUI_CHECK(hover_log.size() == 1);
        NUI_CHECK(hover_log[0] == "A+");

        a->on_leave = [&] {
            (void)hover_tree.dispatch(
                test::pointer(ui::InputType::PointerMove, 220.0f, 20.0f), hover_platform);
        };
        (void)hover_tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 120.0f, 20.0f), hover_platform);

        const std::vector<std::string> expected{"A+", "A-", "B+", "B-", "C+"};
        NUI_CHECK(hover_log == expected);

        const auto settled_count = hover_log.size();
        (void)hover_tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 220.0f, 20.0f), hover_platform);
        NUI_CHECK(hover_log.size() == settled_count);
    }
}

} // namespace

int main() { return test::run("canvas", &suite); }
