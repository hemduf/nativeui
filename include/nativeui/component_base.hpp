#pragma once

#include <nativeui/constraints.hpp>
#include <nativeui/input.hpp>
#include <nativeui/invalidation.hpp>
#include <nativeui/paint.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

struct FlexFactors {
    float grow{};
    float shrink{};
};

struct ChildMetrics {
    Size minimum{};
    Size preferred{};
    FlexFactors flex{};

    ChildMetrics() = default;
    explicit ChildMetrics(Size preferred_size) : preferred(preferred_size) {}
    ChildMetrics(Size minimum_size, Size preferred_size)
        : minimum(minimum_size), preferred(preferred_size) {}
    ChildMetrics(Size minimum_size, Size preferred_size, FlexFactors flex_factors)
        : minimum(minimum_size), preferred(preferred_size), flex(flex_factors) {}
};

struct ChildPlacement {
    Rect bounds;
};

using NodeId = std::uint64_t;
inline constexpr NodeId kInvalidNodeId = 0;

class PaintContext {
public:
    PaintContext(Painter& painter, Rect bounds, bool focused, PlatformServices& platform)
        : PaintContext(painter, bounds, bounds, focused, platform) {}

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

    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }
    [[nodiscard]] Rect clip_bounds() const noexcept { return clip_bounds_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }
    [[nodiscard]] Painter& painter() noexcept { return painter_; }
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return platform_.text_metrics(text, style);
    }
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

class CanvasContext2D {
public:
    CanvasContext2D(Painter& painter, Rect bounds, bool focused)
        : painter_(painter), bounds_(bounds), focused_(focused) {}

    [[nodiscard]] Size size() const noexcept { return Size{bounds_.w, bounds_.h}; }
    [[nodiscard]] float width() const noexcept { return bounds_.w; }
    [[nodiscard]] float height() const noexcept { return bounds_.h; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }

    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return TextService::measure(text, style);
    }
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return TextService::measure(text, size).width;
    }

    void fill_rect(Rect rect, Color color) {
        painter_.fill_rounded_rect(rect, 0.0f, color);
    }

    void stroke_rect(Rect rect, float width, Color color) {
        painter_.stroke_rounded_rect(rect, 0.0f, width, color);
    }

    void fill_rounded_rect(Rect rect, float radius, Color color) {
        painter_.fill_rounded_rect(rect, radius, color);
    }

    void stroke_rounded_rect(Rect rect, float radius, float width, Color color) {
        painter_.stroke_rounded_rect(rect, radius, width, color);
    }

    void circle(Point center, float radius, Color color) {
        painter_.circle(center, radius, color);
    }

    void arc(Point center, float radius, float start, float end, float width, Color color) {
        painter_.arc(center, radius, start, end, width, color);
    }

    void line(Point a, Point b, float width, Color color) {
        painter_.line(a, b, width, color);
    }

    void text(Point position, std::string_view text, const TextStyle& style) {
        painter_.text(position, text, style);
    }

    void text(Point position, std::string_view text, float size, Color color,
              TextAlign align = TextAlign::Left) {
        painter_.text(position, text, size, color, align);
    }

    void push_clip(Rect rect) { painter_.push_clip(rect); }
    void pop_clip() { painter_.pop_clip(); }

    void save() { painter_.save(); }
    void restore() { painter_.restore(); }
    void translate(float x, float y) { painter_.translate(x, y); }
    void translate(Point offset) { painter_.translate(offset); }
    void scale(float x, float y) { painter_.scale(x, y); }
    void scale(float uniform) { painter_.scale(uniform); }
    void rotate(float radians) { painter_.rotate(radians); }
    void concat(const Transform2D& transform) { painter_.concat(transform); }

private:
    Painter& painter_;
    Rect bounds_{};
    bool focused_{};
};

class InputContext {
public:
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
          capture_(std::move(capture)),
          release_(std::move(release)) {}

    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return platform_.text_metrics(text, style);
    }
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return platform_.text_width(text, size);
    }
    void set_clipboard_text(std::string_view text) { platform_.set_clipboard_text(text); }
    void request_clipboard_text() { platform_.request_clipboard_text(); }
    [[nodiscard]] bool accept_drop(std::string_view type) {
        return platform_.accept_drop(type, bounds_);
    }
    void reject_drop() { platform_.reject_drop(bounds_); }
    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) {
        platform_.set_text_input(active, area, cursor_offset);
    }
    /// Repaint this component without recomputing layout.
    void invalidate() const { invalidate_(); }
    /// Recompute layout from this component through its ancestors, then repaint.
    void invalidate_layout() const { invalidate_layout_(); }
    void capture_pointer() const { capture_(); }
    void release_pointer() const { release_(); }

private:
    Rect bounds_{};
    PlatformServices& platform_;
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
    std::function<void()> capture_;
    std::function<void()> release_;
};

class CanvasInputContext {
public:
    explicit CanvasInputContext(InputContext& context) : context_(context) {}

    [[nodiscard]] Size size() const noexcept {
        const auto b = context_.bounds();
        return Size{b.w, b.h};
    }

    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return context_.text_metrics(text, style);
    }
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return context_.text_width(text, size);
    }

    void invalidate() const { context_.invalidate(); }
    void invalidate_layout() const { context_.invalidate_layout(); }
    void capture_pointer() const { context_.capture_pointer(); }
    void release_pointer() const { context_.release_pointer(); }
    void set_clipboard_text(std::string_view text) { context_.set_clipboard_text(text); }
    void request_clipboard_text() { context_.request_clipboard_text(); }
    [[nodiscard]] bool accept_drop(std::string_view type) { return context_.accept_drop(type); }
    void reject_drop() { context_.reject_drop(); }

private:
    InputContext& context_;
};

class FocusContext {
public:
    FocusContext(Rect bounds,
                 PlatformServices& platform,
                 std::function<void()> invalidate,
                 std::function<void()> invalidate_layout)
        : bounds_(bounds),
          platform_(platform),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)) {}

    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }
    [[nodiscard]] TextMetrics text_metrics(std::string_view text, const TextStyle& style) const {
        return platform_.text_metrics(text, style);
    }
    [[nodiscard]] float text_width(std::string_view text, float size) const {
        return platform_.text_width(text, size);
    }
    void set_text_input(bool active, Rect area = {}, float cursor_offset = 0.0f) {
        platform_.set_text_input(active, area, cursor_offset);
    }
    void invalidate() const { invalidate_(); }
    void invalidate_layout() const { invalidate_layout_(); }

private:
    Rect bounds_{};
    PlatformServices& platform_;
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
};

class MountContext {
public:
    MountContext(NodeId node_id,
                 std::function<void()> invalidate,
                 std::function<void()> invalidate_layout,
                 std::function<void()> invalidate_focus)
        : node_id_(node_id),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)),
          invalidate_focus_(std::move(invalidate_focus)) {}

    [[nodiscard]] NodeId node_id() const noexcept { return node_id_; }
    /// Long-lived callback for state changes that only affect painting.
    [[nodiscard]] std::function<void()> invalidator() const { return invalidate_; }
    /// Long-lived callback for state changes that can affect preferred size/layout.
    [[nodiscard]] std::function<void()> layout_invalidator() const { return invalidate_layout_; }
    /// Long-lived callback for state changes that affect focus availability/scopes.
    [[nodiscard]] std::function<void()> focus_invalidator() const { return invalidate_focus_; }

private:
    NodeId node_id_{kInvalidNodeId};
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
    std::function<void()> invalidate_focus_;
};

class LifecycleContext {
public:
    LifecycleContext(NodeId node_id,
                     Rect bounds,
                     std::function<void()> invalidate,
                     std::function<void()> invalidate_layout)
        : node_id_(node_id),
          bounds_(bounds),
          invalidate_(std::move(invalidate)),
          invalidate_layout_(std::move(invalidate_layout)) {}

    [[nodiscard]] NodeId node_id() const noexcept { return node_id_; }
    [[nodiscard]] Rect bounds() const noexcept { return bounds_; }
    void invalidate() const { invalidate_(); }
    void invalidate_layout() const { invalidate_layout_(); }

private:
    NodeId node_id_{kInvalidNodeId};
    Rect bounds_{};
    std::function<void()> invalidate_;
    std::function<void()> invalidate_layout_;
};

class Component {
public:
    virtual ~Component() = default;

    [[nodiscard]] virtual bool focusable() const noexcept { return false; }

    /// Focus-scope metadata used by the tree focus manager. Normal components
    /// are not scopes and therefore remain unaffected by scope state.
    [[nodiscard]] virtual bool is_focus_scope() const noexcept { return false; }
    [[nodiscard]] virtual bool focus_scope_active() const noexcept { return false; }
    [[nodiscard]] virtual bool focus_scope_traps() const noexcept { return false; }
    [[nodiscard]] virtual std::size_t focus_scope_default_index() const noexcept { return 0; }

    /// Optional main-axis flex factors consumed by Row/Column. Most components
    /// remain intrinsic-sized; the `Flex` layout wrapper overrides this.
    [[nodiscard]] virtual FlexFactors flex_factors() const noexcept { return {}; }

    /// Clip descendant painting and hit testing to this node's bounds. The
    /// component itself is still painted under its inherited ancestor clip.
    [[nodiscard]] virtual bool clips_children() const noexcept { return false; }

    /// Existing intrinsic preferred-size hook. Kept source-compatible for
    /// custom components while constrained measurement is layered around it.
    [[nodiscard]] virtual Size measure(const std::vector<ChildMetrics>& children) const = 0;

    /// Intrinsic minimum size before parent constraints are applied. Components
    /// may override this independently from preferred size.
    [[nodiscard]] virtual Size minimum_size(const std::vector<ChildMetrics>&) const {
        return {};
    }

    /// Constraints used when recursively measuring one child. The default
    /// removes the parent's minimum while preserving its maximum bounds.
    [[nodiscard]] virtual Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const {
        return constraints.loosen();
    }

    /// Constraint-aware measurement. Most components only override `measure`
    /// and optionally `minimum_size`; specialized components such as wrapped
    /// text can override this later when their intrinsic size depends on bounds.
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

    virtual void layout_children(
        Rect,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>&) const {}

    /// Called exactly once after the runtime node is compiled. Parent nodes are
    /// mounted before children.
    virtual void mount(MountContext&) {}

    /// Called whenever the tree becomes active. Activation traverses parent to
    /// child before keyboard focus is applied.
    virtual void activate(LifecycleContext&) {}

    /// Called when the tree becomes inactive. Focus is removed first, then
    /// components deactivate child-to-parent in reverse sibling order.
    virtual void deactivate(LifecycleContext&) {}

    /// Called once before the runtime node is destroyed. Unmount traverses
    /// child-to-parent in reverse sibling order.
    virtual void unmount(LifecycleContext&) {}

    virtual void focus_changed(bool, FocusContext&) {}

    /// Handle a targeted input event. Returning `Handled` consumes the event;
    /// returning `Ignored` leaves it unconsumed. The current tree routes to one
    /// leaf target first, then bubbles ignored input through ancestors.
    virtual EventResult input(const InputEvent&, InputContext&) {
        return EventResult::Ignored;
    }

    virtual void paint(PaintContext&) const = 0;
};

struct Node {
    NodeId id{kInvalidNodeId};
    Node* parent{};
    std::unique_ptr<Component> component;
    std::vector<std::unique_ptr<Node>> children;
    Rect bounds{};
    bool layout_dirty{true};
    bool focus_scope_active_cached{};
    Node* focus_restore{};
};

struct Spec {
    std::function<std::unique_ptr<Component>()> factory;
    std::vector<Spec> children;
};

template <class T>
Spec make_spec(T&& value) {
    return std::forward<T>(value).spec();
}

} // namespace ui
