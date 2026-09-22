// Throwaway WebAssembly spike: exercises the pinned wasm32 Skia package and the
// headless NativeUI core under Emscripten. Not a product target.
#include <nativeui/nativeui.hpp>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class ProbeComponent final : public ui::Component {
public:
    explicit ProbeComponent(std::function<void(ui::Painter&)> draw)
        : draw_(std::move(draw)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 48.0f};
    }

    void paint(ui::PaintContext& context) const override { draw_(context.painter()); }

private:
    std::function<void(ui::Painter&)> draw_;
};

class Probe {
public:
    explicit Probe(std::function<void(ui::Painter&)> draw) : draw_(std::move(draw)) {}

    ui::Spec spec() && {
        auto draw = std::move(draw_);
        return ui::Spec{
            [draw = std::move(draw)]() mutable {
                return std::make_unique<ProbeComponent>(std::move(draw));
            },
            {}};
    }

private:
    std::function<void(ui::Painter&)> draw_;
};

std::uint32_t fnv1a(const std::vector<std::uint8_t>& pixels) {
    std::uint32_t hash = 2166136261u;
    for (const std::uint8_t byte : pixels) {
        hash = (hash ^ byte) * 16777619u;
    }
    return hash;
}

bool render_shapes() {
    ui::HeadlessRenderer renderer{{120.0f, 48.0f}, 1.0f};
    ui::UI tree{Probe{[](ui::Painter& painter) {
        painter.fill_rounded_rect({0.0f, 0.0f, 120.0f, 48.0f}, 6.0f, ui::colors::accent);
    }}};
    tree.resize({120.0f, 48.0f});
    if (!renderer.render(tree)) {
        std::printf("wasm spike: shapes render failed\n");
        return false;
    }
    std::printf("wasm spike: shapes %dx%d checksum=%08x\n", renderer.pixel_width(),
                renderer.pixel_height(), fnv1a(renderer.rgba_pixels()));
    return true;
}

bool render_text() {
    ui::HeadlessRenderer renderer{{180.0f, 64.0f}, 1.0f};
    ui::UI tree{ui::Button{"Wasm", [] {}}};
    tree.resize({180.0f, 64.0f});
    if (!renderer.render(tree)) {
        std::printf("wasm spike: text render failed\n");
        return false;
    }
    std::printf("wasm spike: text %dx%d checksum=%08x\n", renderer.pixel_width(),
                renderer.pixel_height(), fnv1a(renderer.rgba_pixels()));
    return true;
}

bool verify_preloaded_font() {
    const bool available = ui::FontManager::has_family("NativeUITestLatin");
    std::printf("wasm spike: preloaded_font=%s\n", available ? "ok" : "fail");
    return available;
}

bool render_embedded_font_text() {
    std::ifstream file{"/nativeui/fonts/NativeUITestLatin.ttf", std::ios::binary};
    if (!file) {
        std::printf("wasm spike: embedded font file missing\n");
        return false;
    }
    const std::string raw{std::istreambuf_iterator<char>{file},
                          std::istreambuf_iterator<char>{}};
    const std::span<const std::byte> bytes{
        reinterpret_cast<const std::byte*>(raw.data()), raw.size()};
    if (bytes.empty() ||
        !ui::FontManager::register_embedded_font("NativeUI Test Latin", bytes)) {
        std::printf("wasm spike: embedded font registration failed\n");
        return false;
    }

    ui::TextStyle style{};
    style.size = 20.0f;
    style.color = {1.0f, 0.0f, 0.0f, 1.0f};
    style.family = "NativeUI Test Latin";

    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};
    ui::UI label{ui::Label{"A"}.style(style)};
    if (!renderer.render(label)) {
        std::printf("wasm spike: embedded font render failed\n");
        return false;
    }

    int red_pixels = 0;
    const auto& pixels = renderer.rgba_pixels();
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        if (pixels[i] > 100 && pixels[i] > pixels[i + 1] && pixels[i] > pixels[i + 2]) {
            ++red_pixels;
        }
    }
    std::printf("wasm spike: embedded font red_pixels=%d checksum=%08x\n", red_pixels,
                fnv1a(pixels));
    return red_pixels > 0;
}

} // namespace

int main() {
    // Keep this first: it proves the conventional MEMFS directory is scanned
    // before the public API snapshots the embedded-face registry.
    const bool preloaded = verify_preloaded_font();
    const bool shapes = render_shapes();
    const bool text = render_text();
    const bool font = render_embedded_font_text();
    std::printf(
        "wasm spike: preloaded=%s shapes=%s text=%s embedded_font=%s\n",
        preloaded ? "ok" : "fail", shapes ? "ok" : "fail",
        text ? "ok" : "fail", font ? "ok" : "fail");
    return (preloaded && shapes && text && font) ? 0 : 1;
}
