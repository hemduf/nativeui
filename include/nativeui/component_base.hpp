#pragma once

#include <nativeui/constraints.hpp>
#include <nativeui/image.hpp>
#include <nativeui/input.hpp>
#include <nativeui/invalidation.hpp>
#include <nativeui/paint.hpp>
#include <nativeui/semantics.hpp>
#include <nativeui/svg.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

namespace detail {
class OverlayService;
} // namespace detail

class Tree;

/// Dimensionless main-axis flex weights advertised by a retained component.
///
/// `grow` participates when a flex container has positive free space and
/// `shrink` participates when content must contract. Zero opts out of the
/// corresponding redistribution step. These values are layout metadata only.
struct FlexFactors {
    /// Relative share of positive free space.
    float grow{};
    /// Relative share of required shrinkage.
    float shrink{};
};

/// Constraint-aware size information exchanged between parent and child layout.
///
/// `minimum` and `preferred` use NativeUI logical pixels. `flex` is
/// dimensionless. Values are owned snapshots for one measurement pass and do
/// not track later component, constraint or viewport changes.
struct ChildMetrics {
    /// Smallest logical size reported for the current measurement constraints.
    Size minimum{};
    /// Preferred logical size after current constraints are considered.
    Size preferred{};
    /// Optional main-axis flex metadata consumed by flex-aware parents.
    FlexFactors flex{};

    /// Construct zero minimum/preferred sizes with no flex participation.
    ChildMetrics() = default;
    /// Construct an intrinsic preferred-size result with a zero minimum.
    explicit ChildMetrics(Size preferred_size) : preferred(preferred_size) {}
    /// Construct explicit minimum and preferred sizes in logical pixels.
    ChildMetrics(Size minimum_size, Size preferred_size)
        : minimum(minimum_size), preferred(preferred_size) {}
    /// Construct explicit sizes plus dimensionless flex metadata.
    ChildMetrics(Size minimum_size, Size preferred_size, FlexFactors flex_factors)
        : minimum(minimum_size), preferred(preferred_size), flex(flex_factors) {}
};

/// Parent-computed placement for one retained child.
///
/// `bounds` is an owned logical-coordinate snapshot in the parent's space.
struct ChildPlacement {
    /// Final child bounds for the current retained layout pass.
    Rect bounds;
};

/// Stable retained-node identity within one tree/UI lifetime.
///
/// This is an identity token, not an owning handle and not a cross-tree or
/// cross-thread synchronization primitive. Stored IDs must tolerate teardown.
using NodeId = std::uint64_t;
/// Sentinel used when no retained node identity is available.
inline constexpr NodeId kInvalidNodeId = 0;

/// Retained visibility policy before ancestry is resolved.
enum class VisibilityMode {
    /// Participate in layout, painting, semantics and ordinary interaction.
    Visible,
    /// Keep layout participation while suppressing presentation/interaction.
    Hidden,
    /// Remove ordinary visible layout/presentation participation.
    Collapsed,
};

/// Effective/local retained availability state for one component.
///
/// Ancestors can only make descendants less available. `interactive()` tests
/// visibility plus enabled state only; read-only remains a separate editing
/// policy consumed by controls that support it.
struct ComponentAvailability {
    /// Current retained visibility policy.
    VisibilityMode visibility{VisibilityMode::Visible};
    /// Whether ordinary interaction is enabled.
    bool enabled{true};
    /// Whether mutation/editing should be suppressed while remaining present.
    bool read_only{};

    /// Return true when visible and enabled.
    ///
    /// `read_only` is intentionally not folded into this result.
    [[nodiscard]] bool interactive() const noexcept {
        return visibility == VisibilityMode::Visible && enabled;
    }

    /// Compare all availability dimensions by value.
    bool operator==(const ComponentAvailability&) const = default;
};

/// Borrowed services and geometry for one `Component::paint()` callback.
///
/// The context borrows `Painter` and `PlatformServices`; neither is kept alive
/// by this object and obtained references must not escape the callback. Geometry
/// uses NativeUI logical pixels. Painting/text measurement are UI/render work
/// and are not audio/DSP real-time APIs.
class PaintContext {
public:
    /// Construct with an effective clip equal to `bounds`.
    PaintContext(Painter& painter, Rect bounds, bool focused, PlatformServices& platform)
        : PaintContext(painter, bounds, bounds, focused, platform) {}

    /// Construct with explicit retained bounds and inherited logical clip.
    /// All referenced services are borrowed for the paint-callback lifetime.
    PaintContext(Painter& painter,
                 Rect bounds,
                 Rect clip_bounds,
                 bool focused,
                 PlatformServices& platform)
        : painter_(painter),
          bounds_(bounds),
          clip_bounds_(clip_bounds),
          focused_(focused),
          platform_(platform) {}

    /// Retained component bounds in the current logical coordinate space.
    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }
    /// Effective inherited paint clip in logical coordinates.
    [[nodiscard]] Rect clip_bounds() const noexcept { return clip_bounds_; }
    /// Whether this component is the current keyboard-focus owner.
    [[nodiscard]] bool focused() const noexcept { return focused_; }
    /// Borrow the active painter; do not retain the reference after `paint()`.
    [[nodiscard]] Painter& painter() noexcept { return painter_; }
    /// Measure borrowed UTF-8 text through the owning platform service.
    /// The returned metrics are an owned value in logical pixels.
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return platform_.text_metrics(text, style);
    }
    /// Convenience width measurement; `size` and the result are logical pixels.
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return platform_.text_width(text, size);
    }

private:
    Painter& painter_;
    Rect bounds_;
    Rect clip_bounds_;
    bool focused_{};
    PlatformServices& platform_;
};

/// Canvas-style borrowed facade over the active `Painter`.
///
/// The facade does not own the painter. Geometry, stroke widths, radii and text
/// sizes use logical pixels; angular arguments are radians. Calls forward
/// immediately to the painter and inherit its clip/transform stack. The context
/// must stay inside the paint callback and is not an audio/DSP RT API.
class CanvasContext2D {
public:
    /// Borrow a painter for a canvas with the supplied logical bounds.
    CanvasContext2D(Painter& painter, Rect bounds, bool focused)
        : painter_(painter), bounds_(bounds), focused_(focused) {}

    /// Return the canvas extent in logical pixels.
    [[nodiscard]] Size size() const noexcept { return Size{bounds_.w, bounds_.h}; }
    /// Return logical canvas width.
    [[nodiscard]] float width() const noexcept { return bounds_.w; }
    /// Return logical canvas height.
    [[nodiscard]] float height() const noexcept { return bounds_.h; }
    /// Return the focus snapshot captured for this paint callback.
    [[nodiscard]] bool focused() const noexcept { return focused_; }

    /// Measure borrowed UTF-8 text and return owned logical-pixel metrics.
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return TextService::measure(text, style);
    }
    /// Convenience text width; `size` and result use logical pixels.
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return TextService::measure(text, size).width;
    }

    /// Fill an axis-aligned logical rectangle with a solid color.
    ///
    /// `rect` is expressed in the current logical coordinate space and is
    /// transformed/clipped by the borrowed Painter state immediately.
    void fill_rect(Rect rect, Color color) {
        painter_.fill_rounded_rect(rect, 0.0f, color);
    }

    /// Fill an axis-aligned logical rectangle with a borrowed linear gradient.
    ///
    /// Gradient geometry and `rect` use logical coordinates. `options` is
    /// copied for this draw; neither the gradient nor options are retained.
    void fill_rect(Rect rect, const LinearGradient& gradient, PaintOptions options = {}) {
        painter_.fill_rounded_rect(rect, 0.0f, gradient, options);
    }

    /// Fill an axis-aligned logical rectangle with a borrowed radial gradient.
    ///
    /// Gradient geometry and `rect` use logical coordinates. The call executes
    /// synchronously through the active Painter and retains no gradient reference.
    void fill_rect(Rect rect, const RadialGradient& gradient, PaintOptions options = {}) {
        painter_.fill_rounded_rect(rect, 0.0f, gradient, options);
    }

    /// Fill an axis-aligned logical rectangle with a borrowed Brush.
    ///
    /// Brush/image/shader resources follow Painter's synchronous borrowing rules;
    /// callers must keep referenced resources alive for this call.
    void fill_rect(Rect rect, const Brush& brush, PaintOptions options = {}) {
        painter_.fill_rounded_rect(rect, 0.0f, brush, options);
    }

    /// Stroke a logical rectangle; `width` is in logical pixels.
    void stroke_rect(Rect rect, float width, Color color) {
        painter_.stroke_rounded_rect(rect, 0.0f, width, color);
    }

    /// Stroke a logical rectangle with a borrowed Brush.
    ///
    /// `width` is a logical-pixel stroke width and `options` is copied for
    /// this synchronous draw.
    void stroke_rect(Rect rect, float width, const Brush& brush,
                     PaintOptions options = {}) {
        painter_.stroke_rounded_rect(rect, 0.0f, width, brush, options);
    }

    /// Fill a rounded logical rectangle; `radius` is in logical pixels.
    void fill_rounded_rect(Rect rect, float radius, Color color) {
        painter_.fill_rounded_rect(rect, radius, color);
    }

    /// Fill a rounded logical rectangle with a borrowed linear gradient.
    ///
    /// `rect`, `radius`, and gradient geometry are logical. The gradient is
    /// consumed synchronously and is not retained by this facade.
    void fill_rounded_rect(Rect rect, float radius, const LinearGradient& gradient,
                           PaintOptions options = {}) {
        painter_.fill_rounded_rect(rect, radius, gradient, options);
    }

    /// Fill a rounded logical rectangle with a borrowed radial gradient.
    ///
    /// `rect`, `radius`, and gradient geometry are logical. The gradient is
    /// consumed synchronously and is not retained by this facade.
    void fill_rounded_rect(Rect rect, float radius, const RadialGradient& gradient,
                           PaintOptions options = {}) {
        painter_.fill_rounded_rect(rect, radius, gradient, options);
    }

    /// Fill a rounded logical rectangle with a borrowed Brush.
    ///
    /// `rect` and `radius` use logical pixels; Painter owns any validation,
    /// fallback, and shader/image materialization behavior for the Brush.
    void fill_rounded_rect(Rect rect, float radius, const Brush& brush,
                           PaintOptions options = {}) {
        painter_.fill_rounded_rect(rect, radius, brush, options);
    }

    /// Stroke a rounded rectangle; radius/width use logical pixels.
    void stroke_rounded_rect(Rect rect, float radius, float width, Color color) {
        painter_.stroke_rounded_rect(rect, radius, width, color);
    }

    /// Stroke a rounded rectangle with a borrowed Brush.
    ///
    /// `radius` and `width` are logical pixels. Brush resources are borrowed
    /// for this call and remain owned by the caller.
    void stroke_rounded_rect(Rect rect, float radius, float width, const Brush& brush,
                             PaintOptions options = {}) {
        painter_.stroke_rounded_rect(rect, radius, width, brush, options);
    }

    /// Draw a filled circle in logical coordinates.
    void circle(Point center, float radius, Color color) {
        painter_.circle(center, radius, color);
    }

    /// Draw a filled logical circle with a borrowed Brush.
    ///
    /// `center` and `radius` are transformed by the current Painter state;
    /// the Brush is not retained after the synchronous draw.
    void circle(Point center, float radius, const Brush& brush, PaintOptions options = {}) {
        painter_.circle(center, radius, brush, options);
    }

    /// Stroke an arc; center/radius/width are logical and start/end are radians.
    void arc(Point center, float radius, float start, float end, float width, Color color) {
        painter_.arc(center, radius, start, end, width, color);
    }

    /// Stroke an arc with a borrowed Brush.
    ///
    /// Center/radius/width use logical coordinates, `start`/`end` are radians,
    /// and Brush resources are borrowed only for this call.
    void arc(Point center, float radius, float start, float end, float width,
             const Brush& brush, PaintOptions options = {}) {
        painter_.arc(center, radius, start, end, width, brush, options);
    }

    /// Stroke a logical line with a logical-pixel width.
    void line(Point a, Point b, float width, Color color) {
        painter_.line(a, b, width, color);
    }

    /// Stroke a logical line with a borrowed Brush.
    ///
    /// Endpoints and `width` use logical coordinates/pixels and are transformed
    /// by the current Painter state.
    void line(Point a, Point b, float width, const Brush& brush,
              PaintOptions options = {}) {
        painter_.line(a, b, width, brush, options);
    }

    /// Fill a borrowed path under the current transform/clip.
    void fill_path(const Path& path, Color color) {
        painter_.fill_path(path, color);
    }

    /// Fill a borrowed Path with a borrowed Brush.
    ///
    /// Both objects need remain valid only for this synchronous call; neither is
    /// retained by CanvasContext2D.
    void fill_path(const Path& path, const Brush& brush, PaintOptions options = {}) {
        painter_.fill_path(path, brush, options);
    }

    /// Stroke a borrowed path with a copied stroke-style value.
    void stroke_path(const Path& path, Color color, StrokeStyle style = {}) {
        painter_.stroke_path(path, color, style);
    }

    /// Stroke a borrowed Path with a borrowed Brush and copied style/options.
    ///
    /// Path/Brush lifetimes need cover only this call. Stroke dimensions are
    /// interpreted in the Painter's current logical coordinate system.
    void stroke_path(const Path& path, const Brush& brush, StrokeStyle style = {},
                     PaintOptions options = {}) {
        painter_.stroke_path(path, brush, style, options);
    }

    /// Draw a borrowed decoded image into a logical destination rectangle.
    /// The context does not retain image ownership.
    void draw_image(const Image& image,
                    Rect destination,
                    ImageFit fit = ImageFit::Fill) {
        detail::draw_image(painter_, image, destination, fit);
    }

    /// Draw a logical source sub-rectangle of a borrowed image to destination.
    void draw_image(const Image& image,
                    Rect source,
                    Rect destination,
                    ImageFit fit = ImageFit::Fill) {
        detail::draw_image(painter_, image, source, destination, fit);
    }

    /// Draw a borrowed parsed SVG icon into logical destination bounds.
    void draw_svg(const SvgIcon& icon, Rect destination) {
        detail::draw_svg(painter_, icon, destination);
    }

    /// Draw borrowed UTF-8 text at a logical position using the supplied style.
    ///
    /// `text` and `style` are borrowed only for this call. Text measurement
    /// and font fallback follow Painter/TextService behavior and may allocate.
    void text(Point position, std::string_view text, const TextStyle& style) {
        painter_.text(position, text, style);
    }

    /// Draw borrowed UTF-8 text with a convenience logical-pixel size/color style.
    ///
    /// `position` and `size` use logical coordinates. `align` controls the
    /// horizontal anchor exactly as the equivalent TextStyle overload.
    void text(Point position, std::string_view text, float size, Color color,
              TextAlign align = TextAlign::Left) {
        painter_.text(position, text, size, color, align);
    }

    /// Intersect the active painter clip with a logical rectangle.
    ///
    /// Clip state is stack-scoped; balance this call with `pop_clip()` before
    /// the enclosing paint callback returns.
    void push_clip(Rect rect) { painter_.push_clip(rect); }
    /// Pop the most recently pushed clip.
    ///
    /// The same balancing/underflow contract as the underlying Painter applies.
    void pop_clip() { painter_.pop_clip(); }

    /// Save the current painter transform/clip state.
    ///
    /// Balance with `restore()`; state is owned by the borrowed Painter, not by
    /// CanvasContext2D.
    void save() { painter_.save(); }
    /// Restore the most recently saved painter state.
    void restore() { painter_.restore(); }
    /// Append a logical X/Y translation to the active transform.
    void translate(float x, float y) { painter_.translate(x, y); }
    /// Append a logical translation vector to the active transform.
    void translate(Point offset) { painter_.translate(offset); }
    /// Append independent dimensionless X/Y scale factors.
    void scale(float x, float y) { painter_.scale(x, y); }
    /// Append one dimensionless uniform scale factor on both axes.
    void scale(float uniform) { painter_.scale(uniform); }
    /// Append a rotation in radians.
    void rotate(float radians) { painter_.rotate(radians); }
    /// Append the supplied transform value by immediate copy/borrow semantics of Painter.
    void concat(const Transform2D& transform) { painter_.concat(transform); }

private:
    Painter& painter_;
    Rect bounds_{};
    bool focused_{};
};

/// Borrowed services for the duration of one `Component::input()` callback.
///
/// Do not retain this object or references to services obtained through it.
/// Bounds and geometry use NativeUI logical coordinates. Pointer capture/release
/// applies to the pointer identity associated with the current callback and is
/// guarded against stale/re-entrant contact generations.
class InputContext {
public:
    /// Construct a callback-scoped input facade from borrowed platform services
    /// and owned callback functions.
    ///
    /// `bounds` is the receiving component's logical rectangle. `platform` is
    /// borrowed for the lifetime of this context and must outlive it. The supplied
    /// invalidation/capture/release callables are moved into the context and may
    /// allocate or throw when invoked. This constructor is primarily useful to
    /// adapters/tests; retained Tree dispatch supplies generation-aware capture
    /// callbacks through its internal constructor.
    InputContext(
        Rect bounds,
        PlatformServices& platform,
        std::function<void()> invalidate,
        std::function<void()> invalidate_layout,
        std::function<void()> capture,
        std::function<void()> release)
        : bounds_(bounds),
          platform_(platform),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)),
          legacy_capture_(std::move(capture)),
          legacy_release_(std::move(release)) {}

    /// Return retained bounds of the receiving component in logical coordinates.
    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }

    /// Measure borrowed UTF-8 text through the active PlatformServices.
    ///
    /// `text`/`style` need remain valid only for the call. Returned metrics
    /// are an owned logical-pixel snapshot; platform/font work may allocate.
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return platform_.text_metrics(text, style);
    }
    /// Convenience UTF-8 width measurement in logical pixels.
    ///
    /// `text` is borrowed for the call; `size` and the returned width use
    /// logical pixels.
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return platform_.text_width(text, size);
    }
    /// Replace the platform clipboard text from a payload borrowed for this call.
    ///
    /// Platform-specific failure behavior is owned by PlatformServices; this
    /// wrapper adds no retry, fallback, or cross-thread synchronization.
    void set_clipboard_text(std::string_view text) { platform_.set_clipboard_text(text); }

    /// Ask the platform adapter to deliver clipboard text through its normal
    /// NativeUI input path.
    ///
    /// The request is asynchronous only if the PlatformServices implementation
    /// defines it that way; InputContext does not retain a completion callback.
    void request_clipboard_text() { platform_.request_clipboard_text(); }

    /// Accept one exact advertised drop type for this component's logical bounds.
    ///
    /// Returns the PlatformServices acceptance result. `type` is borrowed for
    /// the call and no wildcard/implicit conversion is applied here.
    [[nodiscard]] bool accept_drop(std::string_view type) {
        return platform_.accept_drop(type, bounds_);
    }
    /// Reject the current drop offer for this component's logical bounds.
    void reject_drop() { platform_.reject_drop(bounds_); }

    /// Start, update, or stop native text input for this focused editor.
    ///
    /// `area` and `cursor_offset` are logical geometry; the platform boundary
    /// performs device scaling. The call is synchronous at this API boundary and
    /// inherits PlatformServices error/reentrancy behavior.
    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) {
        platform_.set_text_input(active, area, cursor_offset);
    }
    /// Request repaint of this component without recomputing layout.
    ///
    /// The retained invalidator is invoked synchronously and may re-enter/throw
    /// according to the owning Tree's invalidation contract.
    void invalidate() const { invalidate_(); }
    /// Request layout from this component through its ancestors, then repaint.
    ///
    /// The retained invalidator is invoked synchronously and may re-enter/throw;
    /// geometry is recomputed later at the Tree/UI layout checkpoint.
    void invalidate_layout() const { invalidate_layout_(); }
    /// Capture the current pointer contact to this retained component.
    ///
    /// Capture requests from terminal PointerUp/PointerCancel/ContextMenu
    /// callbacks are ignored. Stale outer callbacks cannot replace a newer
    /// re-entrant capture for the same pointer ID.
    void capture_pointer() const {
        if (capture_) {
            if (pointer_action_.capture_allowed) capture_(pointer_action_);
        } else if (legacy_capture_) {
            legacy_capture_();
        }
    }
    /// Release capture owned by this component for the current pointer contact.
    /// A stale callback cannot release a newer re-entrant capture generation.
    void release_pointer() const {
        if (release_) release_(pointer_action_);
        else if (legacy_release_) legacy_release_();
    }

private:
    friend class Tree;

    struct PointerAction {
        PointerId id{};
        std::uint64_t interaction_token{};
        bool capture_allowed{};
    };

    InputContext(
        Rect bounds,
        PlatformServices& platform,
        std::function<void()> invalidate,
        std::function<void()> invalidate_layout,
        PointerAction pointer_action,
        std::function<void(const PointerAction&)> capture,
        std::function<void(const PointerAction&)> release)
        : bounds_(bounds),
          platform_(platform),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)),
          pointer_action_(pointer_action),
          capture_(std::move(capture)),
          release_(std::move(release)) {}

    Rect bounds_{};
    PlatformServices& platform_;
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
    PointerAction pointer_action_{};
    std::function<void(const PointerAction&)> capture_;
    std::function<void(const PointerAction&)> release_;
    std::function<void()> legacy_capture_;
    std::function<void()> legacy_release_;
};

/// Canvas-friendly callback-scoped facade over `InputContext`.
///
/// It exposes logical size rather than absolute bounds and forwards text
/// measurement, invalidation, pointer capture, clipboard and drag/drop operations
/// to the same borrowed context. It does not own PlatformServices or retained
/// interaction state and must not outlive the enclosing input callback.
class CanvasInputContext {
public:
    /// Borrow an existing callback-scoped input context.
    explicit CanvasInputContext(InputContext& context) : context_(context) {}

    /// Return the receiving component extent in logical pixels.
    [[nodiscard]] Size size() const noexcept {
        const auto b = context_.bounds();
        return Size{b.w, b.h};
    }

    /// Measure borrowed UTF-8 text through the owning platform service.
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return context_.text_metrics(text, style);
    }
    /// Convenience width measurement in logical pixels.
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return context_.text_width(text, size);
    }

    /// Request paint invalidation for the receiving component.
    void invalidate() const { context_.invalidate(); }
    /// Request retained layout plus paint invalidation.
    void invalidate_layout() const { context_.invalidate_layout(); }
    /// Capture the current eligible pointer contact to this retained target.
    void capture_pointer() const { context_.capture_pointer(); }
    /// Release capture owned by this target for the current contact.
    void release_pointer() const { context_.release_pointer(); }
    /// Replace platform clipboard text with the borrowed payload.
    void set_clipboard_text(std::string_view text) { context_.set_clipboard_text(text); }
    /// Request clipboard text through the normal platform/input path.
    void request_clipboard_text() { context_.request_clipboard_text(); }
    /// Accept exactly one advertised drop type for this component.
    [[nodiscard]] bool accept_drop(std::string_view type) { return context_.accept_drop(type); }
    /// Reject the current drop offer for this component.
    void reject_drop() { context_.reject_drop(); }

private:
    InputContext& context_;
};

/// Borrowed services for one `Component::focus_changed()` callback.
///
/// This context is valid only for the callback duration. It provides the
/// focused component's logical bounds, text metrics/text-input platform seam,
/// and paint/layout invalidation.
class FocusContext {
public:
    /// Construct one callback-scoped focus-transition context.
    ///
    /// `bounds` is an owned logical snapshot. `platform` is borrowed for this
    /// context's lifetime, while invalidation callables are moved into the
    /// context. Construction itself invokes no platform or application callback.
    FocusContext(Rect bounds,
                 PlatformServices& platform,
                 std::function<void()> invalidate,
                 std::function<void()> invalidate_layout)
        : bounds_(bounds),
          platform_(platform),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)) {}

    /// Return logical bounds captured for this focus transition.
    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }

    /// Measure borrowed UTF-8 text and return owned logical-pixel metrics.
    ///
    /// The PlatformServices implementation owns font lookup/fallback and any
    /// allocation or error behavior.
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return platform_.text_metrics(text, style);
    }

    /// Convenience borrowed-text width measurement in logical pixels.
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return platform_.text_width(text, size);
    }

    /// Start, update, or stop platform text input for the focused component.
    ///
    /// `area` and `cursor_offset` are logical geometry; the platform boundary
    /// owns device scaling. The call inherits PlatformServices error semantics.
    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) {
        platform_.set_text_input(active, area, cursor_offset);
    }

    /// Request paint invalidation for the focused component.
    ///
    /// Notification is synchronous through the retained invalidator and may
    /// re-enter or throw according to the owning Tree contract.
    void invalidate() const { invalidate_(); }

    /// Request retained layout plus paint invalidation through ancestors.
    ///
    /// Geometry is recomputed later at the normal layout checkpoint; the
    /// invalidation callback itself is invoked synchronously.
    void invalidate_layout() const { invalidate_layout_(); }

private:
    Rect bounds_{};
    PlatformServices& platform_;
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
};

/// Services supplied for the retained `Component::mount()` transition.
///
/// The context itself is callback-scoped. Returned invalidator callables are
/// copyable handles intended for later UI-domain state changes; they do not
/// transfer ownership of the tree or component. The overlay seam is borrowed.
/// Mount/unmount and invalidation scheduling are UI/main-thread operations.
class MountContext {
public:
    /// Construct mount services for one retained identity.
    ///
    /// `node_id` is an identity token, not ownership. Invalidation callables are
    /// moved into this context and may be copied out through the accessors below.
    /// `overlay_service` is borrowed and may be null; it must not be retained as
    /// an owning pointer or used outside the owning UI/main-thread domain.
    MountContext(NodeId node_id,
                 std::function<void()> invalidate,
                 std::function<void()> invalidate_layout,
                 std::function<void()> invalidate_focus,
                 std::function<void()> invalidate_availability = {},
                 detail::OverlayService* overlay_service = nullptr)
        : node_id_(node_id),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)),
          invalidate_focus_(std::move(invalidate_focus)),
          invalidate_availability_(std::move(invalidate_availability)),
          overlay_service_(overlay_service) {}

    /// Return this component's retained identity in the current tree.
    ///
    /// The value does not keep the node or Tree alive and may become stale after
    /// unmount/reconciliation.
    [[nodiscard]] NodeId node_id() const noexcept { return node_id_; }

    /// Return a copyable callback for state changes that affect only painting.
    ///
    /// The returned callable is intended for later UI-domain use. It does not own
    /// the component/Tree; after the retained identity disappears, generated Tree
    /// invalidators safely become inert according to the owning runtime.
    [[nodiscard]] std::function<void()> invalidator() const {
        return invalidate_ ? invalidate_ : make_invalidator(invalidator_factory_.invalidate);
    }

    /// Return a copyable callback for changes that can affect measurement/layout.
    ///
    /// Invocation schedules retained layout and the paint damage implied by
    /// geometry movement; it is not an immediate synchronous layout pass.
    [[nodiscard]] std::function<void()> layout_invalidator() const {
        return invalidate_layout_ ? invalidate_layout_
                                  : make_invalidator(invalidator_factory_.invalidate_layout);
    }

    /// Return a copyable callback for changes that affect focusability/scope state.
    ///
    /// Invocation requests focus-structure reconciliation at the runtime's safe
    /// checkpoint; application focus callbacks may run during that later repair.
    [[nodiscard]] std::function<void()> focus_invalidator() const {
        return invalidate_focus_ ? invalidate_focus_
                                 : make_invalidator(invalidator_factory_.invalidate_focus);
    }

    /// Return a copyable callback for visibility/enabled/read-only changes.
    ///
    /// The returned function is always callable; when no availability invalidator
    /// exists this accessor returns an inert no-op rather than an empty function.
    [[nodiscard]] std::function<void()> availability_invalidator() const {
        auto callback = invalidate_availability_
                            ? invalidate_availability_
                            : make_invalidator(invalidator_factory_.invalidate_availability);
        return callback ? std::move(callback) : std::function<void()>{[] {}};
    }

    /// Borrow the optional per-UI overlay service.
    ///
    /// Returns null for direct/headless retained trees without a UI overlay owner.
    /// The pointer is non-owning and must not escape the owning UI lifetime or be
    /// used from worker/audio threads.
    [[nodiscard]] detail::OverlayService* overlay_service() const noexcept {
        return overlay_service_;
    }

private:
    friend class Tree;

    using InvalidatorFactoryFn = std::function<void()> (*)(void*, NodeId);
    struct InvalidatorFactory {
        void* owner{};
        InvalidatorFactoryFn invalidate{};
        InvalidatorFactoryFn invalidate_layout{};
        InvalidatorFactoryFn invalidate_focus{};
        InvalidatorFactoryFn invalidate_availability{};
    };

    MountContext(NodeId node_id,
                 InvalidatorFactory invalidator_factory,
                 detail::OverlayService* overlay_service)
        : node_id_(node_id),
          invalidator_factory_(invalidator_factory),
          overlay_service_(overlay_service) {}

    [[nodiscard]] std::function<void()> make_invalidator(InvalidatorFactoryFn factory) const {
        return factory ? factory(invalidator_factory_.owner, node_id_) : std::function<void()>{};
    }

    NodeId node_id_{kInvalidNodeId};
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
    std::function<void()> invalidate_focus_;
    std::function<void()> invalidate_availability_;
    InvalidatorFactory invalidator_factory_{};
    detail::OverlayService* overlay_service_{};
};

/// Borrowed geometry/invalidation services for activate/deactivate/unmount.
///
/// The object is valid only for the lifecycle callback receiving it. Geometry
/// is a logical snapshot for that transition; do not retain this context.
class LifecycleContext {
public:
    /// Construct one callback-scoped lifecycle context.
    ///
    /// `node_id` and `bounds` are owned snapshots for the transition. The
    /// invalidation callbacks are moved into the context and invoke the owning
    /// retained runtime synchronously when called.
    LifecycleContext(NodeId node_id,
                     Rect bounds,
                     std::function<void()> invalidate,
                     std::function<void()> invalidate_layout)
        : node_id_(node_id),
          bounds_(bounds),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)) {}

    /// Return retained identity for this lifecycle callback.
    ///
    /// The value is non-owning and may be stale after unmount/reconciliation.
    [[nodiscard]] NodeId node_id() const noexcept { return node_id_; }

    /// Return logical component bounds captured for this lifecycle transition.
    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }

    /// Request paint invalidation through the owning retained runtime.
    ///
    /// The callback is invoked synchronously and may re-enter/throw according to
    /// Tree invalidation rules.
    void invalidate() const { invalidate_(); }

    /// Request layout plus paint invalidation through retained ancestors.
    ///
    /// The request is synchronous, while actual geometry recomputation occurs at
    /// the next safe layout checkpoint.
    void invalidate_layout() const { invalidate_layout_(); }

private:
    NodeId node_id_{kInvalidNodeId};
    Rect bounds_{};
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
};

/// Polymorphic protocol implemented by retained custom controls.
///
/// A component is owned by its retained `Node` after a `Spec` factory
/// materializes it; application code normally owns only the originating recipe
/// and external state, not the live Component object. Every virtual below runs
/// in the owning Tree/UI domain. Callback arguments and child-metric containers
/// are borrowed for that call unless documented otherwise and must not escape.
///
/// Virtuals may be invoked from measurement, reconciliation, input, lifecycle or
/// paint transactions and may propagate ordinary C++ exceptions through the
/// operation that entered them. Structural work requested re-entrantly is
/// reconciled only at Tree/UI safe checkpoints; custom components must not mutate
/// retained Node storage directly from a callback. None of these hooks is an
/// audio/DSP real-time entry point.
class Component {
public:
    /// Destroy the component when its owning retained node is torn down.
    virtual ~Component() = default;

    /// Report whether this node is eligible to own keyboard focus.
    ///
    /// The answer is a local capability only. Effective visibility/enabled state,
    /// active focus scopes and retained-tree policy can still make the node
    /// ineligible. If this value can change, request `focus_invalidator()` from
    /// the stored MountContext handle so focus structure is reconciled.
    [[nodiscard]] virtual bool focusable() const noexcept { return false; }

    /// Report whether pointer hit testing may start a route at this node.
    ///
    /// This is independent from current effective availability, which the Tree
    /// applies separately. The default preserves the historical contract that
    /// focusable controls are pointer targets; non-focusable controls must opt in.
    /// Dynamic changes require the same focus/interaction reconciliation discipline
    /// as other targetability changes.
    [[nodiscard]] virtual bool pointer_targetable() const noexcept { return focusable(); }

    /// Return this component's local availability before ancestry is resolved.
    ///
    /// The Tree combines the owned value monotonically with ancestor availability
    /// and caches the effective result. Components whose local value changes after
    /// mount must invoke the MountContext availability invalidator; returning a
    /// value here does not itself schedule repaint/layout/focus repair.
    [[nodiscard]] virtual ComponentAvailability local_availability() const noexcept { return {}; }

    /// Return the ancestry-resolved availability snapshot cached by the Tree.
    ///
    /// The returned value is owned and remains valid after later reconciliation;
    /// it does not force availability recomputation.
    [[nodiscard]] ComponentAvailability effective_availability() const noexcept {
        return effective_availability_;
    }
    /// Return the cached effective visibility without triggering reconciliation.
    [[nodiscard]] VisibilityMode effective_visibility() const noexcept {
        return effective_availability_.visibility;
    }
    /// Return the cached effective enabled state without triggering reconciliation.
    [[nodiscard]] bool effective_enabled() const noexcept {
        return effective_availability_.enabled;
    }
    /// Return the cached effective read-only state without triggering reconciliation.
    [[nodiscard]] bool effective_read_only() const noexcept {
        return effective_availability_.read_only;
    }

    /// Whether this component defines a focus scope for retained descendants.
    ///
    /// Normal components return false and ignore the remaining scope metadata.
    [[nodiscard]] virtual bool is_focus_scope() const noexcept { return false; }

    /// Whether this focus scope currently participates in focus routing.
    ///
    /// Returning false excludes the scope subtree from ordinary scoped traversal
    /// until focus structure is invalidated/reconciled.
    [[nodiscard]] virtual bool focus_scope_active() const noexcept { return false; }

    /// Whether focus traversal is constrained to this active scope.
    ///
    /// This flag is consulted only for components that report `is_focus_scope()`.
    [[nodiscard]] virtual bool focus_scope_traps() const noexcept { return false; }

    /// Preferred focusable-descendant index when an active scope needs initial focus.
    ///
    /// The index is advisory; runtime eligibility/availability still determines
    /// the actual target and invalid/out-of-range candidates fall back to normal
    /// scope traversal.
    [[nodiscard]] virtual std::size_t focus_scope_default_index() const noexcept { return 0; }

    /// Return this node's platform-neutral semantic contribution as an owned value.
    ///
    /// The default `None` role flattens this component while preserving semantic
    /// descendants. Text contained by the returned SemanticInfo is copied/owned by
    /// that value; callers need not keep temporary source strings alive. Snapshot
    /// construction may allocate and exceptions propagate to the operation asking
    /// for semantics. This hook runs in the owning UI domain, not on an audio thread.
    [[nodiscard]] virtual SemanticInfo semantics() const { return {}; }

    /// Return dimensionless main-axis flex weights consumed by flex-aware parents.
    ///
    /// The default opts out of both growth and shrink redistribution. The value is
    /// an owned snapshot; returning it does not invalidate layout. Components whose
    /// factors change after mount must request layout invalidation explicitly.
    [[nodiscard]] virtual FlexFactors flex_factors() const noexcept { return {}; }

    /// Return whether descendant paint and hit testing are clipped to this node.
    ///
    /// The component's own paint still receives the inherited ancestor clip.
    /// This is retained layout/input metadata; changing it after mount requires
    /// appropriate layout/paint/interaction invalidation by the component.
    [[nodiscard]] virtual bool clips_children() const noexcept { return false; }

    /// Return conservative logical paint overflow outside retained layout bounds.
    ///
    /// Each VisualOutset edge is interpreted in logical pixels and is used only
    /// for damage/culling. It does not enlarge layout bounds, focus geometry or hit
    /// testing. Under-reporting can cause stale pixels; over-reporting is safe but
    /// may increase repaint work. Dynamic changes require paint invalidation.
    [[nodiscard]] virtual VisualOutset visual_outset() const noexcept { return {}; }

    /// Compute intrinsic preferred size from borrowed child metrics.
    ///
    /// `children` is ordered exactly like retained children and is valid only for
    /// this call. Returned width/height are logical pixels and are owned values.
    /// This hook must not publish viewport geometry or retain child references.
    /// Ordinary C++ exceptions propagate through the current measurement operation.
    [[nodiscard]] virtual Size measure(const std::vector<ChildMetrics>& children) const = 0;

    /// Compute intrinsic minimum size from borrowed child metrics.
    ///
    /// The vector has the same ordering/lifetime as `measure()`. Returned values
    /// use logical pixels. The default is zero minimum size on both axes; parent
    /// constraints are applied later by `measure_constrained()`.
    [[nodiscard]] virtual Size minimum_size(const std::vector<ChildMetrics>&) const {
        return {};
    }

    /// Derive constraints for one retained child before recursively measuring it.
    ///
    /// The Constraints argument is borrowed for the call; the two size_t values
    /// identify the child index and total child count in retained order. The
    /// returned Constraints value is owned. The default removes parent minima
    /// while preserving finite/infinite maxima via `Constraints::loosen()`.
    [[nodiscard]] virtual Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const {
        return constraints.loosen();
    }

    /// Perform constraint-aware measurement and return owned child metrics.
    ///
    /// `constraints` and `children` are callback-scoped borrows. The default
    /// constrains the intrinsic minimum, raises preferred size to at least that
    /// minimum, then constrains preferred size to the supplied bounds. Specialized
    /// components may override when intrinsic size itself depends on constraints.
    /// Measurement is synchronous UI-domain work, may allocate/throw through
    /// overrides it calls, and does not itself schedule layout or paint.
    [[nodiscard]] virtual ChildMetrics measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const {
        auto minimum = constraints.constrain(minimum_size(children));
        auto preferred = measure(children);
        preferred.w = std::max(preferred.w, minimum.w);
        preferred.h = std::max(preferred.h, minimum.h);
        preferred = constraints.constrain(preferred);
        return ChildMetrics{minimum, preferred};
    }

    /// Write logical placements for retained children after measurement.
    ///
    /// The Rect is this node's assigned logical bounds. Child metrics are borrowed
    /// in retained order; `placements` is caller-owned mutable output whose
    /// entries correspond by index to those children. Implementations should fill
    /// the placements required by their layout policy without retaining either
    /// container. The default performs no writes. Exceptions abort the current
    /// layout transaction and propagate to the owning Tree/UI operation.
    virtual void layout_children(
        Rect,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>&) const {}

    /// Observe initial attachment to a compiled retained node.
    ///
    /// Called exactly once per Component instance after its node identity exists,
    /// parent before child. The MountContext is borrowed only for this call, but
    /// its returned invalidator functions are intentionally copyable for later
    /// UI-domain use. Mount may allocate/re-enter application code through user
    /// overrides; exceptions propagate through retained compilation/reconciliation.
    virtual void mount(MountContext&) {}

    /// Observe transition of the owning tree into the active state.
    ///
    /// Activation traverses parent before child and occurs before keyboard focus is
    /// applied. The LifecycleContext is callback-scoped. A Component may activate
    /// more than once over its mounted lifetime as the tree toggles active state.
    virtual void activate(LifecycleContext&) {}

    /// Observe transition of the owning tree out of the active state.
    ///
    /// Focus is removed first, then components deactivate child-to-parent in
    /// reverse sibling order. The callback-scoped context must not be retained.
    /// Later reactivation is permitted while the component remains mounted.
    virtual void deactivate(LifecycleContext&) {}

    /// Observe final detachment before this runtime node is destroyed.
    ///
    /// Called at most once after mount, child-to-parent in reverse sibling order.
    /// Stored NodeId values become stale after detachment; long-lived invalidators
    /// obtained from MountContext must therefore tolerate becoming inert.
    virtual void unmount(LifecycleContext&) {}

    /// Observe keyboard-focus gain or loss for this component.
    ///
    /// The bool is true on gain and false on loss. FocusContext is a callback-
    /// scoped borrow providing logical bounds, text-input services and invalidation.
    /// The hook can run during focus repair triggered by re-entrant UI work; do not
    /// retain the context or assume surrounding retained objects survive callbacks.
    virtual void focus_changed(bool, FocusContext&) {}

    /// Observe focus moving to a retained descendant.
    ///
    /// The Rect is the focused descendant's current bounds expressed in this
    /// component's local logical coordinates. The hook runs on ancestors only and
    /// is not repeated when focus is merely refreshed on the same node. The value
    /// is an owned snapshot and may be kept if application code needs the geometry.
    virtual void descendant_focus_changed(Rect) {}

    /// Handle one targeted borrowed input event with callback-scoped services.
    ///
    /// Input first targets one retained leaf and bubbles through ancestors while
    /// handlers return `Ignored`; returning `Handled` stops that propagation.
    /// InputEvent and InputContext must not escape the call. Handlers may capture
    /// the current pointer, invalidate retained state and invoke platform services;
    /// those operations may re-enter application code or throw. Tree dispatch
    /// restores its bookkeeping before propagating callback exceptions.
    virtual EventResult input(const InputEvent&, InputContext&) {
        return EventResult::Ignored;
    }

    /// Paint this component synchronously in retained logical coordinates.
    ///
    /// PaintContext, its Painter reference and PlatformServices are borrowed only
    /// for the callback. Drawing is clipped/translated by the owning Tree and may
    /// allocate or throw through text/resource/platform work. Implementations must
    /// not retain callback borrows, perform audio-thread work, or directly mutate
    /// private retained Node geometry from paint.
    virtual void paint(PaintContext&) const = 0;

private:
    friend class Tree;
    /// Internal availability transition hook used by T038 style-aware widgets.
    /// Most components have geometry that is independent from Enabled/ReadOnly
    /// state and therefore keep the default paint-only invalidation behavior.
    [[nodiscard]] virtual bool availability_change_affects_layout(
        const ComponentAvailability&,
        const ComponentAvailability&) const {
        return false;
    }
    void set_effective_availability(ComponentAvailability value) noexcept {
        effective_availability_ = value;
    }

    ComponentAvailability effective_availability_{};
};

/// Retained runtime node owned by the tree.
///
/// Application composition normally uses `Spec`/builders. `parent` is
/// non-owning; `component` and `children` are owned. Geometry/cache fields are
/// tree-maintained UI-domain state and are not independently thread-safe.
struct Node {
    /// Retained identity, or the invalid sentinel before publication.
    NodeId id{kInvalidNodeId};
    /// Borrowed parent pointer; null for a retained root.
    Node* parent{};
    /// Owned component instance materialized from a `Spec` factory.
    std::unique_ptr<Component> component;
    /// Owned child nodes in retained composition order.
    std::vector<std::unique_ptr<Node>> children;
    /// Current logical layout bounds.
    Rect bounds{};
    /// Last logical visual bounds published for paint invalidation.
    Rect published_visual_bounds{};
    /// Whether published visual bounds currently contain a valid snapshot.
    bool visual_bounds_published{};
    /// Whether retained layout must be recomputed.
    bool layout_dirty{true};
    /// Cached resolved activity for focus-scope routing.
    bool focus_scope_active_cached{};
    /// Non-owning retained identity used for focus restoration, or invalid.
    NodeId focus_restore{kInvalidNodeId};
};

/// Owned declarative recipe for one retained component subtree.
///
/// The factory transfers ownership of a new component instance when the tree
/// materializes the node. Child specs are owned recursively. Captured external
/// references/pointers keep ordinary C++ lifetime obligations; storing the
/// specification does not extend those referenced lifetimes.
struct Spec {
    /// Factory invoked by retained materialization to create the component.
    std::function<std::unique_ptr<Component>()> factory;
    /// Owned child specifications in retained composition order.
    std::vector<Spec> children;
};

/// Convert a builder/value exposing `spec()` into an owned `Spec`.
///
/// The argument is perfectly forwarded. Allocations/exceptions from `spec()`
/// propagate unchanged; this helper adds no fallback or synchronization.
template <class T>
Spec make_spec(T&& value) {
    return std::forward<T>(value).spec();
}

} // namespace ui
