#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/path.hpp>
#include <nativeui/text.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkTypes.h"
#include "include/core/SkTypeface.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

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
};

[[nodiscard]] ResolvedTextLayout resolve_text_layout(
    std::string_view text,
    const TextStyle& style);

} // namespace detail


class Painter {
public:
    class StateGuard {
    public:
        explicit StateGuard(Painter& painter) noexcept
            : painter_(&painter), previous_floor_(painter.restore_floor_) {
            painter_->canvas_.save();
            ++painter_->save_depth_;
            guard_depth_ = painter_->save_depth_;
            painter_->restore_floor_ = guard_depth_;
        }

        StateGuard(const StateGuard&) = delete;
        StateGuard& operator=(const StateGuard&) = delete;
        StateGuard(StateGuard&& other) noexcept
            : painter_(std::exchange(other.painter_, nullptr)),
              previous_floor_(other.previous_floor_),
              guard_depth_(other.guard_depth_) {}

        ~StateGuard() {
            if (!painter_) return;
            [[maybe_unused]] const bool balanced = painter_->save_depth_ == guard_depth_;
            while (painter_->save_depth_ > guard_depth_) painter_->restore_unchecked();
            if (painter_->save_depth_ == guard_depth_) painter_->restore_unchecked();
            painter_->restore_floor_ = previous_floor_;
            assert(balanced && "Unbalanced Painter save()/restore() inside paint scope");
        }

    private:
        Painter* painter_{};
        int previous_floor_{};
        int guard_depth_{};
    };

    explicit Painter(SkCanvas& canvas) : canvas_(canvas) {}
    ~Painter() { assert(save_depth_ == 0 && "Painter destroyed with unbalanced save stack"); }

    [[nodiscard]] SkCanvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] StateGuard scoped_state() noexcept { return StateGuard{*this}; }
    [[nodiscard]] int save_depth() const noexcept { return save_depth_; }

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
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor4f(to_sk_color(color));
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius, paint);
    }

    void stroke_rounded_rect(Rect rect, float radius, float width, Color color) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(width);
        paint.setColor4f(to_sk_color(color));
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius, paint);
    }

    void circle(Point center, float radius, Color color) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f(to_sk_color(color));
        canvas_.drawCircle(center.x, center.y, radius, paint);
    }

    void arc(Point center, float radius, float start, float end, float width, Color color) {
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

    void line(Point a, Point b, float width, Color color) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(width);
        paint.setStrokeCap(SkPaint::kRound_Cap);
        paint.setColor4f(to_sk_color(color));
        canvas_.drawLine(a.x, a.y, b.x, b.y, paint);
    }

    void fill_path(const Path& path, Color color) {
        if (path.empty()) return;
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        paint.setColor4f(to_sk_color(color));
        canvas_.drawPath(to_sk_path(path), paint);
    }

    void stroke_path(const Path& path, Color color, StrokeStyle style = {}) {
        if (path.empty() || style.width <= 0.0f) return;
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(style.width);
        paint.setStrokeCap(to_sk_cap(style.cap));
        paint.setStrokeJoin(to_sk_join(style.join));
        paint.setStrokeMiter(std::max(0.0f, style.miter_limit));
        paint.setColor4f(to_sk_color(color));
        canvas_.drawPath(to_sk_path(path), paint);
    }

    void text(Point position, std::string_view text, const TextStyle& style) {
        const auto layout = detail::resolve_text_layout(text, style);
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
    [[nodiscard]] static SkRect to_sk_rect(Rect r) {
        return SkRect::MakeXYWH(r.x, r.y, r.w, r.h);
    }

    [[nodiscard]] static SkColor4f to_sk_color(Color c) {
        return SkColor4f{c.r, c.g, c.b, c.a};
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
        SkPath result;
        for (const auto& command : path.commands_) {
            switch (command.verb) {
                case Path::Verb::Move:
                    result.moveTo(command.a.x, command.a.y);
                    break;
                case Path::Verb::Line:
                    result.lineTo(command.a.x, command.a.y);
                    break;
                case Path::Verb::Quad:
                    result.quadTo(command.a.x, command.a.y,
                                  command.b.x, command.b.y);
                    break;
                case Path::Verb::Cubic:
                    result.cubicTo(command.a.x, command.a.y,
                                   command.b.x, command.b.y,
                                   command.c.x, command.c.y);
                    break;
                case Path::Verb::Close:
                    result.close();
                    break;
            }
        }
        return result;
    }

    void restore_unchecked() {
        assert(save_depth_ > 0);
        canvas_.restore();
        --save_depth_;
    }

    SkCanvas& canvas_;
    int save_depth_{};
    int restore_floor_{};
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
