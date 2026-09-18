#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/paint_style.hpp>
#include <nativeui/path.hpp>
#include <nativeui/text.hpp>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRect.h"
#include "include/core/SkRRect.h"
#include "include/core/SkTypes.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

struct PainterLayerFaultAccess;
struct PainterEffectFaultAccess;

struct ResolvedTextRun {
    std::size_t byte_offset{};
    std::size_t byte_count{};
    sk_sp<SkTypeface> typeface;
    float width{};
    bool synthetic_bold{};
};

struct ResolvedTextLayout {
    TextMetrics metrics;
    std::vector<ResolvedTextRun> runs;
    // Only populated for malformed input. Run offsets then refer to these
    // owned bytes, so measurement and painting never pass invalid UTF-8 to Skia.
    std::string repaired_text;

    [[nodiscard]] std::string_view text_bytes(std::string_view original) const noexcept {
        return repaired_text.empty() ? original : std::string_view{repaired_text};
    }
};

[[nodiscard]] ResolvedTextLayout resolve_text_layout(
    std::string_view text,
    const TextStyle& style);

} // namespace detail


class Painter {
    struct ScopeFrame {
        int previous_floor{};
        int entry_depth{};
        int guard_depth{};
    };

    enum class LayerFaultPoint : unsigned char {
        None,
        AfterHardClip,
        AfterSaveLayer,
        BeforeEffectMaterialization,
        AfterEffectOutputClip,
        AfterEffectSaveLayer,
        AfterEffectSourceClip,
    };

    friend struct detail::PainterLayerFaultAccess;
    friend struct detail::PainterEffectFaultAccess;

public:
    class StateGuard {
    public:
        StateGuard(const StateGuard&) = delete;
        StateGuard& operator=(const StateGuard&) = delete;
        StateGuard(StateGuard&&) = delete;
        StateGuard& operator=(StateGuard&&) = delete;

        ~StateGuard() noexcept {
            painter_->end_scope(frame_);
        }

    private:
        friend class Painter;

        struct AdoptFrameTag {};

        explicit StateGuard(Painter& painter) noexcept
            : painter_(&painter), frame_(painter.begin_scope()) {}

        StateGuard(Painter& painter, ScopeFrame frame, AdoptFrameTag) noexcept
            : painter_(&painter), frame_(frame) {}

        Painter* painter_{};
        ScopeFrame frame_{};
    };

    explicit Painter(SkCanvas& canvas) : canvas_(canvas) {}
    ~Painter() noexcept {
        // Framework-owned tree scopes may be unwinding because component paint
        // threw before the matching manual pop. Restore every outstanding save
        // before the Painter leaves the stack so the same SkCanvas is usable by
        // the next frame. Component-local imbalance remains diagnosed by the
        // StateGuard that wraps every component paint callback.
        [[maybe_unused]] const bool balanced = save_depth_ == 0;
        const bool unwinding = std::uncaught_exceptions() > 0;
        while (save_depth_ > 0) restore_unchecked();
        if (!unwinding) {
            assert(balanced && "Painter destroyed with unbalanced save stack");
        }
    }

    [[nodiscard]] SkCanvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] StateGuard scoped_state() noexcept { return StateGuard{*this}; }
    [[nodiscard]] int save_depth() const noexcept { return save_depth_; }

    [[nodiscard]] StateGuard scoped_clip(Rect rect) {
        const SkRect clip = valid_clip_rect(rect) ? to_sk_rect(rect) : SkRect::MakeEmpty();
        const ScopeFrame frame = begin_scope();
        try {
            canvas_.clipRect(clip, SkClipOp::kIntersect, true);
        } catch (...) {
            rollback_scope(frame);
            throw;
        }
        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    [[nodiscard]] StateGuard scoped_clip(Rect rect, float radius) {
        const bool valid_rect = valid_clip_rect(rect);
        const SkRect sk_rect = valid_rect ? to_sk_rect(rect) : SkRect::MakeEmpty();
        const float clip_radius = canonical_clip_radius(rect, radius);
        const bool rounded = valid_rect && clip_radius > 0.0f;
        const SkRRect sk_rounded = rounded
            ? SkRRect::MakeRectXY(sk_rect, clip_radius, clip_radius)
            : SkRRect{};

        const ScopeFrame frame = begin_scope();
        try {
            if (rounded) {
                canvas_.clipRRect(sk_rounded, SkClipOp::kIntersect, true);
            } else {
                canvas_.clipRect(sk_rect, SkClipOp::kIntersect, true);
            }
        } catch (...) {
            rollback_scope(frame);
            throw;
        }
        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    [[nodiscard]] StateGuard scoped_clip(const Path& path) {
        // Path conversion may allocate inside the backend value builder. Finish
        // it before entering the Painter save frame so construction failure
        // cannot publish a partially active scope.
        const SkPath clip = valid_clip_path(path) ? to_sk_path(path) : SkPath{};
        const ScopeFrame frame = begin_scope();
        try {
            canvas_.clipPath(clip, SkClipOp::kIntersect, true);
        } catch (...) {
            rollback_scope(frame);
            throw;
        }
        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    [[nodiscard]] StateGuard scoped_layer(Rect logical_bounds,
                                          PaintOptions options = {}) {
        const bool valid_bounds = valid_clip_rect(logical_bounds);
        const SkRect layer_bounds = valid_bounds
            ? to_sk_rect(logical_bounds)
            : SkRect::MakeEmpty();

        // Invalid geometry is deliberately an empty clip-only scope. In
        // particular, never call saveLayer() with invalid/empty bounds because
        // Skia treats layer bounds as a sizing hint rather than a hard limit.
        if (!valid_bounds) {
            const ScopeFrame frame = begin_scope();
            try {
                canvas_.clipRect(layer_bounds, SkClipOp::kIntersect, false);
            } catch (...) {
                rollback_scope(frame);
                throw;
            }
            return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
        }

        // Prepare all NativeUI-owned composition state before entering either
        // private backend frame. PaintOptions canonicalization is shared with
        // ordinary Painter draws so opacity/blend semantics stay identical.
        SkPaint layer_paint;
        apply_paint_options(layer_paint, options);

        // One public StateGuard owns two private backend frames:
        //   1) a real hard clip captured under the current transform;
        //   2) the saveLayer frame that applies opacity/blend once on restore.
        // The saveLayer bounds remain only a backend sizing hint.
        ScopeFrame frame{restore_floor_, save_depth_, save_depth_};
        try {
            canvas_.save();
            ++save_depth_;
            canvas_.clipRect(layer_bounds, SkClipOp::kIntersect, false);
            maybe_fail_layer(LayerFaultPoint::AfterHardClip);

            [[maybe_unused]] const int previous_save_count =
                canvas_.saveLayer(layer_bounds, &layer_paint);
            ++save_depth_;
            maybe_fail_layer(LayerFaultPoint::AfterSaveLayer);

            frame.guard_depth = save_depth_;
            restore_floor_ = frame.guard_depth;
        } catch (...) {
            rollback_scope(frame);
            throw;
        }

        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    [[nodiscard]] StateGuard scoped_layer(Rect logical_bounds,
                                          const Effect& effect,
                                          PaintOptions options = {}) {
        // Preserve T076's exact invalid/no-op behavior and avoid even creating
        // a backend image filter when there is nothing to evaluate.
        if (!valid_clip_rect(logical_bounds)) {
            return scoped_layer(logical_bounds, options);
        }
        if (effect.kind_ != Effect::Kind::GaussianBlur ||
            (effect.sigma_x_ == 0.0f && effect.sigma_y_ == 0.0f)) {
            return scoped_layer(logical_bounds, options);
        }

        const SkRect source_bounds = to_sk_rect(logical_bounds);
        SkRect output_bounds;
        SkRect device_output_bounds;
        if (!effect_output_bounds(logical_bounds, effect, output_bounds) ||
            !effect_device_output_bounds(output_bounds, device_output_bounds)) {
            // A transform or expansion that cannot be represented safely must
            // fail closed. Reuse T076's balanced empty clip-only scope.
            return scoped_layer(Rect{}, options);
        }

        // All fallible backend preparation happens before any private Painter
        // frame is entered or restore floor is published.
        maybe_fail_layer(LayerFaultPoint::BeforeEffectMaterialization);
        auto image_filter = SkImageFilters::Blur(
            effect.sigma_x_, effect.sigma_y_, SkTileMode::kDecal, nullptr);
        if (!image_filter) {
            throw std::bad_alloc{};
        }

        SkPaint layer_paint;
        apply_paint_options(layer_paint, options);
        layer_paint.setImageFilter(std::move(image_filter));

        // Filtered topology differs deliberately from T076:
        //   outer output-support clip -> saveLayer(filter) -> source clip.
        // The source clip is inside saveLayer so restoring the filtered layer
        // removes it before compositing the halo back under the output clip.
        const auto entry_matrix = canvas_.getLocalToDevice();
        ScopeFrame frame{restore_floor_, save_depth_, save_depth_};
        try {
            canvas_.save();
            ++save_depth_;

            // The support clip is an axis-aligned conservative device-space
            // envelope. Existing parent clips already live in device space and
            // remain intersected while only this new clip is installed under
            // identity coordinates.
            canvas_.resetMatrix();
            canvas_.clipRect(device_output_bounds, SkClipOp::kIntersect, false);
            canvas_.setMatrix(entry_matrix);
            maybe_fail_layer(LayerFaultPoint::AfterEffectOutputClip);

            [[maybe_unused]] const int previous_save_count =
                canvas_.saveLayer(output_bounds, &layer_paint);
            ++save_depth_;
            maybe_fail_layer(LayerFaultPoint::AfterEffectSaveLayer);

            canvas_.clipRect(source_bounds, SkClipOp::kIntersect, false);
            maybe_fail_layer(LayerFaultPoint::AfterEffectSourceClip);

            frame.guard_depth = save_depth_;
            restore_floor_ = frame.guard_depth;
        } catch (...) {
            rollback_scope(frame);
            throw;
        }

        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    void save() {
        canvas_.save();
        ++save_depth_;
    }

    void restore() {
        assert(save_depth_ > restore_floor_ && "Painter restore() crossed a protected paint scope");
        if (save_depth_ <= restore_floor_) return;
        restore_unchecked();
    }

    void translate(float x, float y) { canvas_.translate(x, y); }
    void translate(Point offset) { translate(offset.x, offset.y); }
    void scale(float x, float y) { canvas_.scale(x, y); }
    void scale(float uniform) { scale(uniform, uniform); }
    void rotate(float radians) { canvas_.rotate(radians * (180.0f / kPi)); }
    void concat(const Transform2D& transform) {
        canvas_.concat(SkMatrix::MakeAll(
            transform.m00, transform.m01, transform.m02,
            transform.m10, transform.m11, transform.m12,
            0.0f, 0.0f, 1.0f));
    }

    void fill_rounded_rect(Rect rect, float radius, Color color) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(color, {}));
    }

    void fill_rounded_rect(Rect rect, float radius, const LinearGradient& gradient,
                           PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(gradient, options));
    }

    void fill_rounded_rect(Rect rect, float radius, const RadialGradient& gradient,
                           PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(gradient, options));
    }

    void fill_rounded_rect(Rect rect, float radius, const Brush& brush,
                           PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(brush, options));
    }

    void stroke_rounded_rect(Rect rect, float radius, float width, Color color) {
        stroke_rounded_rect(rect, radius, width, Brush{color});
    }

    void stroke_rounded_rect(Rect rect, float radius, float width, const Brush& brush,
                             PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_stroke_paint(brush, StrokeStyle{width}, options));
    }

    void circle(Point center, float radius, Color color) {
        canvas_.drawCircle(center.x, center.y, radius, make_fill_paint(color, {}));
    }

    void circle(Point center, float radius, const Brush& brush,
                PaintOptions options = {}) {
        canvas_.drawCircle(center.x, center.y, radius, make_fill_paint(brush, options));
    }

    void arc(Point center, float radius, float start, float end, float width, Color color) {
        arc(center, radius, start, end, width, Brush{color});
    }

    void arc(Point center, float radius, float start, float end, float width,
             const Brush& brush, PaintOptions options = {}) {
        const auto paint = make_stroke_paint(
            brush, StrokeStyle{width, StrokeCap::Round}, options);
        const auto oval = SkRect::MakeXYWH(center.x - radius, center.y - radius,
                                           radius * 2.0f, radius * 2.0f);
        constexpr float rad_to_deg = 180.0f / kPi;
        canvas_.drawArc(oval, start * rad_to_deg, (end - start) * rad_to_deg, false, paint);
    }

    void line(Point a, Point b, float width, Color color) {
        line(a, b, width, Brush{color});
    }

    void line(Point a, Point b, float width, const Brush& brush,
              PaintOptions options = {}) {
        const auto paint = make_stroke_paint(
            brush, StrokeStyle{width, StrokeCap::Round}, options);
        canvas_.drawLine(a.x, a.y, b.x, b.y, paint);
    }

    void fill_path(const Path& path, Color color) {
        if (path.empty()) return;
        canvas_.drawPath(to_sk_path(path), make_fill_paint(color, {}));
    }

    void fill_path(const Path& path, const Brush& brush, PaintOptions options = {}) {
        if (path.empty()) return;
        canvas_.drawPath(to_sk_path(path), make_fill_paint(brush, options));
    }

    void stroke_path(const Path& path, Color color, StrokeStyle style = {}) {
        stroke_path(path, Brush{color}, style);
    }

    void stroke_path(const Path& path, const Brush& brush, StrokeStyle style = {},
                     PaintOptions options = {}) {
        if (path.empty() || style.width <= 0.0f) return;
        canvas_.drawPath(to_sk_path(path), make_stroke_paint(brush, style, options));
    }

    void text(Point position, std::string_view text, const TextStyle& style) {
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
            SkFont font(run.typeface, std::max(0.0f, style.size));
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

    void text(Point position, std::string_view text, float size, Color color,
              TextAlign align = TextAlign::Left) {
        TextStyle style{};
        style.size = size;
        style.color = color;
        style.align = align;
        this->text(position, text, style);
    }

    void push_clip(Rect rect) {
        save();
        canvas_.clipRect(to_sk_rect(rect), SkClipOp::kIntersect, true);
    }

    void pop_clip() { restore(); }

    [[nodiscard]] static float measure_text(std::string_view text, float size) {
        return TextService::measure(text, size).width;
    }

private:
    [[nodiscard]] ScopeFrame begin_scope() noexcept {
        ScopeFrame frame{restore_floor_, save_depth_, save_depth_};
        canvas_.save();
        ++save_depth_;
        frame.guard_depth = save_depth_;
        restore_floor_ = frame.guard_depth;
        return frame;
    }

    void end_scope(ScopeFrame frame) noexcept {
        [[maybe_unused]] const bool balanced = save_depth_ == frame.guard_depth;
        while (save_depth_ > frame.entry_depth) restore_unchecked();
        restore_floor_ = frame.previous_floor;
        assert(balanced && "Unbalanced Painter save()/restore() inside paint scope");
    }

    void rollback_scope(ScopeFrame frame) noexcept {
        while (save_depth_ > frame.entry_depth) restore_unchecked();
        restore_floor_ = frame.previous_floor;
    }

    void maybe_fail_layer(LayerFaultPoint point) {
        if (layer_fault_point_ != point) return;
        layer_fault_point_ = LayerFaultPoint::None;
        throw std::bad_alloc{};
    }

    [[nodiscard]] static bool finite_point(Point point) noexcept {
        return std::isfinite(point.x) && std::isfinite(point.y);
    }

    [[nodiscard]] static bool effect_output_bounds(Rect source,
                                                   const Effect& effect,
                                                   SkRect& output) noexcept {
        const double support_x = 3.0 * static_cast<double>(effect.sigma_x_);
        const double support_y = 3.0 * static_cast<double>(effect.sigma_y_);
        const double left = static_cast<double>(source.x) - support_x;
        const double top = static_cast<double>(source.y) - support_y;
        const double right =
            static_cast<double>(source.x) + static_cast<double>(source.w) + support_x;
        const double bottom =
            static_cast<double>(source.y) + static_cast<double>(source.h) + support_y;

        const double max_float = static_cast<double>(std::numeric_limits<float>::max());
        if (!std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(right) || !std::isfinite(bottom) ||
            left < -max_float || top < -max_float ||
            right > max_float || bottom > max_float ||
            !(left < right) || !(top < bottom)) {
            return false;
        }

        output = SkRect::MakeLTRB(static_cast<float>(left),
                                  static_cast<float>(top),
                                  static_cast<float>(right),
                                  static_cast<float>(bottom));
        return output.isFinite() && !output.isEmpty();
    }

    [[nodiscard]] bool effect_device_output_bounds(const SkRect& local_bounds,
                                                   SkRect& device_output) const noexcept {
        const SkMatrix matrix = canvas_.getLocalToDeviceAs3x3();
        if (!matrix.isFinite() || matrix.hasPerspective()) return false;

        const SkRect mapped = matrix.mapRect(local_bounds);
        if (!mapped.isFinite() || mapped.isEmpty()) return false;

        // saveLayer ultimately allocates in integer device coordinates. Build a
        // conservative outward-rounded device AABB before entering any Painter
        // frame, and reject arithmetic that cannot be represented safely.
        const double left = std::floor(static_cast<double>(mapped.left()));
        const double top = std::floor(static_cast<double>(mapped.top()));
        const double right = std::ceil(static_cast<double>(mapped.right()));
        const double bottom = std::ceil(static_cast<double>(mapped.bottom()));
        const double int_min = static_cast<double>(std::numeric_limits<int>::min());
        const double int_max = static_cast<double>(std::numeric_limits<int>::max());
        if (!std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(right) || !std::isfinite(bottom) ||
            left < int_min || top < int_min ||
            right > int_max || bottom > int_max ||
            !(left < right) || !(top < bottom)) {
            return false;
        }

        device_output = SkRect::MakeLTRB(static_cast<float>(left),
                                         static_cast<float>(top),
                                         static_cast<float>(right),
                                         static_cast<float>(bottom));
        return device_output.isFinite() && !device_output.isEmpty();
    }

    [[nodiscard]] static bool valid_clip_rect(Rect rect) noexcept {
        if (!std::isfinite(rect.x) || !std::isfinite(rect.y) ||
            !std::isfinite(rect.w) || !std::isfinite(rect.h) ||
            !(rect.w > 0.0f) || !(rect.h > 0.0f)) {
            return false;
        }
        return std::isfinite(rect.x + rect.w) && std::isfinite(rect.y + rect.h);
    }

    [[nodiscard]] static float canonical_clip_radius(Rect rect, float radius) noexcept {
        if (!valid_clip_rect(rect) || !std::isfinite(radius) || !(radius > 0.0f)) {
            return 0.0f;
        }
        const float max_radius = 0.5f * (std::min)(rect.w, rect.h);
        return (std::min)(radius, max_radius);
    }

    [[nodiscard]] static bool valid_clip_path(const Path& path) noexcept {
        for (const auto& command : path.commands_) {
            switch (command.verb) {
                case Path::Verb::Move:
                case Path::Verb::Line:
                    if (!finite_point(command.a)) return false;
                    break;
                case Path::Verb::Quad:
                    if (!finite_point(command.a) || !finite_point(command.b)) return false;
                    break;
                case Path::Verb::Cubic:
                    if (!finite_point(command.a) || !finite_point(command.b) ||
                        !finite_point(command.c)) return false;
                    break;
                case Path::Verb::Close:
                    break;
            }
        }
        return true;
    }

    [[nodiscard]] static SkRect to_sk_rect(Rect r) {
        return SkRect::MakeXYWH(r.x, r.y, r.w, r.h);
    }

    [[nodiscard]] static SkColor4f to_sk_color(Color c) {
        return SkColor4f{c.r, c.g, c.b, c.a};
    }

    static void apply_fill_source(SkPaint& paint, Color color) {
        paint.setColor4f(to_sk_color(color));
    }

    static void apply_fill_source(SkPaint& paint, const LinearGradient& gradient) {
        const auto start = gradient.start();
        const auto end = gradient.end();
        const SkPoint points[2] = {{start.x, start.y}, {end.x, end.y}};
        apply_gradient(paint, gradient.stops(), [points](const SkGradient& sk_gradient) {
            return SkShaders::LinearGradient(points, sk_gradient);
        });
    }

    static void apply_fill_source(SkPaint& paint, const RadialGradient& gradient) {
        const auto center = gradient.center();
        const SkPoint sk_center{center.x, center.y};
        apply_gradient(paint, gradient.stops(), [sk_center, &gradient](const SkGradient& sk_gradient) {
            if (!(gradient.radius() > 0.0f) || !std::isfinite(gradient.radius())) {
                return sk_sp<SkShader>{};
            }
            return SkShaders::RadialGradient(sk_center, gradient.radius(), sk_gradient);
        });
    }

    [[nodiscard]] static SkPaint make_fill_paint(Color color, PaintOptions options) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        apply_fill_source(paint, color);
        apply_paint_options(paint, options, color.a);
        return paint;
    }

    [[nodiscard]] static SkPaint make_fill_paint(const LinearGradient& gradient,
                                                 PaintOptions options) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        apply_fill_source(paint, gradient);
        // Preserve T021 gradient opacity semantics exactly: PaintOptions opacity
        // is the paint alpha even when an invalid gradient falls back to a
        // source color with its own alpha.
        apply_paint_options(paint, options);
        return paint;
    }

    [[nodiscard]] static SkPaint make_fill_paint(const RadialGradient& gradient,
                                                 PaintOptions options) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        apply_fill_source(paint, gradient);
        apply_paint_options(paint, options);
        return paint;
    }

    [[nodiscard]] static SkPaint make_fill_paint(const Brush& brush, PaintOptions options) {
        return brush.visit([options](const auto& source) {
            return make_fill_paint(source, options);
        });
    }

    [[nodiscard]] static SkPaint make_stroke_paint(const Brush& brush,
                                                   StrokeStyle style,
                                                   PaintOptions options) {
        auto paint = make_fill_paint(brush, options);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(style.width);
        paint.setStrokeCap(to_sk_cap(style.cap));
        paint.setStrokeJoin(to_sk_join(style.join));
        paint.setStrokeMiter(std::max(0.0f, style.miter_limit));
        return paint;
    }

    [[nodiscard]] static bool valid_gradient_stops(const std::vector<GradientStop>& stops) {
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
    static void apply_gradient(SkPaint& paint,
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

    [[nodiscard]] static SkBlendMode to_sk_blend_mode(BlendMode mode) noexcept {
        switch (mode) {
            case BlendMode::SourceOver: return SkBlendMode::kSrcOver;
            case BlendMode::Multiply: return SkBlendMode::kMultiply;
            case BlendMode::Screen: return SkBlendMode::kScreen;
            case BlendMode::Plus: return SkBlendMode::kPlus;
        }
        return SkBlendMode::kSrcOver;
    }

    static void apply_paint_options(SkPaint& paint, PaintOptions options,
                                    float source_alpha = 1.0f) {
        const float opacity = std::isfinite(options.opacity)
            ? std::clamp(options.opacity, 0.0f, 1.0f)
            : 1.0f;
        paint.setAlphaf(source_alpha * opacity);
        paint.setBlendMode(to_sk_blend_mode(options.blend));
    }

    [[nodiscard]] static SkPaint::Cap to_sk_cap(StrokeCap cap) {
        switch (cap) {
            case StrokeCap::Butt: return SkPaint::kButt_Cap;
            case StrokeCap::Round: return SkPaint::kRound_Cap;
            case StrokeCap::Square: return SkPaint::kSquare_Cap;
        }
        return SkPaint::kButt_Cap;
    }

    [[nodiscard]] static SkPaint::Join to_sk_join(StrokeJoin join) {
        switch (join) {
            case StrokeJoin::Miter: return SkPaint::kMiter_Join;
            case StrokeJoin::Round: return SkPaint::kRound_Join;
            case StrokeJoin::Bevel: return SkPaint::kBevel_Join;
        }
        return SkPaint::kMiter_Join;
    }

    [[nodiscard]] static SkPath to_sk_path(const Path& path) {
        SkPathBuilder builder;
        for (const auto& command : path.commands_) {
            switch (command.verb) {
                case Path::Verb::Move:
                    builder.moveTo(command.a.x, command.a.y);
                    break;
                case Path::Verb::Line:
                    builder.lineTo(command.a.x, command.a.y);
                    break;
                case Path::Verb::Quad:
                    builder.quadTo(command.a.x, command.a.y,
                                   command.b.x, command.b.y);
                    break;
                case Path::Verb::Cubic:
                    builder.cubicTo(command.a.x, command.a.y,
                                    command.b.x, command.b.y,
                                    command.c.x, command.c.y);
                    break;
                case Path::Verb::Close:
                    builder.close();
                    break;
            }
        }
        return builder.detach();
    }

    void restore_unchecked() {
        assert(save_depth_ > 0);
        canvas_.restore();
        --save_depth_;
    }

    SkCanvas& canvas_;
    int save_depth_{};
    int restore_floor_{};
    LayerFaultPoint layer_fault_point_{LayerFaultPoint::None};
};

class PlatformServices {
public:
    virtual ~PlatformServices() = default;
    [[nodiscard]] virtual TextMetrics text_metrics(std::string_view text, const TextStyle& style) {
        return TextService::measure(text, style);
    }
    [[nodiscard]] virtual float text_width(std::string_view text, float size) {
        return TextService::measure(text, size).width;
    }
    virtual void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) {
        (void)active; (void)area; (void)cursor_offset;
    }
    // Pointer capture remains owned by the retained tree. These platform-neutral
    // lifecycle hooks let a concrete native view mirror only real none<->owner
    // transitions when its OS requires an explicit native pointer grab.
    virtual void begin_pointer_capture() noexcept {}
    virtual void end_pointer_capture() noexcept {}
    virtual void set_clipboard_text(std::string_view text) = 0;
    virtual void request_clipboard_text() = 0;
    // Drag-and-drop is synchronous at offer time. The default implementation
    // rejects support so headless/custom platform services need no DnD code.
    virtual bool accept_drop(std::string_view type, Rect region) {
        (void)type; (void)region; return false;
    }
    virtual void reject_drop(Rect region) { (void)region; }
};


} // namespace ui