#include "test_support.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

bool accent(ui::Rgba8 p) {
    return p.r > 220 && p.g > 120 && p.g < 190 && p.b < 100 && p.a > 220;
}

struct CopyTrackedScrollObserver {
    explicit CopyTrackedScrollObserver(std::shared_ptr<int> copies)
        : copies(std::move(copies)) {}

    CopyTrackedScrollObserver(const CopyTrackedScrollObserver& other)
        : copies(other.copies) {
        ++*copies;
    }

    CopyTrackedScrollObserver(CopyTrackedScrollObserver&&) noexcept = default;
    CopyTrackedScrollObserver& operator=(const CopyTrackedScrollObserver&) = default;
    CopyTrackedScrollObserver& operator=(CopyTrackedScrollObserver&&) noexcept = default;

    void operator()(ui::Point) const {}

    std::shared_ptr<int> copies;
};

void scroll_state_observer_contract() {
    // One pass exposes one stable value; recursive writes settle synchronously
    // as a later pass and the latest recursive write wins.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        std::vector<float> first_values;
        std::vector<float> second_values;
        std::vector<float> second_visible_offsets;

        auto first = state.observe([&](ui::Point value) {
            first_values.push_back(value.x);
            if (value.x == 1.0f) {
                state.set_offset({2.0f, 0.0f});
                state.set_offset({3.0f, 0.0f});
            }
        });
        auto second = state.observe([&](ui::Point value) {
            second_values.push_back(value.x);
            second_visible_offsets.push_back(state.offset().x);
        });

        state.set_offset({1.0f, 0.0f});
        NUI_CHECK(first_values.size() == 2);
        NUI_CHECK(second_values.size() == 2);
        NUI_CHECK(first_values[0] == 1.0f);
        NUI_CHECK(first_values[1] == 3.0f);
        NUI_CHECK(second_values[0] == 1.0f);
        NUI_CHECK(second_values[1] == 3.0f);
        NUI_CHECK(second_visible_offsets[0] == 1.0f);
        NUI_CHECK(second_visible_offsets[1] == 3.0f);
        NUI_CHECK(state.offset().x == 3.0f);

        const auto callback_count = second_values.size();
        state.set_offset({3.0f, 0.0f});
        NUI_CHECK(second_values.size() == callback_count);
    }

    // Removing a later observer is immediate for the current pass, while an
    // observer added during a pass starts only on a later pass.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        std::optional<ui::ScrollState::Subscription> later;
        int later_calls = 0;
        auto remover = state.observe([&](ui::Point) { later->reset(); });
        later.emplace(state.observe([&](ui::Point) { ++later_calls; }));

        state.set_offset({1.0f, 0.0f});
        NUI_CHECK(later_calls == 0);
        NUI_CHECK(!later->active());

        ui::ScrollState addition{ui::ScrollAxis::Both};
        ui::ScrollState::Subscription added;
        int added_calls = 0;
        auto adder = addition.observe([&](ui::Point) {
            if (!added.active()) {
                added = addition.observe([&](ui::Point) { ++added_calls; });
            }
        });

        addition.set_offset({1.0f, 0.0f});
        NUI_CHECK(added.active());
        NUI_CHECK(added_calls == 0);
        addition.set_offset({2.0f, 0.0f});
        NUI_CHECK(added_calls == 1);
    }

    // Observer exceptions restore dispatch bookkeeping, discard recursive
    // pending work and do not implicitly deliver the unstarted suffix.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        bool throw_once = true;
        int suffix_calls = 0;
        std::vector<float> first_values;

        auto first = state.observe([&](ui::Point value) {
            first_values.push_back(value.x);
            if (throw_once) {
                throw_once = false;
                state.set_offset({2.0f, 0.0f});
                throw std::runtime_error{"scroll observer failure"};
            }
        });
        auto suffix = state.observe([&](ui::Point) { ++suffix_calls; });

        bool threw = false;
        try {
            state.set_offset({1.0f, 0.0f});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        NUI_CHECK(state.offset().x == 1.0f);
        NUI_CHECK(suffix_calls == 0);

        state.set_offset({3.0f, 0.0f});
        NUI_CHECK(state.offset().x == 3.0f);
        NUI_CHECK(suffix_calls == 1);
        NUI_CHECK(first_values.size() == 2);
        NUI_CHECK(first_values[0] == 1.0f);
        NUI_CHECK(first_values[1] == 3.0f);
    }

    // Callback-driven destruction is safe: the dispatch control block survives
    // the active callback, stops the suffix and leaves outliving subscriptions inactive.
    {
        auto state = std::make_unique<ui::ScrollState>(ui::ScrollAxis::Both);
        ui::ScrollState::Subscription destroyer;
        ui::ScrollState::Subscription suffix;
        int suffix_calls = 0;

        destroyer = state->observe([&](ui::Point) { state.reset(); });
        suffix = state->observe([&](ui::Point) { ++suffix_calls; });
        auto* raw = state.get();
        raw->set_offset({1.0f, 0.0f});

        NUI_CHECK(!state);
        NUI_CHECK(suffix_calls == 0);
        NUI_CHECK(!destroyer.active());
        NUI_CHECK(!suffix.active());
        destroyer.reset();
        suffix.reset();
    }

    // Stable observer scrolling must not clone the callback object on every mutation.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        auto copies = std::make_shared<int>(0);
        CopyTrackedScrollObserver observer{copies};
        auto subscription = state.observe(observer);
        const int registration_copies = *copies;

        state.set_offset({1.0f, 0.0f});
        state.set_offset({2.0f, 0.0f});
        state.set_offset({3.0f, 0.0f});
        NUI_CHECK(*copies == registration_copies);

        auto moved = std::move(subscription);
        NUI_CHECK(!subscription.active());
        NUI_CHECK(moved.active());
        moved.reset();
        NUI_CHECK(!moved.active());
    }
}

struct PointerEatingState {
    int down{};
    int move{};
    int up{};
};

class PointerEatingComponent final : public ui::Component {
public:
    explicit PointerEatingComponent(std::shared_ptr<PointerEatingState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 400.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++state_->down;
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerUp:
            ++state_->up;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<PointerEatingState> state_;
};

class PointerEatingContent {
public:
    explicit PointerEatingContent(std::shared_ptr<PointerEatingState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<PointerEatingComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<PointerEatingState> state_;
};

void suite() {
    scroll_state_observer_contract();

    // Vertical scrolling exposes metrics and repositions content by a clamped offset.
    {
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{
            ui::Scroll{state,
                ui::Canvas{ui::Size{80.0f, 240.0f}, [](ui::CanvasContext2D& g) {
                    g.fill_rect({0.0f, 0.0f, 80.0f, 20.0f}, ui::colors::accent);
                    g.fill_rect({0.0f, 100.0f, 80.0f, 20.0f}, ui::colors::accent);
                }}}};

        ui::HeadlessRenderer renderer{{80.0f, 80.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.viewport_size().w, 80.0f, 0.001f);
        NUI_CHECK_NEAR(state.viewport_size().h, 80.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().w, 80.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().h, 240.0f, 0.001f);
        NUI_CHECK(accent(renderer.pixel(10, 10)));

        state.set_offset({0.0f, 100.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 0.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);
        NUI_CHECK(accent(renderer.pixel(10, 10)));

        state.set_offset({50.0f, 1000.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 0.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 160.0f, 0.001f);

        state.scroll_by({0.0f, -1000.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);

        int observed = 0;
        auto subscription = state.observe([&](ui::Point) { ++observed; });
        state.set_offset({0.0f, 150.0f});
        NUI_CHECK(renderer.render(tree));
        renderer.resize({80.0f, 200.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().y, 40.0f, 0.001f);
        NUI_CHECK(observed >= 2); // explicit set + automatic clamp after viewport resize
    }

    // Horizontal scrolling keeps the cross axis constrained to the viewport.
    {
        ui::ScrollState state{ui::ScrollAxis::Horizontal};
        ui::UI tree{
            ui::Scroll{state,
                ui::Canvas{ui::Size{220.0f, 30.0f}, [](ui::CanvasContext2D& g) {
                    g.fill_rect({100.0f, 0.0f, 20.0f, 30.0f}, ui::colors::accent);
                }}}};

        ui::HeadlessRenderer renderer{{70.0f, 50.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.content_size().w, 220.0f, 0.001f);
        NUI_CHECK_NEAR(state.viewport_size().w, 70.0f, 0.001f);
        state.set_offset({100.0f, 30.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 100.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);
        NUI_CHECK(accent(renderer.pixel(10, 10)));
    }

    // Both-axis mode measures nested natural content while the viewport remains fixed.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        ui::UI tree{
            ui::Scroll{state,
                ui::Column{
                    ui::Spacer{180.0f, 40.0f},
                    ui::Spacer{180.0f, 50.0f},
                    ui::Spacer{180.0f, 60.0f}}
                    .gap(5.0f)
                    .padding(0.0f)}};

        ui::HeadlessRenderer renderer{{90.0f, 70.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.viewport_size().w, 90.0f, 0.001f);
        NUI_CHECK_NEAR(state.viewport_size().h, 70.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().w, 180.0f, 0.001f);
        NUI_CHECK_NEAR(state.content_size().h, 160.0f, 0.001f);
        state.set_offset({999.0f, 999.0f});
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK_NEAR(state.offset().x, 90.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 90.0f, 0.001f);
    }

    // T034 ScrollView is pointer-targetable for scrolling without becoming a keyboard focus stop.
    {
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}};
        test::MockPlatform platform;
        tree.resize({100.0f, 100.0f});
        tree.activate(platform);

        ui::InputEvent event{};
        event.type = ui::InputType::PointerWheel;
        event.position = {20.0f, 20.0f};
        event.delta = {0.0f, 40.0f};
        NUI_CHECK(tree.dispatch(event, platform) == ui::EventResult::Handled);
        NUI_CHECK_NEAR(state.offset().y, 40.0f, 0.001f);

        ui::InputEvent tab{};
        tab.type = ui::InputType::KeyDown;
        tab.key = ui::Key::Tab;
        NUI_CHECK(tree.dispatch(tab, platform) == ui::EventResult::Ignored);
    }

    // T034 ensure-visible alignment uses ScrollState as the sole offset authority.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{300.0f, 400.0f}}};
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));

        NUI_CHECK(ui::ensure_visible(state, {20.0f, 20.0f, 20.0f, 20.0f}, ui::ScrollAlignment::Nearest) == false);
        NUI_CHECK(ui::ensure_visible(state, {20.0f, 180.0f, 20.0f, 20.0f}, ui::ScrollAlignment::Nearest));
        NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);

        NUI_CHECK(ui::ensure_visible(state, {150.0f, 250.0f, 20.0f, 20.0f}, ui::ScrollAlignment::Start));
        NUI_CHECK_NEAR(state.offset().x, 150.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 250.0f, 0.001f);

        NUI_CHECK(ui::ensure_visible(state, {180.0f, 300.0f, 20.0f, 20.0f}, ui::ScrollAlignment::Center));
        NUI_CHECK_NEAR(state.offset().x, 140.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 260.0f, 0.001f);

        NUI_CHECK(ui::ensure_visible(state, {250.0f, 350.0f, 20.0f, 20.0f}, ui::ScrollAlignment::End));
        NUI_CHECK_NEAR(state.offset().x, 170.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 270.0f, 0.001f);

        NUI_CHECK(ui::ensure_visible(state, {10.0f, 50.0f, 180.0f, 140.0f}, ui::ScrollAlignment::Nearest));
        NUI_CHECK_NEAR(state.offset().x, 10.0f, 0.001f);
        NUI_CHECK_NEAR(state.offset().y, 50.0f, 0.001f);
    }

    // T034 overlay scrollbar geometry follows the fixed 8px/18px policy.
    {
        ui::ScrollState state{ui::ScrollAxis::Both};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{300.0f, 400.0f}}};
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        state.set_offset({50.0f, 100.0f});

        const auto bars = ui::detail::scroll_view_bars(state, {0.0f, 0.0f, 100.0f, 100.0f});
        NUI_CHECK(bars.horizontal.visible);
        NUI_CHECK(bars.vertical.visible);
        NUI_CHECK_NEAR(bars.horizontal.track.w, 92.0f, 0.001f);
        NUI_CHECK_NEAR(bars.horizontal.track.h, 8.0f, 0.001f);
        NUI_CHECK_NEAR(bars.horizontal.track.y, 92.0f, 0.001f);
        NUI_CHECK_NEAR(bars.vertical.track.x, 92.0f, 0.001f);
        NUI_CHECK_NEAR(bars.vertical.track.h, 92.0f, 0.001f);
        NUI_CHECK_NEAR(bars.vertical.thumb.h, 23.0f, 0.001f);
        NUI_CHECK_NEAR(bars.vertical.thumb.y, 23.0f, 0.001f);
        NUI_CHECK_NEAR(bars.horizontal.thumb.w, 30.6667f, 0.002f);
        NUI_CHECK_NEAR(bars.horizontal.thumb.x, 15.3333f, 0.002f);
    }

    // T034 thumb dragging maps linearly; track clicks consume without panning.
    {
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{ui::ScrollView{state, ui::Spacer{100.0f, 400.0f}}.pointer_pan(true)};
        test::MockPlatform platform;
        tree.resize({100.0f, 100.0f});
        tree.activate(platform);
        state.set_offset({0.0f, 100.0f});

        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 80.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(state.offset().y, 300.0f, 0.001f);
        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerUp, 96.0f, 80.0f), platform) ==
                  ui::EventResult::Handled);

        state.set_offset({0.0f, 100.0f});
        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 96.0f, 90.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);
        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 40.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK_NEAR(state.offset().y, 100.0f, 0.001f);
    }

    // T034 scrollbar overlay input must win over an interactive content descendant.
    {
        auto content = std::make_shared<PointerEatingState>();
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{
            ui::ScrollView{state, PointerEatingContent{content}}.pointer_pan(true)};
        test::MockPlatform platform;
        tree.resize({100.0f, 100.0f});
        tree.activate(platform);
        state.set_offset({0.0f, 100.0f});

        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 96.0f, 30.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(content->down == 0);
        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerMove, 96.0f, 80.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK_NEAR(state.offset().y, 300.0f, 0.001f);
        NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerUp, 96.0f, 80.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(content->move == 0);
        NUI_CHECK(content->up == 0);
    }

    // T034: moving focus to an offscreen descendant reveals it with Nearest before paint.
    {
        ui::ScrollState state{ui::ScrollAxis::Vertical};
        ui::UI tree{
            ui::ScrollView{state,
                ui::Column{
                    ui::Button{"first", [] {}},
                    ui::Spacer{100.0f, 180.0f},
                    ui::Button{"target", [] {}}}
                    .gap(0.0f)
                    .padding(0.0f)}};
        test::MockPlatform platform;
        tree.resize({100.0f, 100.0f});
        tree.activate(platform);
        NUI_CHECK_NEAR(state.offset().y, 0.0f, 0.001f);

        ui::InputEvent tab{};
        tab.type = ui::InputType::KeyDown;
        tab.key = ui::Key::Tab;
        NUI_CHECK(tree.dispatch(tab, platform) == ui::EventResult::Handled);
        NUI_CHECK_NEAR(state.offset().y, 160.0f, 0.001f);

        const float revealed = state.offset().y;
        tree.refresh_focus(platform);
        NUI_CHECK_NEAR(state.offset().y, revealed, 0.001f);
    }
}

} // namespace

int main() { return test::run("scroll_layout", &suite); }
