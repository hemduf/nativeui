#include "example_support.hpp"

#include <nativeui/cached_layer.hpp>
#include <nativeui/headless.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class ArtworkComponent final : public ui::Component {
public:
    ArtworkComponent(ui::Binding<bool> warm, ui::Binding<int> revision,
                     std::shared_ptr<int> paints)
        : warm_(std::move(warm)), revision_(std::move(revision)),
          paints_(std::move(paints)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {520.0f, 180.0f};
    }

    void paint(ui::PaintContext& context) const override {
        ++*paints_;
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        painter.fill_rounded_rect(bounds, 10.0f, {0.04f, 0.06f, 0.09f, 1.0f});
        constexpr int columns = 32;
        constexpr int rows = 12;
        const float step_x = (bounds.w - 24.0f) / static_cast<float>(columns);
        const float step_y = (bounds.h - 24.0f) / static_cast<float>(rows);
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                const int value = (row * 7 + column * 11 + revision_.get()) % 19;
                const float intensity = 0.2f + static_cast<float>(value) / 24.0f;
                const ui::Color color = warm_.get()
                    ? ui::Color{intensity, 0.30f * intensity, 0.10f, 1.0f}
                    : ui::Color{0.08f, 0.65f * intensity, intensity, 1.0f};
                painter.fill_rounded_rect(
                    {bounds.x + 12.0f + static_cast<float>(column) * step_x,
                     bounds.y + 12.0f + static_cast<float>(row) * step_y,
                     step_x - 2.0f, step_y - 2.0f}, 2.0f, color);
            }
        }
    }

private:
    ui::Binding<bool> warm_;
    ui::Binding<int> revision_;
    std::shared_ptr<int> paints_;
};

class Artwork {
public:
    Artwork(ui::State<bool>& warm, ui::State<int>& revision,
            std::shared_ptr<int> paints)
        : warm_(warm.binding()), revision_(revision.binding()),
          paints_(std::move(paints)) {}

    ui::Spec spec() && {
        return {[warm = warm_, revision = revision_, paints = paints_] {
            return std::make_unique<ArtworkComponent>(warm, revision, paints);
        }, {}};
    }

private:
    ui::Binding<bool> warm_;
    ui::Binding<int> revision_;
    std::shared_ptr<int> paints_;
};

int self_test() {
    ui::State<bool> warm{false};
    ui::State<int> revision{0};
    auto paints = std::make_shared<int>(0);
    ui::UI scene{ui::CachedLayer{Artwork{warm, revision, paints}}.depends(warm, revision, warm)};
    ui::HeadlessRenderer renderer{{520.0f, 180.0f}};
    if (!renderer.render(scene) || *paints != 1) {
        return example::fail("CachedLayer cold frame did not paint its content once");
    }
    const auto original = renderer.rgba_pixels();
    scene.invalidate();
    if (!renderer.render(scene) || *paints != 1 || renderer.rgba_pixels() != original) {
        return example::fail("CachedLayer warm frame did not reuse identical pixels");
    }
    warm.set(false);
    revision.set(0);
    if (scene.dirty()) return example::fail("Equal dependency values invalidated CachedLayer");
    warm.set(true);
    revision.set(1);
    if (!scene.paint_dirty() || scene.layout_dirty() || *paints != 1) {
        return example::fail("Dependency change did not schedule a paint-only update");
    }
    if (!renderer.render(scene) || *paints != 2 || renderer.rgba_pixels() == original) {
        return example::fail("Changed dependencies did not redraw the artwork");
    }
    scene.invalidate();
    if (!renderer.render(scene) || *paints != 2) {
        return example::fail("Changed artwork did not become reusable");
    }

    auto static_paints = std::make_shared<example::BoxObservation>();
    ui::UI static_scene{ui::CachedLayer{example::Box{
        "Zero dependencies", {240.0f, 80.0f},
        {0.10f, 0.20f, 0.30f, 1.0f}, {120.0f, 40.0f}, static_paints}}};
    if (!renderer.render(static_scene)) return example::fail("Static CachedLayer render failed");
    static_scene.invalidate();
    if (!renderer.render(static_scene) || static_paints->paints != 1) {
        return example::fail("Zero-dependency CachedLayer did not reuse its raster");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    ui::State<bool> warm{false};
    ui::State<int> revision{0};
    auto paints = std::make_shared<int>(0);
    ui::UI* scene_pointer{};
    ui::UI scene{ui::Column{
        ui::Header{"CachedLayer"},
        ui::Label{"The artwork redraws when its theme or pattern changes."},
        ui::CachedLayer{Artwork{warm, revision, paints}}.depends(warm, revision),
        ui::Canvas{520.0f, 28.0f, [paints](ui::CanvasContext2D& canvas) {
            canvas.text({0.0f, 18.0f}, "Artwork paint calls: " + std::to_string(*paints),
                        14.0f, ui::colors::text);
        }},
        ui::Row{
            ui::Button{"Change theme", [&] { warm.set(!warm.get()); }},
            ui::Button{"Change pattern", [&] { revision.set((revision.get() + 1) % 97); }},
            ui::Button{"Repaint scene", [&] {
                if (scene_pointer) scene_pointer->invalidate();
            }}
        }.gap(10.0f),
        ui::Label{"Repaint scene reuses the artwork and keeps its paint count unchanged."}
    }.padding(18.0f).gap(12.0f)};
    scene_pointer = &scene;
    return example::run_window(scene, "NativeUI CachedLayer", {620.0f, 430.0f});
}
