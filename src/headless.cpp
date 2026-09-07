#include <nativeui/headless.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ui {

namespace {

class HeadlessPlatformServices final : public PlatformServices {
public:
    void set_clipboard_text(std::string_view text) override {
        clipboard_.assign(text);
    }

    void request_clipboard_text() override {}

private:
    std::string clipboard_;
};

void validate_size(Size logical_size, float scale_factor) {
    if (!(logical_size.w > 0.0f) || !(logical_size.h > 0.0f) ||
        !std::isfinite(logical_size.w) || !std::isfinite(logical_size.h) ||
        !(scale_factor > 0.0f) || !std::isfinite(scale_factor)) {
        throw std::invalid_argument("HeadlessRenderer requires finite positive size and scale");
    }
}

} // namespace

class HeadlessRenderer::Impl {
public:
    Impl(Size logical_size, float scale_factor) {
        resize(logical_size, scale_factor);
    }

    void resize(Size logical_size, float scale_factor) {
        validate_size(logical_size, scale_factor);
        logical_size_ = logical_size;
        scale_factor_ = scale_factor;
        pixel_width_ = std::max(1, static_cast<int>(std::lround(logical_size.w * scale_factor)));
        pixel_height_ = std::max(1, static_cast<int>(std::lround(logical_size.h * scale_factor)));
        surface_.reset();
        pixels_.clear();
    }

    bool render(UI& ui) {
        if (!surface_) {
            const auto info = SkImageInfo::Make(
                pixel_width_, pixel_height_, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
            surface_ = SkSurfaces::Raster(info);
            if (!surface_) return false;
        }

        // resize() is idempotent for an unchanged logical viewport, so this
        // safely supports rendering different UI instances without pointer
        // identity/lifetime bookkeeping and without forcing layout each frame.
        ui.resize(logical_size_);

        auto* canvas = surface_->getCanvas();
        if (!canvas) return false;

        canvas->save();
        canvas->scale(scale_factor_, scale_factor_);
        ui.paint(*canvas, services_);
        canvas->restore();

        SkPixmap pixmap;
        if (!surface_->peekPixels(&pixmap) || !pixmap.addr()) return false;

        constexpr std::size_t bytes_per_pixel = 4;
        const auto packed_row_bytes = static_cast<std::size_t>(pixel_width_) * bytes_per_pixel;
        pixels_.resize(packed_row_bytes * static_cast<std::size_t>(pixel_height_));
        const auto* source = static_cast<const std::uint8_t*>(pixmap.addr());
        for (int y = 0; y < pixel_height_; ++y) {
            std::memcpy(pixels_.data() + static_cast<std::size_t>(y) * packed_row_bytes,
                        source + static_cast<std::size_t>(y) * pixmap.rowBytes(),
                        packed_row_bytes);
        }
        return true;
    }

    [[nodiscard]] Rgba8 pixel(int x, int y) const {
        if (x < 0 || y < 0 || x >= pixel_width_ || y >= pixel_height_) {
            throw std::out_of_range("HeadlessRenderer::pixel outside surface");
        }
        constexpr std::size_t channels = 4;
        const auto offset =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(pixel_width_) +
             static_cast<std::size_t>(x)) * channels;
        return Rgba8{pixels_.at(offset + 0),
                     pixels_.at(offset + 1),
                     pixels_.at(offset + 2),
                     pixels_.at(offset + 3)};
    }

    Size logical_size_{};
    float scale_factor_{1.0f};
    int pixel_width_{};
    int pixel_height_{};
    sk_sp<SkSurface> surface_;
    HeadlessPlatformServices services_;
    std::vector<std::uint8_t> pixels_;
};

HeadlessRenderer::HeadlessRenderer(Size logical_size, float scale_factor)
    : impl_(std::make_unique<Impl>(logical_size, scale_factor)) {}

HeadlessRenderer::~HeadlessRenderer() = default;
HeadlessRenderer::HeadlessRenderer(HeadlessRenderer&&) noexcept = default;
HeadlessRenderer& HeadlessRenderer::operator=(HeadlessRenderer&&) noexcept = default;

void HeadlessRenderer::resize(Size logical_size, float scale_factor) {
    impl_->resize(logical_size, scale_factor);
}

bool HeadlessRenderer::render(UI& ui) { return impl_->render(ui); }
Size HeadlessRenderer::logical_size() const noexcept { return impl_->logical_size_; }
float HeadlessRenderer::scale_factor() const noexcept { return impl_->scale_factor_; }
int HeadlessRenderer::pixel_width() const noexcept { return impl_->pixel_width_; }
int HeadlessRenderer::pixel_height() const noexcept { return impl_->pixel_height_; }
const std::vector<std::uint8_t>& HeadlessRenderer::rgba_pixels() const noexcept {
    return impl_->pixels_;
}
Rgba8 HeadlessRenderer::pixel(int x, int y) const { return impl_->pixel(x, y); }

} // namespace ui
