#include "test_support.hpp"

namespace {

struct OverlayProbeState {
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
};

class OverlayProbeComponent final : public ui::Component {
public:
    explicit OverlayProbeComponent(std::shared_ptr<OverlayProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {24.0f, 16.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void activate(ui::LifecycleContext&) override { ++state_->activates; }
    void deactivate(ui::LifecycleContext&) override { ++state_->deactivates; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<OverlayProbeState> state_;
};

class OverlayProbe {
public:
    explicit OverlayProbe(std::shared_ptr<OverlayProbeState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<OverlayProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<OverlayProbeState> state_;
};

ui::OverlaySpec centered_overlay(ui::Spec content) {
    ui::OverlaySpec spec;
    spec.placement = ui::OverlayPlacement::Center;
    spec.content = std::move(content);
    return spec;
}

void overlay_structural_queue_contract() {
    test::MockPlatform platform;
    auto state = std::make_shared<OverlayProbeState>();

    ui::UI tree{
        ui::Canvas{ui::Size{96.0f, 48.0f}, [](ui::CanvasContext2D&) {}}
    };
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    const auto handle = tree.show_overlay(
        centered_overlay(ui::make_spec(OverlayProbe{state})));
    NUI_CHECK(handle.valid());
    NUI_CHECK(state->mounts == 0);

    // show() participates in T058's deferred structural checkpoint. The
    // overlay cannot mount synchronously on the caller's stack.
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(state->mounts == 1);
    NUI_CHECK(state->activates == 1);

    NUI_CHECK(tree.close_overlay(handle));
    NUI_CHECK(!handle.valid());
    NUI_CHECK(state->unmounts == 0);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(state->deactivates == 1);
    NUI_CHECK(state->unmounts == 1);
    NUI_CHECK(!tree.close_overlay(handle));

    // A show immediately closed before the structural checkpoint coalesces to
    // no retained child and therefore emits no lifecycle callbacks.
    auto coalesced = std::make_shared<OverlayProbeState>();
    const auto transient = tree.show_overlay(
        centered_overlay(ui::make_spec(OverlayProbe{coalesced})));
    NUI_CHECK(tree.close_overlay(transient));
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(coalesced->mounts == 0);
    NUI_CHECK(coalesced->unmounts == 0);

    // Handles are UI-owned and the structurally invalid modal/pointer-ignore
    // combination is rejected before any retained work is queued.
    ui::UI other{
        ui::Canvas{ui::Size{96.0f, 48.0f}, [](ui::CanvasContext2D&) {}}
    };
    const auto owned = tree.show_overlay(centered_overlay(ui::make_spec(OverlayProbe{coalesced})));
    NUI_CHECK(!other.close_overlay(owned));
    NUI_CHECK(tree.close_overlay(owned));

    auto invalid = centered_overlay(ui::make_spec(OverlayProbe{coalesced}));
    invalid.mode = ui::OverlayMode::Modal;
    invalid.pointer_policy = ui::OverlayPointerPolicy::Ignore;
    NUI_CHECK(!tree.show_overlay(std::move(invalid)).valid());
}

void suite() {
    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};

    // Seed the raster surface, then render an otherwise empty retained tree.
    // The renderer owns the same black framebuffer clear as the GPU path;
    // Tree itself must add neither a styled background nor instructional text.
    ui::UI seed{
        ui::Canvas{
            ui::Size{96.0f, 48.0f},
            [](ui::CanvasContext2D& canvas) {
                canvas.fill_rect(
                    ui::Rect{0.0f, 0.0f, 96.0f, 48.0f},
                    ui::Color{0.15f, 0.35f, 0.65f, 1.0f});
            }}
    };
    NUI_CHECK(renderer.render(seed));
    const auto seeded_pixels = renderer.rgba_pixels();

    ui::UI empty{
        ui::Canvas{
            ui::Size{96.0f, 48.0f},
            [](ui::CanvasContext2D&) {}}
    };
    NUI_CHECK(renderer.render(empty));
    NUI_CHECK(renderer.rgba_pixels() != seeded_pixels);

    bool only_renderer_clear = true;
    const auto& empty_pixels = renderer.rgba_pixels();
    for (std::size_t i = 0; i < empty_pixels.size(); i += 4) {
        if (empty_pixels[i] != 0 || empty_pixels[i + 1] != 0 ||
            empty_pixels[i + 2] != 0 || empty_pixels[i + 3] != 255) {
            only_renderer_clear = false;
            break;
        }
    }
    NUI_CHECK(only_renderer_clear);

    ui::State<bool> enabled{true};
    ui::UI tree{
        ui::Padding{4.0f,
            ui::Toggle{"Enabled", enabled}}
    };

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 96);
    NUI_CHECK(renderer.pixel_height() == 48);
    NUI_CHECK(renderer.rgba_pixels().size() == 96U * 48U * 4U);

    // The same logical surface at 2x produces exactly twice the physical
    // dimensions while keeping UI layout coordinates logical.
    renderer.resize({96.0f, 48.0f}, 2.0f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 192);
    NUI_CHECK(renderer.pixel_height() == 96);
    NUI_CHECK(renderer.rgba_pixels().size() == 192U * 96U * 4U);

    // A state-only repaint is consumable headlessly without a layout invalidation.
    enabled.set(false);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());

    overlay_structural_queue_contract();
}

} // namespace

int main() { return test::run("headless", &suite); }
