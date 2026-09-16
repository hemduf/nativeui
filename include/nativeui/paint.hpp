#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/paint_style.hpp>
#include <nativeui/path.hpp>
#include <nativeui/text.hpp>

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

class SkCanvas;

namespace ui {

namespace detail {

// Transitional type-erased owner used by the private Skia text backend. It
// keeps backend ownership out of normal installed header dependencies while
// preserving resolved fallback-face lifetime across measure/paint.
class TypefaceHandle {
public:
    TypefaceHandle() noexcept = default;

    template <class Ptr>
        requires requires(const Ptr& value) { value.get(); }
    TypefaceHandle(const Ptr& owner) noexcept {
        using Element = std::remove_pointer_t<decltype(owner.get())>;
        ptr_ = owner.get();
        if (!ptr_) return;
        retain_ = [](void* value) noexcept { static_cast<Element*>(value)->ref(); };
        release_ = [](void* value) noexcept { static_cast<Element*>(value)->unref(); };
        retain_(ptr_);
    }

    TypefaceHandle(const TypefaceHandle& other) noexcept
        : ptr_(other.ptr_), retain_(other.retain_), release_(other.release_) {
        if (ptr_ && retain_) retain_(ptr_);
    }

    TypefaceHandle(TypefaceHandle&& other) noexcept
        : ptr_(std::exchange(other.ptr_, nullptr)),
          retain_(std::exchange(other.retain_, nullptr)),
          release_(std::exchange(other.release_, nullptr)) {}

    TypefaceHandle& operator=(const TypefaceHandle& other) noexcept {
        if (this == &other) return *this;
        reset();
        ptr_ = other.ptr_;
        retain_ = other.retain_;
        release_ = other.release_;
        if (ptr_ && retain_) retain_(ptr_);
        return *this;
    }

    TypefaceHandle& operator=(TypefaceHandle&& other) noexcept {
        if (this == &other) return *this;
        reset();
        ptr_ = std::exchange(other.ptr_, nullptr);
        retain_ = std::exchange(other.retain_, nullptr);
        release_ = std::exchange(other.release_, nullptr);
        return *this;
    }

    ~TypefaceHandle() { reset(); }

    [[nodiscard]] void* get() const noexcept { return ptr_; }

private:
    void reset() noexcept {
        if (ptr_ && release_) release_(ptr_);
        ptr_ = nullptr;
        retain_ = nullptr;
        release_ = nullptr;
    }

    void* ptr_{};
    void (*retain_)(void*) noexcept{};
    void (*release_)(void*) noexcept{};
};

struct ResolvedTextRun {
    std::size_t byte_offset{};
    std::size_t byte_count{};
    TypefaceHandle typeface;
    float width{};
    bool synthetic_bold{};
};

struct ResolvedTextLayout {
    TextMetrics metrics;
    std::vector<ResolvedTextRun> runs;
    // Only populated for malformed input. Run offsets then refer to these
    // owned bytes, so measurement and painting never pass invalid UTF-8 to the
    // private renderer backend.
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
public:
    class StateGuard {
    public:
        explicit StateGuard(Painter& painter) noexcept
            : painter_(&painter), previous_floor_(painter.restore_floor_) {
            painter_->save();
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

    // T069 phase 1: public headers no longer include Skia. These transitional
    // backend-construction entry points are removed in the final freeze batch.
    explicit Painter(SkCanvas& canvas) noexcept : canvas_(canvas) {}
    ~Painter() noexcept;

    [[nodiscard]] SkCanvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] StateGuard scoped_state() noexcept { return StateGuard{*this}; }
    [[nodiscard]] int save_depth() const noexcept { return save_depth_; }

    void save();
    void restore();

    void translate(float x, float y);
    void translate(Point offset) { translate(offset.x, offset.y); }
    void scale(float x, float y);
    void scale(float uniform) { scale(uniform, uniform); }
    void rotate(float radians);
    void concat(const Transform2D& transform);

    void fill_rounded_rect(Rect rect, float radius, Color color);
    void fill_rounded_rect(Rect rect, float radius, const LinearGradient& gradient,
                           PaintOptions options = {});
    void fill_rounded_rect(Rect rect, float radius, const RadialGradient& gradient,
                           PaintOptions options = {});
    void stroke_rounded_rect(Rect rect, float radius, float width, Color color);
    void circle(Point center, float radius, Color color);
    void arc(Point center, float radius, float start, float end, float width, Color color);
    void line(Point a, Point b, float width, Color color);
    void fill_path(const Path& path, Color color);
    void stroke_path(const Path& path, Color color, StrokeStyle style = {});
    void text(Point position, std::string_view text, const TextStyle& style);
    void text(Point position, std::string_view text, float size, Color color,
              TextAlign align = TextAlign::Left);
    void push_clip(Rect rect);
    void pop_clip();

    [[nodiscard]] static float measure_text(std::string_view text, float size) {
        return TextService::measure(text, size).width;
    }

private:
    void restore_unchecked();

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
