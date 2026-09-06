#include "test_support.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace {

struct HitState {
    int pointer_down{};
    std::vector<ui::Rect> focus_bounds;
};

class HitComponent final : public ui::Component {
public:
    explicit HitComponent(std::shared_ptr<HitState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }
    void focus_changed(bool, ui::FocusContext& context) override {
        state_->focus_bounds.push_back(context.bounds());
    }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerDown) {
            ++state_->pointer_down;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<HitState> state_;
};

class Hit {
public:
    explicit Hit(std::shared_ptr<HitState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{[state = std::move(state)] {
            return std::make_unique<HitComponent>(state);
        }, {}};
    }
private:
    std::shared_ptr<HitState> state_;
};

bool red(ui::Rgba8 pixel) {
    return pixel.r > 220 && pixel.g < 40 && pixel.b < 40 && pixel.a > 220;
}

bool green(ui::Rgba8 pixel) {
    return pixel.g > 220 && pixel.r < 40 && pixel.b < 40 && pixel.a > 220;
}

void suite() {
    // A clip viewport prevents an overflowing child from painting outside it.
    {
        ui::UI tree{
            ui::Row{
                ui::Clip{
                    ui::Canvas{
                        ui::Size{40.0f, 40.0f},
                        [](ui::CanvasContext2D& g) {
                            g.fill_rect({-10.0f, 0.0f, 80.0f, 40.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                        }}},
                ui::Spacer{60.0f, 40.0f}}
                .gap(0.0f)};
        ui::HeadlessRenderer renderer{{100.0f, 60.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(20, 10)));
        NUI_CHECK(!red(renderer.pixel(60, 10)));
    }

    // Nested clips compose by intersection: the inner clip wins inside the outer viewport.
    {
        ui::UI tree{
            ui::Row{
                ui::Clip{
                    ui::Padding{10.0f,
                        ui::Clip{
                            ui::Canvas{
                                ui::Size{80.0f, 40.0f},
                                [](ui::CanvasContext2D& g) {
                                    g.fill_rect({-20.0f, -20.0f, 140.0f, 80.0f},
                                                {0.0f, 1.0f, 0.0f, 1.0f});
                                }}}}},
                ui::Spacer{20.0f, 60.0f}}
                .gap(0.0f)};
        ui::HeadlessRenderer renderer{{120.0f, 80.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(green(renderer.pixel(20, 20)));
        NUI_CHECK(!green(renderer.pixel(95, 20)));
        NUI_CHECK(!green(renderer.pixel(5, 20)));
    }

    // Hit testing follows the same clip chain. The child intentionally keeps a
    // 100px intrinsic width while the Flex+Clip viewport shrinks to 50px.
    {
        auto hit = std::make_shared<HitState>();
        ui::UI tree{
            ui::Row{
                ui::Flex{ui::Clip{Hit{hit}}}.shrink(1.0f),
                ui::Spacer{50.0f, 40.0f}}
                .gap(0.0f)};
        test::MockPlatform platform;
        tree.resize({100.0f, 40.0f});
        tree.activate(platform);
        NUI_CHECK(!hit->focus_bounds.empty());
        NUI_CHECK_NEAR(hit->focus_bounds.back().w, 100.0f, 0.001f);

        const auto clipped = tree.dispatch(
            test::pointer(ui::InputType::PointerDown, 75.0f, 20.0f), platform);
        NUI_CHECK(clipped == ui::EventResult::Ignored);
        NUI_CHECK(hit->pointer_down == 0);

        const auto inside = tree.dispatch(
            test::pointer(ui::InputType::PointerDown, 25.0f, 20.0f), platform);
        NUI_CHECK(inside == ui::EventResult::Handled);
        NUI_CHECK(hit->pointer_down == 1);
    }
}

} // namespace

int main() { return test::run("clipping", &suite); }
