/// \file
/// Retained drawing surface used by NativeUI component paint callbacks.
///
/// Painter borrows a backend SkCanvas for one paint traversal while tracking
/// NativeUI save/restore and logical-transform state. Public drawing coordinates
/// and lengths are logical UI units unless stated otherwise. This is UI/render-
/// thread work and is not an audio-real-time API.
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
#include "include/core/SkShader.h"
#include "include/core/SkTypes.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {

namespace detail {

struct PainterLayerFaultAccess;
struct PainterEffectFaultAccess;
struct PainterTransformHistoryFaultAccess;
struct ShaderBrushSnapshot;

[[nodiscard]] sk_sp<SkShader> materialize_shader_brush(
    const std::shared_ptr<const ShaderBrushSnapshot>& snapshot);
[[nodiscard]] sk_sp<SkShader> materialize_image_texture(
    const ImageTexture& texture);

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


/// Backend-backed drawing context for one retained paint traversal.
///
/// Painter does not own the SkCanvas supplied at construction and must not
/// outlive it. Prefer typed Painter methods over mutating the raw canvas:
/// backend save/restore/transform calls can bypass NativeUI's transform history
/// and protected-scope invariants.
///
/// Path/gradient/effect/text materialization and unusually deep save stacks may
/// allocate. Exceptions from fallible scope setup roll back protected backend
/// state before propagating.
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
    friend struct detail::PainterTransformHistoryFaultAccess;

public:
    /// Non-copyable lexical guard that restores one protected Painter scope.
    ///
    /// The guard borrows its Painter and must die first. Manual save()/restore()
    /// calls inside the scope must balance before guard destruction; debug builds
    /// diagnose imbalance while cleanup still restores outstanding frames.
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

        explicit StateGuard(Painter& painter)
            : painter_(&painter), frame_(painter.begin_scope()) {}

        StateGuard(Painter& painter, ScopeFrame frame, AdoptFrameTag) noexcept
            : painter_(&painter), frame_(frame) {}

        Painter* painter_{};
        ScopeFrame frame_{};
    };

    /// Borrow `canvas`; no ownership is transferred and Painter must die first.
    explicit Painter(SkCanvas& canvas) : canvas_(canvas) {}
    /// Restore outstanding backend saves before releasing the borrowed canvas.
    ///
    /// Destruction is noexcept so component-paint unwinding cannot poison the
    /// backend save stack used by a later frame.
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

    /// Return the borrowed backend canvas.
    ///
    /// This low-level escape hatch is valid only for the Painter/canvas lifetime.
    /// Avoid backend save/restore/transform mutations through it because Painter
    /// tracks those operations separately.
    [[nodiscard]] SkCanvas& canvas() noexcept { return canvas_; }
    /// Save drawing state and restore it automatically at lexical scope exit.
    [[nodiscard]] StateGuard scoped_state() { return StateGuard{*this}; }
    /// Current Painter-managed backend save depth, primarily for diagnostics.
    [[nodiscard]] int save_depth() const noexcept { return save_depth_; }
    /// Report whether a non-trivial image effect was materialized.
    ///
    /// This renderer-observability bit supports conservative partial/full scene
    /// decisions; ordinary opacity, blend and gradients do not set it.
    // Renderer-internal effect observability for scene update validation.
    [[nodiscard]] bool used_effects() const noexcept { return used_effects_; }
    /// Current logical affine transform accumulated through Painter.
    ///
    /// It is restored with Painter's save stack. Direct raw-canvas transforms are
    /// outside this tracking contract.
    [[nodiscard]] Transform2D current_transform() const noexcept {
        return current_transform_;
    }

    /// Intersect drawing with a logical rectangle until guard destruction.
    ///
    /// Non-finite, empty or non-positive geometry becomes an empty clip. Backend
    /// failure rolls back the newly entered save frame before propagating.
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

    /// Intersect drawing with a rounded logical rectangle.
    ///
    /// Radius is logical: non-finite/non-positive becomes zero and positive
    /// values clamp to half the smaller rectangle dimension. Invalid rectangle
    /// geometry produces an empty rectangular clip.
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

    /// Intersect drawing with `path` until scope exit.
    ///
    /// Path conversion may allocate and finishes before publishing the protected
    /// save frame. Any non-finite path coordinate yields an empty clip.
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

    /// Create a bounded offscreen composition layer.
    ///
    /// Bounds are logical. PaintOptions opacity/blend apply once when restoring
    /// the layer. Invalid bounds create an empty clip-only scope instead of an
    /// unbounded backend layer. Fallible setup is transactional.
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
            push_backend_frame([&] { canvas_.save(); });
            canvas_.clipRect(layer_bounds, SkClipOp::kIntersect, false);
            maybe_fail_layer(LayerFaultPoint::AfterHardClip);

            push_backend_frame([&] {
                [[maybe_unused]] const int previous_save_count =
                    canvas_.saveLayer(layer_bounds, &layer_paint);
            });
            maybe_fail_layer(LayerFaultPoint::AfterSaveLayer);

            frame.guard_depth = save_depth_;
            restore_floor_ = frame.guard_depth;
        } catch (...) {
            rollback_scope(frame);
            throw;
        }

        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    /// Create a bounded filtered layer and apply `effect` on composition.
    ///
    /// Effects use logical/local units. Zero blur reduces to a normal layer.
    /// Transparent DropShadow keeps the source; transparent DropShadowOnly yields
    /// empty output. Non-finite/unrepresentable support fails closed. Filter
    /// materialization may allocate/throw and successful effects set used_effects().
    [[nodiscard]] StateGuard scoped_layer(Rect logical_bounds,
                                          const Effect& effect,
                                          PaintOptions options = {}) {
        // Preserve T076's exact invalid/no-op behavior and avoid even creating
        // a backend image filter when there is nothing to evaluate.
        if (!valid_clip_rect(logical_bounds)) {
            return scoped_layer(logical_bounds, options);
        }
        if (effect.kind_ == Effect::Kind::GaussianBlur &&
            effect.sigma_x_ == 0.0f && effect.sigma_y_ == 0.0f) {
            return scoped_layer(logical_bounds, options);
        }
        if ((effect.kind_ == Effect::Kind::DropShadow ||
             effect.kind_ == Effect::Kind::DropShadowOnly) &&
            effect.color_.a <= 0.0f) {
            return effect.kind_ == Effect::Kind::DropShadow
                ? scoped_layer(logical_bounds, options)
                : scoped_empty_device_output();
        }

        const SkRect source_bounds = to_sk_rect(logical_bounds);
        SkIRect device_source_bounds;
        if (!effect_source_device_bounds(source_bounds, device_source_bounds)) {
            // A transform that cannot produce a finite conservative source
            // rectangle must fail closed before backend filter materialization.
            return scoped_empty_device_output();
        }

        // All fallible backend preparation happens before any private Painter
        // frame is entered or restore floor is published.
        maybe_fail_layer(LayerFaultPoint::BeforeEffectMaterialization);
        auto image_filter = materialize_effect_filter(effect);
        if (!image_filter) {
            throw std::bad_alloc{};
        }
        used_effects_ = true;

        SkIRect device_output_bounds;
        if (!effect_filter_output_bounds(
                *image_filter, device_source_bounds, device_output_bounds)) {
            return scoped_empty_device_output();
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
            push_backend_frame([&] { canvas_.save(); });

            // The support clip is an axis-aligned conservative device-space
            // envelope. Existing parent clips already live in device space and
            // remain intersected while only this new clip is installed under
            // identity coordinates.
            canvas_.resetMatrix();
            canvas_.clipIRect(device_output_bounds, SkClipOp::kIntersect);
            canvas_.setMatrix(entry_matrix);
            maybe_fail_layer(LayerFaultPoint::AfterEffectOutputClip);

            // saveLayer bounds are only a sizing hint in pinned Skia.
            // The real finite device clip above already bounds output/allocation,
            // so omit the optional hint and keep correctness independent of it.
            push_backend_frame([&] {
                [[maybe_unused]] const int previous_save_count =
                    canvas_.saveLayer(nullptr, &layer_paint);
            });
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

    /// Push backend state plus Painter's logical-transform snapshot.
    ///
    /// Ordinary nesting uses inline storage; unusually deep nesting may allocate
    /// and throw before the backend save is published. Pair with restore().
    void save() {
        push_backend_frame([&] { canvas_.save(); });
    }

    /// Restore one matching Painter save().
    ///
    /// Crossing a protected scoped_state/scoped_clip/scoped_layer restore floor
    /// is a programming error and is ignored at that floor.
    void restore() {
        assert(save_depth_ > restore_floor_ && "Painter restore() crossed a protected paint scope");
        if (save_depth_ <= restore_floor_) return;
        restore_unchecked();
    }

    /// Post-concatenate a logical translation; non-finite inputs are ignored.
    void translate(float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        const Transform2D operation = Transform2D::translation(x, y);
        apply_logical_transform(operation, [&] { canvas_.translate(x, y); });
    }
    /// Convenience overload for a logical translation vector.
    void translate(Point offset) { translate(offset.x, offset.y); }
    /// Post-concatenate dimensionless X/Y scale factors; non-finite input is ignored.
    void scale(float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        const Transform2D operation = Transform2D::scaling(x, y);
        apply_logical_transform(operation, [&] { canvas_.scale(x, y); });
    }
    /// Apply one dimensionless scale factor to both axes.
    void scale(float uniform) { scale(uniform, uniform); }
    /// Post-concatenate rotation in radians; non-finite input is ignored.
    void rotate(float radians) {
        if (!std::isfinite(radians)) return;
        const Transform2D operation = Transform2D::rotation(radians);
        apply_logical_transform(operation, [&] {
            canvas_.concat(SkMatrix::MakeAll(
                operation.m00, operation.m01, operation.m02,
                operation.m10, operation.m11, operation.m12,
                0.0f, 0.0f, 1.0f));
        });
    }
    /// Post-concatenate an affine logical transform.
    ///
    /// Non-finite input/composition is ignored. Backend mutation completes before
    /// Painter publishes the new logical transform.
    void concat(const Transform2D& transform) {
        apply_logical_transform(transform, [&] {
            canvas_.concat(SkMatrix::MakeAll(
                transform.m00, transform.m01, transform.m02,
                transform.m10, transform.m11, transform.m12,
                0.0f, 0.0f, 1.0f));
        });
    }

    /// Fill a rounded logical rectangle with a solid color.
    void fill_rounded_rect(Rect rect, float radius, Color color) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(color, {}));
    }

    /// Fill a rounded logical rectangle with a linear gradient.
    void fill_rounded_rect(Rect rect, float radius, const LinearGradient& gradient,
                           PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(gradient, options));
    }

    /// Fill a rounded logical rectangle with a radial gradient.
    void fill_rounded_rect(Rect rect, float radius, const RadialGradient& gradient,
                           PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(gradient, options));
    }

    /// Fill a rounded logical rectangle from any Brush source.
    void fill_rounded_rect(Rect rect, float radius, const Brush& brush,
                           PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_fill_paint(brush, options));
    }

    /// Stroke a rounded rectangle; radius/width are logical lengths.
    void stroke_rounded_rect(Rect rect, float radius, float width, Color color) {
        stroke_rounded_rect(rect, radius, width, Brush{color});
    }

    /// Brush overload for stroking a rounded logical rectangle.
    void stroke_rounded_rect(Rect rect, float radius, float width, const Brush& brush,
                             PaintOptions options = {}) {
        canvas_.drawRoundRect(to_sk_rect(rect), radius, radius,
                              make_stroke_paint(brush, StrokeStyle{width}, options));
    }

    /// Fill a logical-coordinate circle with a solid color.
    void circle(Point center, float radius, Color color) {
        canvas_.drawCircle(center.x, center.y, radius, make_fill_paint(color, {}));
    }

    /// Fill a logical-coordinate circle from any Brush source.
    void circle(Point center, float radius, const Brush& brush,
                PaintOptions options = {}) {
        canvas_.drawCircle(center.x, center.y, radius, make_fill_paint(brush, options));
    }

    /// Stroke an arc; angles are radians and radius/width are logical lengths.
    /// Sweep is end-start and caps are round.
    void arc(Point center, float radius, float start, float end, float width, Color color) {
        arc(center, radius, start, end, width, Brush{color});
    }

    /// Brush overload for an arc; angles are radians and caps are round.
    void arc(Point center, float radius, float start, float end, float width,
             const Brush& brush, PaintOptions options = {}) {
        const auto paint = make_stroke_paint(
            brush, StrokeStyle{width, StrokeCap::Round}, options);
        const auto oval = SkRect::MakeXYWH(center.x - radius, center.y - radius,
                                           radius * 2.0f, radius * 2.0f);
        constexpr float rad_to_deg = 180.0f / kPi;
        canvas_.drawArc(oval, start * rad_to_deg, (end - start) * rad_to_deg, false, paint);
    }

    /// Stroke a logical-coordinate line segment with round caps.
    void line(Point a, Point b, float width, Color color) {
        line(a, b, width, Brush{color});
    }

    /// Brush overload for a logical-coordinate line segment with round caps.
    void line(Point a, Point b, float width, const Brush& brush,
              PaintOptions options = {}) {
        const auto paint = make_stroke_paint(
            brush, StrokeStyle{width, StrokeCap::Round}, options);
        canvas_.drawLine(a.x, a.y, b.x, b.y, paint);
    }

    /// Fill a Path with solid color; an empty path is a no-op.
    void fill_path(const Path& path, Color color) {
        if (path.empty()) return;
        canvas_.drawPath(to_sk_path(path), make_fill_paint(color, {}));
    }

    /// Fill a Path from a Brush; an empty path is a no-op.
    void fill_path(const Path& path, const Brush& brush, PaintOptions options = {}) {
        if (path.empty()) return;
        canvas_.drawPath(to_sk_path(path), make_fill_paint(brush, options));
    }

    /// Stroke a Path with solid color and StrokeStyle.
    void stroke_path(const Path& path, Color color, StrokeStyle style = {}) {
        stroke_path(path, Brush{color}, style);
    }

    /// Brush overload; empty paths and non-positive widths are no-ops.
    void stroke_path(const Path& path, const Brush& brush, StrokeStyle style = {},
                     PaintOptions options = {}) {
        if (path.empty() || style.width <= 0.0f) return;
        canvas_.drawPath(to_sk_path(path), make_stroke_paint(brush, style, options));
    }

    /// Draw UTF-8 text centered vertically on `position.y`.
    ///
    /// position.x follows TextStyle::align. Font size/coordinates are logical.
    /// Layout may allocate; malformed UTF-8 is repaired before reaching Skia and
    /// negative drawing size is clamped to zero.
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

    /// Convenience overload constructing TextStyle from size/color/alignment.
    void text(Point position, std::string_view text, float size, Color color,
              TextAlign align = TextAlign::Left) {
        TextStyle style{};
        style.size = size;
        style.color = color;
        style.align = align;
        this->text(position, text, style);
    }

    /// Manually save then intersect a rectangular clip.
    ///
    /// This forwards rect directly and must pair with pop_clip(). Prefer
    /// scoped_clip() for exception-safe balance and invalid-geometry handling.
    void push_clip(Rect rect) {
        save();
        canvas_.clipRect(to_sk_rect(rect), SkClipOp::kIntersect, true);
    }

    /// Restore the state pushed by push_clip().
    void pop_clip() { restore(); }

    /// Measure UTF-8 text width in logical units using TextService defaults.
    [[nodiscard]] static float measure_text(std::string_view text, float size) {
        return TextService::measure(text, size).width;
    }

private:
    static constexpr int kInlineTransformSaveDepth = 32;

    [[nodiscard]] StateGuard scoped_empty_device_output() {
        const auto entry_matrix = canvas_.getLocalToDevice();
        const ScopeFrame frame = begin_scope();
        try {
            canvas_.resetMatrix();
            canvas_.clipIRect(SkIRect::MakeEmpty(), SkClipOp::kIntersect);
            canvas_.setMatrix(entry_matrix);
        } catch (...) {
            rollback_scope(frame);
            throw;
        }
        return StateGuard{*this, frame, StateGuard::AdoptFrameTag{}};
    }

    [[nodiscard]] ScopeFrame begin_scope() {
        ScopeFrame frame{restore_floor_, save_depth_, save_depth_};
        push_backend_frame([&] { canvas_.save(); });
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

    template <class SaveOperation>
    void push_backend_frame(SaveOperation&& save_operation) {
        prepare_transform_snapshot();
        try {
            std::forward<SaveOperation>(save_operation)();
        } catch (...) {
            discard_prepared_transform_snapshot();
            throw;
        }
        ++save_depth_;
    }

    void prepare_transform_snapshot() {
        if (save_depth_ < kInlineTransformSaveDepth) {
            transform_history_inline_[static_cast<std::size_t>(save_depth_)] =
                current_transform_;
            return;
        }

        // Deep nesting is rare. The ordinary path above is allocation-free;
        // if this fallback needs to grow, allocation completes before the
        // backend save so failure cannot desynchronize the two stacks.
        if (fail_transform_history_overflow_) {
            fail_transform_history_overflow_ = false;
            throw std::bad_alloc{};
        }
        transform_history_overflow_.push_back(current_transform_);
    }

    void discard_prepared_transform_snapshot() noexcept {
        if (save_depth_ >= kInlineTransformSaveDepth) {
            assert(!transform_history_overflow_.empty());
            transform_history_overflow_.pop_back();
        }
    }

    [[nodiscard]] Transform2D transform_snapshot(int depth) const noexcept {
        assert(depth >= 0);
        if (depth < kInlineTransformSaveDepth) {
            return transform_history_inline_[static_cast<std::size_t>(depth)];
        }
        const auto overflow_index =
            static_cast<std::size_t>(depth - kInlineTransformSaveDepth);
        assert(overflow_index < transform_history_overflow_.size());
        return transform_history_overflow_[overflow_index];
    }

    [[nodiscard]] static bool finite_transform(const Transform2D& transform) noexcept {
        return std::isfinite(transform.m00) &&
               std::isfinite(transform.m01) &&
               std::isfinite(transform.m02) &&
               std::isfinite(transform.m10) &&
               std::isfinite(transform.m11) &&
               std::isfinite(transform.m12);
    }

    template <class BackendMutation>
    void apply_logical_transform(const Transform2D& operation,
                                 BackendMutation&& backend_mutation) {
        if (!finite_transform(operation)) return;
        const Transform2D composed = current_transform_ * operation;
        if (!finite_transform(composed)) return;

        // Publish NativeUI state only after the backend mutation completed.
        // If a backend ever reports failure by throwing, both sides therefore
        // remain at the previous logical transform.
        std::forward<BackendMutation>(backend_mutation)();
        current_transform_ = composed;
    }

    void maybe_fail_layer(LayerFaultPoint point) {
        if (layer_fault_point_ != point) return;
        layer_fault_point_ = LayerFaultPoint::None;
        throw std::bad_alloc{};
    }

    [[nodiscard]] static bool finite_point(Point point) noexcept {
        return std::isfinite(point.x) && std::isfinite(point.y);
    }

    [[nodiscard]] static sk_sp<SkImageFilter> materialize_effect_filter(
        const Effect& effect) {
        switch (effect.kind_) {
            case Effect::Kind::GaussianBlur:
                return SkImageFilters::Blur(
                    effect.sigma_x_, effect.sigma_y_, SkTileMode::kDecal, nullptr);
            case Effect::Kind::DropShadow:
                return SkImageFilters::DropShadow(
                    effect.offset_.x,
                    effect.offset_.y,
                    effect.sigma_x_,
                    effect.sigma_y_,
                    to_sk_color(effect.color_),
                    nullptr,
                    nullptr);
            case Effect::Kind::DropShadowOnly:
                return SkImageFilters::DropShadowOnly(
                    effect.offset_.x,
                    effect.offset_.y,
                    effect.sigma_x_,
                    effect.sigma_y_,
                    to_sk_color(effect.color_),
                    nullptr,
                    nullptr);
        }
        return nullptr;
    }

    [[nodiscard]] bool effect_source_device_bounds(
        const SkRect& source_bounds,
        SkIRect& device_source) const noexcept {
        const SkMatrix matrix = canvas_.getLocalToDeviceAs3x3();
        if (!matrix.isFinite() || matrix.hasPerspective() ||
            !source_bounds.isFinite() || source_bounds.isEmpty()) {
            return false;
        }

        const double m00 = static_cast<double>(matrix.getScaleX());
        const double m01 = static_cast<double>(matrix.getSkewX());
        const double m02 = static_cast<double>(matrix.getTranslateX());
        const double m10 = static_cast<double>(matrix.getSkewY());
        const double m11 = static_cast<double>(matrix.getScaleY());
        const double m12 = static_cast<double>(matrix.getTranslateY());

        double min_x = std::numeric_limits<double>::infinity();
        double min_y = std::numeric_limits<double>::infinity();
        double max_x = -std::numeric_limits<double>::infinity();
        double max_y = -std::numeric_limits<double>::infinity();
        const double xs[2] = {
            static_cast<double>(source_bounds.left()),
            static_cast<double>(source_bounds.right()),
        };
        const double ys[2] = {
            static_cast<double>(source_bounds.top()),
            static_cast<double>(source_bounds.bottom()),
        };

        for (double x : xs) {
            for (double y : ys) {
                const double mapped_x = m00 * x + m01 * y + m02;
                const double mapped_y = m10 * x + m11 * y + m12;
                if (!std::isfinite(mapped_x) || !std::isfinite(mapped_y)) return false;
                min_x = (std::min)(min_x, mapped_x);
                min_y = (std::min)(min_y, mapped_y);
                max_x = (std::max)(max_x, mapped_x);
                max_y = (std::max)(max_y, mapped_y);
            }
        }

        const double left = std::floor(min_x);
        const double top = std::floor(min_y);
        const double right = std::ceil(max_x);
        const double bottom = std::ceil(max_y);

        // clipIRect converts integer coordinates to SkScalar internally.
        // Staying inside binary32's exact-integer range prevents an outward
        // edge from being rounded inward later.
        constexpr double max_exact_float_integer = 16777216.0; // 2^24
        if (!std::isfinite(left) || !std::isfinite(top) ||
            !std::isfinite(right) || !std::isfinite(bottom) ||
            left < -max_exact_float_integer || top < -max_exact_float_integer ||
            right > max_exact_float_integer || bottom > max_exact_float_integer ||
            !(left < right) || !(top < bottom)) {
            return false;
        }

        device_source = SkIRect::MakeLTRB(static_cast<int>(left),
                                          static_cast<int>(top),
                                          static_cast<int>(right),
                                          static_cast<int>(bottom));
        return !device_source.isEmpty();
    }

    [[nodiscard]] bool effect_filter_output_bounds(
        const SkImageFilter& filter,
        const SkIRect& device_source,
        SkIRect& device_output) const {
        const SkMatrix matrix = canvas_.getLocalToDeviceAs3x3();
        if (!matrix.isFinite() || matrix.hasPerspective()) return false;

        // This public Skia API is specifically documented for clipping and
        // temporary-buffer allocation: the result may be conservative but must
        // never be smaller than the real filtered output.
        const SkIRect output = filter.filterBounds(
            device_source,
            matrix,
            SkImageFilter::kForward_MapDirection);
        if (output.isEmpty()) return false;

        constexpr int max_exact_float_integer = 1 << 24;
        if (output.left() < -max_exact_float_integer ||
            output.top() < -max_exact_float_integer ||
            output.right() > max_exact_float_integer ||
            output.bottom() > max_exact_float_integer) {
            return false;
        }

        device_output = output;
        return true;
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

    static void apply_fill_source(SkPaint& paint, const ImageTexture& texture) {
        auto shader = detail::materialize_image_texture(texture);
        if (!shader) {
            throw std::runtime_error(
                "NativeUI image texture materialization returned no shader");
        }
        paint.setShader(std::move(shader));
    }

    static void apply_fill_source(
        SkPaint& paint,
        const std::shared_ptr<const detail::ShaderBrushSnapshot>& snapshot) {
        auto shader = detail::materialize_shader_brush(snapshot);
        if (!shader) {
            throw std::runtime_error(
                "NativeUI runtime shader materialization returned no shader");
        }
        paint.setShader(std::move(shader));
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

    [[nodiscard]] static SkPaint make_fill_paint(
        const ImageTexture& texture,
        PaintOptions options) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        apply_fill_source(paint, texture);
        apply_paint_options(paint, options);
        return paint;
    }

    [[nodiscard]] static SkPaint make_fill_paint(
        const std::shared_ptr<const detail::ShaderBrushSnapshot>& snapshot,
        PaintOptions options) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setStyle(SkPaint::kFill_Style);
        apply_fill_source(paint, snapshot);
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
        assert(static_cast<std::size_t>(save_depth_ > kInlineTransformSaveDepth
                                           ? save_depth_ - kInlineTransformSaveDepth
                                           : 0) == transform_history_overflow_.size());

        const int snapshot_depth = save_depth_ - 1;
        const Transform2D restored_transform = transform_snapshot(snapshot_depth);
        canvas_.restore();
        --save_depth_;
        current_transform_ = restored_transform;
        if (snapshot_depth >= kInlineTransformSaveDepth) {
            transform_history_overflow_.pop_back();
        }
    }

    SkCanvas& canvas_;
    Transform2D current_transform_{};
    std::array<Transform2D, kInlineTransformSaveDepth> transform_history_inline_{};
    std::vector<Transform2D> transform_history_overflow_;
    int save_depth_{};
    int restore_floor_{};
    LayerFaultPoint layer_fault_point_{LayerFaultPoint::None};
    bool fail_transform_history_overflow_{};
    bool used_effects_{};
};

/// Platform-neutral services used by retained input/focus/paint callbacks.
///
/// UI/Tree code borrows a PlatformServices implementation; ownership is not
/// transferred. The implementation must remain alive for every callback or UI
/// operation that can invoke it. These services are UI/main-thread facilities,
/// may cross native platform boundaries, and are not audio-real-time safe.
///
/// Custom/headless backends may override only the services they support. Text
/// measurement has a TextService fallback, text-input and native pointer-capture
/// hooks default to no-op, drag/drop defaults to rejection, while clipboard
/// write/request are required operations.
class PlatformServices {
public:
    /// Polymorphic destructor for platform-service implementations.
    virtual ~PlatformServices() = default;
    /// Measure UTF-8 text in logical units using `style`.
    ///
    /// The default implementation delegates to TextService::measure(). The
    /// returned value is owned by the caller; `text` is borrowed only for the call.
    [[nodiscard]] virtual TextMetrics text_metrics(std::string_view text, const TextStyle& style) {
        return TextService::measure(text, style);
    }
    /// Width-only convenience measurement in logical units.
    ///
    /// The default implementation delegates to TextService::measure().
    [[nodiscard]] virtual float text_width(std::string_view text, float size) {
        return TextService::measure(text, size).width;
    }
    /// Enable/disable native text/IME integration for the focused editor.
    ///
    /// `area` is the logical text-input rectangle and `cursor_offset` a logical
    /// caret offset used by the platform bridge for candidate/caret placement.
    /// Disabling text input permits callers to pass zero/default geometry.
    virtual void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) {
        (void)active; (void)area; (void)cursor_offset;
    }
    // Pointer capture remains owned by the retained tree. These platform-neutral
    // lifecycle hooks let a concrete native view mirror only real none<->owner
    // transitions when its OS requires an explicit native pointer grab.
    /// Mirror a retained none->owner pointer-capture transition to the OS.
    ///
    /// Retained Tree state remains authoritative; this hook carries no target
    /// identity and must not create a second platform-owned capture model.
    virtual void begin_pointer_capture() noexcept {}
    /// Mirror the retained owner->none pointer-capture transition to the OS.
    virtual void end_pointer_capture() noexcept {}
    /// Publish UTF-8 plain text to the platform clipboard.
    ///
    /// `text` is borrowed for this call; an asynchronous backend must make its
    /// own copy before returning.
    virtual void set_clipboard_text(std::string_view text) = 0;
    /// Request clipboard text delivery through the normal platform input/data path.
    ///
    /// This is a request, not a synchronous getter; no text is returned here.
    virtual void request_clipboard_text() = 0;
    // Drag-and-drop is synchronous at offer time. The default implementation
    // rejects support so headless/custom platform services need no DnD code.
    /// Accept the current synchronous drag/drop offer for MIME/data `type`.
    ///
    /// `region` is the logical target region. Returns true only when the backend
    /// accepted the current offer; the default implementation rejects it.
    virtual bool accept_drop(std::string_view type, Rect region) {
        (void)type; (void)region; return false;
    }
    /// Explicitly reject the current drag/drop offer for a logical target region.
    ///
    /// The default implementation is a no-op for backends without drop support.
    virtual void reject_drop(Rect region) { (void)region; }
};


} // namespace ui
