#include "test_support.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <memory>
#include <stdexcept>
#include <string>
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

struct ThrowingPaintState {
    bool throw_on_paint{true};
    int paints{};
};

class ThrowingPaintComponent final : public ui::Component {
public:
    explicit ThrowingPaintComponent(std::shared_ptr<ThrowingPaintState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {40.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override {
        ++state_->paints;
        if (state_->throw_on_paint) {
            throw std::runtime_error("T130 throwing paint probe");
        }
    }

private:
    std::shared_ptr<ThrowingPaintState> state_;
};

class ThrowingPaint {
public:
    explicit ThrowingPaint(std::shared_ptr<ThrowingPaintState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ThrowingPaintComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<ThrowingPaintState> state_;
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

    // T130: a descendant paint exception under a framework-owned Clip must not
    // leak SkCanvas save/clip state. The failed frame remains dirty and the same
    // UI + canvas can paint successfully on the next attempt.
    {
        auto state = std::make_shared<ThrowingPaintState>();
        ui::UI tree{ui::Clip{ThrowingPaint{state}}};
        tree.resize({40.0f, 40.0f});
        test::MockPlatform platform;

        const auto info = SkImageInfo::Make(
            64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
        auto surface = SkSurfaces::Raster(info);
        NUI_CHECK(surface != nullptr);
        auto* canvas = surface->getCanvas();
        NUI_CHECK(canvas != nullptr);
        const int baseline_save_count = canvas->getSaveCount();

        std::string propagated;
        try {
            tree.paint(*canvas, platform);
        } catch (const std::runtime_error& error) {
            propagated = error.what();
        }

        NUI_CHECK(propagated == "T130 throwing paint probe");
        NUI_CHECK(state->paints == 1);
        NUI_CHECK(canvas->getSaveCount() == baseline_save_count);
        NUI_CHECK(tree.paint_dirty());

        state->throw_on_paint = false;
        tree.paint(*canvas, platform);
        NUI_CHECK(state->paints == 2);
        NUI_CHECK(canvas->getSaveCount() == baseline_save_count);
        NUI_CHECK(!tree.paint_dirty());
    }
}

} // namespace

int main() { return test::run("clipping", &suite); }
