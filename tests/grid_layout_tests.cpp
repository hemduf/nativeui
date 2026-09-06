#include "test_support.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace {

struct SizedState {
    ui::Size minimum{};
    ui::Size preferred{};
    std::vector<ui::Rect> focus_bounds;
};

class SizedComponent final : public ui::Component {
public:
    explicit SizedComponent(std::shared_ptr<SizedState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return state_->preferred;
    }
    [[nodiscard]] ui::Size minimum_size(const std::vector<ui::ChildMetrics>&) const override {
        return state_->minimum;
    }
    void focus_changed(bool, ui::FocusContext& context) override {
        state_->focus_bounds.push_back(context.bounds());
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<SizedState> state_;
};

class Sized {
public:
    explicit Sized(std::shared_ptr<SizedState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{[state = std::move(state)] {
            return std::make_unique<SizedComponent>(state);
        }, {}};
    }
private:
    std::shared_ptr<SizedState> state_;
};

std::shared_ptr<SizedState> sized(ui::Size minimum, ui::Size preferred) {
    return std::make_shared<SizedState>(SizedState{minimum, preferred, {}});
}

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.001f);
}

std::vector<ui::Rect> focus_all(ui::UI& tree,
                                test::MockPlatform& platform,
                                const std::vector<std::shared_ptr<SizedState>>& states) {
    tree.activate(platform);
    std::vector<ui::Rect> result;
    result.reserve(states.size());
    result.push_back(states.front()->focus_bounds.back());
    for (std::size_t i = 1; i < states.size(); ++i) {
        tree.dispatch(test::key(ui::Key::Tab), platform);
        result.push_back(states[i]->focus_bounds.back());
    }
    return result;
}

void suite() {
    // Fixed + Auto + Flex columns: auto follows content, flex consumes remainder.
    {
        auto a = sized({20.0f, 20.0f}, {40.0f, 30.0f});
        auto b = sized({30.0f, 20.0f}, {60.0f, 40.0f});
        auto c = sized({20.0f, 20.0f}, {100.0f, 25.0f});
        ui::UI tree{
            ui::Grid{
                ui::GridTracks{
                    .columns = {ui::Track::fixed(80.0f), ui::Track::auto_size(), ui::Track::flex(1.0f)},
                    .rows = {ui::Track::auto_size()},
                },
                Sized{a}, Sized{b}, Sized{c}}
                .column_gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({400.0f, 60.0f});
        const auto bounds = focus_all(tree, platform, {a, b, c});
        check_rect(bounds[0], {0.0f, 0.0f, 80.0f, 40.0f});
        check_rect(bounds[1], {90.0f, 0.0f, 60.0f, 40.0f});
        check_rect(bounds[2], {160.0f, 0.0f, 240.0f, 40.0f});
    }

    // Auto tracks shrink toward content minimum before violating a minimum.
    {
        auto a = sized({20.0f, 20.0f}, {40.0f, 30.0f});
        auto b = sized({30.0f, 20.0f}, {60.0f, 30.0f});
        auto c = sized({20.0f, 20.0f}, {100.0f, 30.0f});
        ui::UI tree{
            ui::Grid{
                ui::GridTracks{
                    .columns = {ui::Track::fixed(80.0f), ui::Track::auto_size(), ui::Track::flex(1.0f)},
                    .rows = {ui::Track::auto_size()},
                },
                Sized{a}, Sized{b}, Sized{c}}
                .column_gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({150.0f, 60.0f});
        const auto bounds = focus_all(tree, platform, {a, b, c});
        check_rect(bounds[0], {0.0f, 0.0f, 80.0f, 30.0f});
        check_rect(bounds[1], {90.0f, 0.0f, 30.0f, 30.0f});
        check_rect(bounds[2], {130.0f, 0.0f, 20.0f, 30.0f});
    }

    // Multiple rows use maximum content metrics per track and independent row/column gaps.
    {
        auto a = sized({20.0f, 10.0f}, {40.0f, 20.0f});
        auto b = sized({20.0f, 10.0f}, {50.0f, 30.0f});
        auto c = sized({20.0f, 10.0f}, {60.0f, 40.0f});
        auto d = sized({20.0f, 10.0f}, {70.0f, 15.0f});
        ui::UI tree{
            ui::Grid{
                ui::GridTracks{
                    .columns = {ui::Track::auto_size(), ui::Track::auto_size()},
                    .rows = {ui::Track::auto_size(), ui::Track::auto_size()},
                },
                Sized{a}, Sized{b}, Sized{c}, Sized{d}}
                .column_gap(5.0f)
                .row_gap(7.0f)};
        test::MockPlatform platform;
        tree.resize({200.0f, 100.0f});
        const auto bounds = focus_all(tree, platform, {a, b, c, d});
        check_rect(bounds[0], {0.0f, 0.0f, 60.0f, 30.0f});
        check_rect(bounds[1], {65.0f, 0.0f, 70.0f, 30.0f});
        check_rect(bounds[2], {0.0f, 37.0f, 60.0f, 40.0f});
        check_rect(bounds[3], {65.0f, 37.0f, 70.0f, 40.0f});
    }

    // Resize recomputes the flexible track deterministically.
    {
        auto a = sized({20.0f, 20.0f}, {40.0f, 20.0f});
        auto b = sized({20.0f, 20.0f}, {60.0f, 20.0f});
        ui::UI tree{
            ui::Grid{
                ui::GridTracks{
                    .columns = {ui::Track::fixed(80.0f), ui::Track::flex(1.0f)},
                    .rows = {ui::Track::auto_size()},
                },
                Sized{a}, Sized{b}}
                .gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({300.0f, 50.0f});
        auto bounds = focus_all(tree, platform, {a, b});
        check_rect(bounds[0], {0.0f, 0.0f, 80.0f, 20.0f});
        check_rect(bounds[1], {90.0f, 0.0f, 210.0f, 20.0f});

        tree.resize({180.0f, 50.0f});
        tree.dispatch(test::key(ui::Key::Tab, true), platform);
        const auto resized_a = a->focus_bounds.back();
        tree.dispatch(test::key(ui::Key::Tab), platform);
        const auto resized_b = b->focus_bounds.back();
        check_rect(resized_a, {0.0f, 0.0f, 80.0f, 20.0f});
        check_rect(resized_b, {90.0f, 0.0f, 90.0f, 20.0f});
    }
}

} // namespace

int main() { return test::run("grid_layout", &suite); }
