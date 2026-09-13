#include <nativeui/headless.hpp>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkGradient.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {

namespace {

[[nodiscard]] SkRect to_sk_rect(Rect r) {
    return SkRect::MakeXYWH(r.x, r.y, r.w, r.h);
}

[[nodiscard]] SkColor4f to_sk_color(Color c) {
    return SkColor4f{c.r, c.g, c.b, c.a};
}

[[nodiscard]] bool valid_gradient_stops(const std::vector<GradientStop>& stops) {
    if (stops.size() < 2) return false;
    float previous = -1.0f;
    for (const auto& stop : stops) {
        if (!std::isfinite(stop.offset) || stop.offset < 0.0f || stop.offset > 1.0f ||
            stop.offset <= previous) {
            return false;
        }
        previous = stop.offset;
    }
    return true;
}

template <class Factory>
void apply_gradient(SkPaint& paint,
                    const std::vector<GradientStop>& stops,
                    Factory&& factory) {
    if (stops.empty()) {
        paint.setColor4f(to_sk_color(Color{}));
        return;
    }
    if (!valid_gradient_stops(stops)) {
        paint.setColor4f(to_sk_color(stops.front().color));
        return;
    }

    std::vector<SkColor4f> colors;
    std::vector<float> positions;
    colors.reserve(stops.size());
    positions.reserve(stops.size());
    for (const auto& stop : stops) {
        colors.push_back(to_sk_color(stop.color));
        positions.push_back(stop.offset);
    }

    const SkGradient sk_gradient{
        {{colors.data(), colors.size()},
         {positions.data(), positions.size()},
         SkTileMode::kClamp},
        {}};
    auto shader = std::forward<Factory>(factory)(sk_gradient);
    if (shader) {
        paint.setShader(std::move(shader));
    } else {
        paint.setColor4f(colors.front());
    }
}

[[nodiscard]] SkBlendMode to_sk_blend_mode(BlendMode mode) noexcept {
    switch (mode) {
        case BlendMode::SourceOver: return SkBlendMode::kSrcOver;
        case BlendMode::Multiply: return SkBlendMode::kMultiply;
        case BlendMode::Screen: return SkBlendMode::kScreen;
        case BlendMode::Plus: return SkBlendMode::kPlus;
    }
    return SkBlendMode::kSrcOver;
}

void apply_paint_options(SkPaint& paint, PaintOptions options) {
    const float opacity = std::isfinite(options.opacity)
        ? std::clamp(options.opacity, 0.0f, 1.0f)
        : 1.0f;
    paint.setAlphaf(opacity);
    paint.setBlendMode(to_sk_blend_mode(options.blend));
}

[[nodiscard]] SkPaint::Cap to_sk_cap(StrokeCap cap) {
    switch (cap) {
        case StrokeCap::Butt: return SkPaint::kButt_Cap;
        case StrokeCap::Round: return SkPaint::kRound_Cap;
        case StrokeCap::Square: return SkPaint::kSquare_Cap;
    }
    return SkPaint::kButt_Cap;
}

[[nodiscard]] SkPaint::Join to_sk_join(StrokeJoin join) {
    switch (join) {
        case StrokeJoin::Miter: return SkPaint::kMiter_Join;
        case StrokeJoin::Round: return SkPaint::kRound_Join;
        case StrokeJoin::Bevel: return SkPaint::kBevel_Join;
    }
    return SkPaint::kMiter_Join;
}

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

Painter::~Painter() {
    assert(save_depth_ == 0 && "Painter destroyed with unbalanced save stack");
}

void Painter::save() {
    canvas_.save();
    ++save_depth_;
}

void Painter::restore() {
    assert(save_depth_ > restore_floor_ && "Painter restore() crossed a protected paint scope");
    if (save_depth_ <= restore_floor_) return;
    restore_unchecked();
}

void Painter::translate(float x, float y) { canvas_.translate(x, y); }
void Painter::scale(float x, float y) { canvas_.scale(x, y); }
void Painter::rotate(float radians) { canvas_.rotate(radians * (180.0f / kPi)); }
void Painter::concat(const Transform2D& transform) {
    canvas_.concat(SkMatrix::MakeAll(
        transform.m00, transform.m01, transform.m02,
        transform.m10, transform.m11, transform.m12,
        0.0f, 0.0f, 1.0f));
}

void Painter::fill_rounded_rect(Rect rect, float radius, Color color) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor4f(to_sk_color(color));
    canvas_.drawRoundRect(to_sk_rect(rect), radius, radius, paint);
}

void Painter::fill_rounded_rect(Rect rect, float radius, const LinearGradient& gradient,
                                PaintOptions options) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);

    const auto start = gradient.start();
    const auto end = gradient.end();
    const SkPoint points[2] = {{start.x, start.y}, {end.x, end.y}};
    apply_gradient(paint, gradient.stops(), [points](const SkGradient& sk_gradient) {
        return SkShaders::LinearGradient(points, sk_gradient);
    });
    apply_paint_options(paint, options);
    canvas_.drawRoundRect(to_sk_rect(rect), radius, radius, paint);
}

void Painter::fill_rounded_rect(Rect rect, float radius, const RadialGradient& gradient,
                                PaintOptions options) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);

    const auto center = gradient.center();
    const SkPoint sk_center{center.x, center.y};
    apply_gradient(paint, gradient.stops(), [sk_center, &gradient](const SkGradient& sk_gradient) {
        if (!(gradient.radius() > 0.0f) || !std::isfinite(gradient.radius())) {
            return sk_sp<SkShader>{};
        }
        return SkShaders::RadialGradient(sk_center, gradient.radius(), sk_gradient);
    });
    apply_paint_options(paint, options);
    canvas_.drawRoundRect(to_sk_rect(rect), radius, radius, paint);
}

void Painter::stroke_rounded_rect(Rect rect, float radius, float width, Color color) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setColor4f(to_sk_color(color));
    canvas_.drawRoundRect(to_sk_rect(rect), radius, radius, paint);
}

void Painter::circle(Point center, float radius, Color color) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor4f(to_sk_color(color));
    canvas_.drawCircle(center.x, center.y, radius, paint);
}

void Painter::arc(Point center, float radius, float start, float end, float width, Color color) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setColor4f(to_sk_color(color));
    const auto oval = SkRect::MakeXYWH(center.x - radius, center.y - radius,
                                       radius * 2.0f, radius * 2.0f);
    constexpr float rad_to_deg = 180.0f / kPi;
    canvas_.drawArc(oval, start * rad_to_deg, (end - start) * rad_to_deg, false, paint);
}

void Painter::line(Point a, Point b, float width, Color color) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setColor4f(to_sk_color(color));
    canvas_.drawLine(a.x, a.y, b.x, b.y, paint);
}

void Painter::fill_path(const Path& path, Color color) {
    if (path.empty()) return;
    SkPathBuilder builder;
    for (const auto& command : path.commands_) {
        switch (command.verb) {
            case Path::Verb::Move: builder.moveTo(command.a.x, command.a.y); break;
            case Path::Verb::Line: builder.lineTo(command.a.x, command.a.y); break;
            case Path::Verb::Quad:
                builder.quadTo(command.a.x, command.a.y, command.b.x, command.b.y);
                break;
            case Path::Verb::Cubic:
                builder.cubicTo(command.a.x, command.a.y,
                                command.b.x, command.b.y,
                                command.c.x, command.c.y);
                break;
            case Path::Verb::Close: builder.close(); break;
        }
    }
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor4f(to_sk_color(color));
    canvas_.drawPath(builder.detach(), paint);
}

void Painter::stroke_path(const Path& path, Color color, StrokeStyle style) {
    if (path.empty() || style.width <= 0.0f) return;
    SkPathBuilder builder;
    for (const auto& command : path.commands_) {
        switch (command.verb) {
            case Path::Verb::Move: builder.moveTo(command.a.x, command.a.y); break;
            case Path::Verb::Line: builder.lineTo(command.a.x, command.a.y); break;
            case Path::Verb::Quad:
                builder.quadTo(command.a.x, command.a.y, command.b.x, command.b.y);
                break;
            case Path::Verb::Cubic:
                builder.cubicTo(command.a.x, command.a.y,
                                command.b.x, command.b.y,
                                command.c.x, command.c.y);
                break;
            case Path::Verb::Close: builder.close(); break;
        }
    }
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(style.width);
    paint.setStrokeCap(to_sk_cap(style.cap));
    paint.setStrokeJoin(to_sk_join(style.join));
    paint.setStrokeMiter(std::max(0.0f, style.miter_limit));
    paint.setColor4f(to_sk_color(color));
    canvas_.drawPath(builder.detach(), paint);
}

void Painter::text(Point position, std::string_view text, const TextStyle& style) {
    const auto layout = detail::resolve_text_layout(text, style);
    text = layout.text_bytes(text);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor4f(to_sk_color(style.color));

    float x = position.x;
    if (style.align == TextAlign::Center) x -= layout.metrics.width * 0.5f;
    if (style.align == TextAlign::Right) x -= layout.metrics.width;
    const float baseline = position.y -
                           (layout.metrics.ascent + layout.metrics.descent) * 0.5f;

    for (const auto& run : layout.runs) {
        auto typeface = sk_ref_sp(static_cast<SkTypeface*>(run.typeface.get()));
        SkFont font(std::move(typeface), std::max(0.0f, style.size));
        font.setEdging(SkFont::Edging::kAntiAlias);
        font.setEmbolden(run.synthetic_bold);
        canvas_.drawSimpleText(text.data() + run.byte_offset,
                               run.byte_count,
                               SkTextEncoding::kUTF8,
                               x,
                               baseline,
                               font,
                               paint);
        x += run.width;
    }
}

void Painter::text(Point position, std::string_view text, float size, Color color,
                   TextAlign align) {
    TextStyle style{};
    style.size = size;
    style.color = color;
    style.align = align;
    this->text(position, text, style);
}

void Painter::push_clip(Rect rect) {
    save();
    canvas_.clipRect(to_sk_rect(rect), SkClipOp::kIntersect, true);
}

void Painter::pop_clip() { restore(); }

void Painter::restore_unchecked() {
    assert(save_depth_ > 0);
    canvas_.restore();
    --save_depth_;
}

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

        ui.resize(logical_size_);

        auto* canvas = surface_->getCanvas();
        if (!canvas) return false;

        canvas->clear(SK_ColorBLACK);
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
