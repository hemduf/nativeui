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

std::pair<ui::Rect, ui::Rect> focused_bounds(ui::UI& tree,
                                             test::MockPlatform& platform,
                                             const std::shared_ptr<SizedState>& a,
                                             const std::shared_ptr<SizedState>& b) {
    tree.activate(platform);
    const auto first = a->focus_bounds.back();
    tree.dispatch(test::key(ui::Key::Tab), platform);
    const auto second = b->focus_bounds.back();
    return {first, second};
}

void suite() {
    // Grow space is distributed proportionally to weights.
    {
        auto a = sized({20.0f, 20.0f}, {50.0f, 20.0f});
        auto b = sized({20.0f, 20.0f}, {50.0f, 20.0f});
        ui::UI tree{
            ui::Row{
                ui::Flex{Sized{a}}.grow(1.0f),
                ui::Flex{Sized{b}}.grow(2.0f),
            }.gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({300.0f, 60.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {0.0f, 0.0f, 113.333333f, 20.0f});
        check_rect(rb, {123.333333f, 0.0f, 176.666667f, 20.0f});
    }

    // Shrink is weighted and clamps each child at its minimum.
    {
        auto a = sized({80.0f, 20.0f}, {100.0f, 20.0f});
        auto b = sized({20.0f, 20.0f}, {100.0f, 20.0f});
        ui::UI tree{
            ui::Row{
                ui::Flex{Sized{a}}.shrink(1.0f),
                ui::Flex{Sized{b}}.shrink(1.0f),
            }.gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({120.0f, 60.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {0.0f, 0.0f, 80.0f, 20.0f});
        check_rect(rb, {90.0f, 0.0f, 30.0f, 20.0f});
    }

    // Zero weights preserve intrinsic sizing and leave spare room to justification.
    {
        auto a = sized({20.0f, 20.0f}, {50.0f, 20.0f});
        auto b = sized({20.0f, 20.0f}, {30.0f, 20.0f});
        ui::UI tree{
            ui::Row{ui::Flex{Sized{a}}, ui::Flex{Sized{b}}}
                .gap(10.0f)
                .justify(ui::Justify::End)};
        test::MockPlatform platform;
        tree.resize({200.0f, 60.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {110.0f, 0.0f, 50.0f, 20.0f});
        check_rect(rb, {170.0f, 0.0f, 30.0f, 20.0f});
    }

    // If the viewport is smaller than the sum of minimums, minimums win and
    // overflow stays explicit for T011 rather than silently violating min size.
    {
        auto a = sized({70.0f, 20.0f}, {100.0f, 20.0f});
        auto b = sized({60.0f, 20.0f}, {100.0f, 20.0f});
        ui::UI tree{
            ui::Row{
                ui::Flex{Sized{a}}.shrink(1.0f),
                ui::Flex{Sized{b}}.shrink(1.0f),
            }.gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({100.0f, 60.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        NUI_CHECK_NEAR(ra.w, 70.0f, 0.001f);
        NUI_CHECK_NEAR(rb.w, 60.0f, 0.001f);
        NUI_CHECK(rb.x + rb.w > 100.0f);
    }

    // Column uses the same allocation algorithm on the vertical main axis.
    {
        auto a = sized({20.0f, 20.0f}, {40.0f, 40.0f});
        auto b = sized({20.0f, 20.0f}, {40.0f, 40.0f});
        ui::UI tree{
            ui::Column{
                ui::Flex{Sized{a}}.grow(1.0f),
                ui::Flex{Sized{b}}.grow(1.0f),
            }.padding(10.0f).gap(10.0f).align(ui::Align::Stretch)};
        test::MockPlatform platform;
        tree.resize({200.0f, 200.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {10.0f, 10.0f, 180.0f, 85.0f});
        check_rect(rb, {10.0f, 105.0f, 180.0f, 85.0f});
    }

    // Responsive re-layout recomputes flex allocations deterministically.
    {
        auto a = sized({30.0f, 20.0f}, {60.0f, 20.0f});
        auto b = sized({30.0f, 20.0f}, {60.0f, 20.0f});
        ui::UI tree{
            ui::Row{
                ui::Flex{Sized{a}}.grow(1.0f).shrink(1.0f),
                ui::Flex{Sized{b}}.grow(1.0f).shrink(1.0f),
            }.gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({250.0f, 60.0f});
        auto first = focused_bounds(tree, platform, a, b);
        check_rect(first.first, {0.0f, 0.0f, 120.0f, 20.0f});
        check_rect(first.second, {130.0f, 0.0f, 120.0f, 20.0f});

        tree.resize({110.0f, 60.0f});
        tree.dispatch(test::key(ui::Key::Tab, true), platform);
        const auto resized_a = a->focus_bounds.back();
        tree.dispatch(test::key(ui::Key::Tab), platform);
        const auto resized_b = b->focus_bounds.back();
        check_rect(resized_a, {0.0f, 0.0f, 50.0f, 20.0f});
        check_rect(resized_b, {60.0f, 0.0f, 50.0f, 20.0f});
    }
}

} // namespace

int main() { return test::run("layout_flex", &suite); }
