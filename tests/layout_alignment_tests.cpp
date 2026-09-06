#include "test_support.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace {

struct SizedState {
    ui::Size size{};
    std::vector<ui::Rect> focus_bounds;
};

class SizedComponent final : public ui::Component {
public:
    explicit SizedComponent(std::shared_ptr<SizedState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return state_->size;
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

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.0001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.0001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
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
    // Defaults preserve current top/left packing.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{30.0f, 40.0f}, {}});
        ui::UI tree{ui::Row{Sized{a}, Sized{b}}.gap(10.0f)};
        test::MockPlatform platform;
        tree.resize({300.0f, 100.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {0.0f, 0.0f, 50.0f, 20.0f});
        check_rect(rb, {60.0f, 0.0f, 30.0f, 40.0f});
    }

    // Row main-axis center + cross-axis center with heterogeneous sizes.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{30.0f, 40.0f}, {}});
        ui::UI tree{
            ui::Row{Sized{a}, Sized{b}}
                .gap(10.0f)
                .justify(ui::Justify::Center)
                .align(ui::Align::Center)};
        test::MockPlatform platform;
        tree.resize({300.0f, 100.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {105.0f, 40.0f, 50.0f, 20.0f});
        check_rect(rb, {165.0f, 30.0f, 30.0f, 40.0f});
    }

    // Space-between consumes free main-axis space while preserving base gap.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{30.0f, 40.0f}, {}});
        ui::UI tree{ui::Row{Sized{a}, Sized{b}}.gap(10.0f).justify(ui::Justify::SpaceBetween)};
        test::MockPlatform platform;
        tree.resize({300.0f, 100.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {0.0f, 0.0f, 50.0f, 20.0f});
        check_rect(rb, {270.0f, 0.0f, 30.0f, 40.0f});
    }

    // Stretch fills only the cross axis and remains bounded by the container.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{30.0f, 40.0f}, {}});
        ui::UI tree{ui::Row{Sized{a}, Sized{b}}.gap(10.0f).align(ui::Align::Stretch)};
        test::MockPlatform platform;
        tree.resize({300.0f, 75.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {0.0f, 0.0f, 50.0f, 75.0f});
        check_rect(rb, {60.0f, 0.0f, 30.0f, 75.0f});
    }

    // Column end/end accounts for padding and mixed child widths.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{80.0f, 30.0f}, {}});
        ui::UI tree{
            ui::Column{Sized{a}, Sized{b}}
                .padding(10.0f)
                .gap(10.0f)
                .justify(ui::Justify::End)
                .align(ui::Align::End)};
        test::MockPlatform platform;
        tree.resize({200.0f, 200.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {140.0f, 130.0f, 50.0f, 20.0f});
        check_rect(rb, {110.0f, 160.0f, 80.0f, 30.0f});
    }

    // Row end positioning and cross-axis end complete the non-stretch alignment set.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{30.0f, 40.0f}, {}});
        ui::UI tree{
            ui::Row{Sized{a}, Sized{b}}
                .gap(10.0f)
                .justify(ui::Justify::End)
                .align(ui::Align::End)};
        test::MockPlatform platform;
        tree.resize({300.0f, 100.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {210.0f, 80.0f, 50.0f, 20.0f});
        check_rect(rb, {270.0f, 60.0f, 30.0f, 40.0f});
    }

    // Column center + space-between distributes vertical free space and centers widths.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{80.0f, 30.0f}, {}});
        ui::UI tree{
            ui::Column{Sized{a}, Sized{b}}
                .padding(10.0f)
                .gap(10.0f)
                .justify(ui::Justify::SpaceBetween)
                .align(ui::Align::Center)};
        test::MockPlatform platform;
        tree.resize({200.0f, 200.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {75.0f, 10.0f, 50.0f, 20.0f});
        check_rect(rb, {60.0f, 160.0f, 80.0f, 30.0f});
    }

    // Column stretch fills the cross axis but never exceeds the inner padded box.
    {
        auto a = std::make_shared<SizedState>(SizedState{{50.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{80.0f, 30.0f}, {}});
        ui::UI tree{
            ui::Column{Sized{a}, Sized{b}}
                .padding(10.0f)
                .gap(10.0f)
                .align(ui::Align::Stretch)};
        test::MockPlatform platform;
        tree.resize({200.0f, 100.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {10.0f, 10.0f, 180.0f, 20.0f});
        check_rect(rb, {10.0f, 40.0f, 180.0f, 30.0f});
    }

    // Nested containers retain alignment behavior after outer layout.
    {
        auto a = std::make_shared<SizedState>(SizedState{{40.0f, 20.0f}, {}});
        auto b = std::make_shared<SizedState>(SizedState{{40.0f, 20.0f}, {}});
        ui::UI tree{
            ui::Padding{10.0f,
                ui::Row{Sized{a}, Sized{b}}.gap(10.0f).justify(ui::Justify::End)}};
        test::MockPlatform platform;
        tree.resize({200.0f, 80.0f});
        const auto [ra, rb] = focused_bounds(tree, platform, a, b);
        check_rect(ra, {100.0f, 10.0f, 40.0f, 20.0f});
        check_rect(rb, {150.0f, 10.0f, 40.0f, 20.0f});
    }
}

} // namespace

int main() { return test::run("layout_alignment", &suite); }
