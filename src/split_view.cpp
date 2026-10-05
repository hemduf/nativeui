#include <nativeui/split_view.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ui {
namespace detail {
struct SplitGeometry {
    Rect bounds{};
    double lower{};
    double upper{};
    double first{};
    double second{};
    float line{};
    float grip{};
};
struct SplitViewState {
    explicit SplitViewState(Binding<double> value) : source(std::move(value)) {}
    Binding<double> source;
    SplitOrientation orientation{SplitOrientation::Horizontal};
    double minimum_first{40.0};
    double minimum_second{40.0};
    double step{10.0};
    SplitViewStyle style;
    std::function<void(double)> on_change;
    std::function<void(double)> on_commit;
    Binding<double>::Subscription subscription;
    std::function<void()> invalidate_layout;
    std::function<void()> release_pointer;
    SplitGeometry geometry;
    std::uint64_t generation{};
    double origin_value{};
    double origin_effective{};
    double origin_pointer{};
    double expected{};
    bool mounted{};
    bool dragging{};
    bool modified{};
    bool writing{};
};
} // namespace detail
namespace {
float logical_extent(double value) noexcept {
    return static_cast<float>(std::clamp(value,0.0,
        static_cast<double>(std::numeric_limits<float>::max())));
}
float logical_coordinate(double value) noexcept {
    if (!std::isfinite(value)) return 0.0f;
    return static_cast<float>(std::clamp(value,
        -static_cast<double>(std::numeric_limits<float>::max()),
        static_cast<double>(std::numeric_limits<float>::max())));
}
void validate_nonnegative(double value, const char* message) {
    if (!std::isfinite(value) || value < 0.0) throw std::invalid_argument(message);
}
void validate_style(const SplitViewStyle& style) {
    validate_nonnegative(style.line_thickness,"SplitView line thickness must be finite and nonnegative");
    validate_nonnegative(style.hit_grip,"SplitView hit grip must be finite and nonnegative");
}
void validate_orientation(SplitOrientation orientation) {
    if (orientation != SplitOrientation::Horizontal && orientation != SplitOrientation::Vertical)
        throw std::invalid_argument("SplitView orientation is invalid");
}
bool horizontal(const detail::SplitViewState& state) noexcept {
    return state.orientation == SplitOrientation::Horizontal;
}
double coordinate(const InputEvent& event, const detail::SplitViewState& state) noexcept {
    return horizontal(state) ? event.position.x : event.position.y;
}
detail::SplitGeometry geometry_for(Rect bounds, const detail::SplitViewState& state) noexcept {
    const auto axis = logical_extent(horizontal(state) ? bounds.w : bounds.h);
    const auto line = std::min(axis,logical_extent(state.style.line_thickness));
    const double available = static_cast<double>(axis) - line;
    double lower = state.minimum_first;
    double upper = available - state.minimum_second;
    if (lower > upper) {
        const auto scale = std::max(state.minimum_first,state.minimum_second);
        const auto first_weight = scale > 0.0 ? state.minimum_first / scale : 0.0;
        const auto second_weight = scale > 0.0 ? state.minimum_second / scale : 0.0;
        lower = upper = first_weight + second_weight > 0.0 ?
            available * first_weight / (first_weight + second_weight) : 0.0;
    }
    const auto requested = state.source.get();
    const auto first = std::clamp(std::isfinite(requested) ? requested : lower,lower,upper);
    return {bounds,lower,upper,first,available-first,line,
            std::min(axis,std::max(line,logical_extent(state.style.hit_grip)))};
}
std::uint64_t stop_drag(const std::shared_ptr<detail::SplitViewState>& state) {
    const auto generation = ++state->generation;
    state->dragging = false;
    state->modified = false;
    auto release = std::exchange(state->release_pointer,{});
    if (release) release();
    return generation;
}
void stop_drag_noexcept(const std::shared_ptr<detail::SplitViewState>& state) noexcept {
    try { stop_drag(state); } catch (...) {}
}

bool publish(const std::shared_ptr<detail::SplitViewState>& state, double next, double* published = nullptr) {
    if (!state->mounted || !state->source.valid() || !std::isfinite(next)) return false;
    const auto previous = state->source.get();
    if (previous == next) return false;
    const auto generation = state->generation;
    struct WritingScope {
        std::shared_ptr<detail::SplitViewState> state;
        bool before;
        double expected;
        ~WritingScope() { state->writing = before; state->expected = expected; }
    } guard{state,state->writing,state->expected};
    state->writing = true;
    state->expected = next;
    try {
        // The source may commit and an earlier application observer may throw
        // before our subscription runs. Keep layout recoverable before writing.
        if (state->invalidate_layout) state->invalidate_layout();
        if (!state->mounted || state->generation != generation) return false;
        if (!state->source.valid()) { stop_drag_noexcept(state); return false; }
        state->source.set(next);
        if (!state->source.valid()) {
            stop_drag_noexcept(state);
            return false;
        }
        const auto accepted = state->source.get();
        const bool changed = accepted != previous;
        if (published) *published = accepted;
        if (changed && state->generation == generation && state->dragging) state->modified = true;
        if (changed && state->mounted && state->generation == generation) {
            auto callback = state->on_change;
            if (!state->source.valid()) {
                stop_drag_noexcept(state);
                return false;
            }
            if (callback && state->mounted && state->generation == generation) callback(accepted);
            if (!state->source.valid()) { stop_drag_noexcept(state); return false; }
        }
        return changed;
    } catch (...) {
        if (state->generation == generation || !state->source.valid()) stop_drag_noexcept(state);
        throw;
    }
}

class SplitPaneComponent final : public Component {
public:
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }
    [[nodiscard]] bool clips_children() const noexcept override { return true; }
    void layout_children(Rect bounds, const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override {
        if (!children.empty()) placements.front().bounds = bounds;
    }
    void paint(PaintContext&) const override {}
};

class SplitHandleComponent final : public Component, public detail::ThemeBinding {
public:
    explicit SplitHandleComponent(std::shared_ptr<detail::SplitViewState> state)
        : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override { return {}; }
    void focus_changed(bool focused, FocusContext& context) override {
        focused_ = focused;
        context.invalidate();
    }
    void deactivate(LifecycleContext&) override {
        focused_ = false;
        hovered_ = false;
        stop_drag_noexcept(state_);
    }
    void unmount(LifecycleContext&) override { stop_drag_noexcept(state_); }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = SemanticRole::Custom;
        info.name = state_->style.accessible_name;
        info.focusable = true;
        info.focused = focused_;
        info.enabled = effective_enabled();
        info.read_only = effective_read_only() || !state_->source.valid();
        info.numeric_value = state_->geometry.first;
        info.value_range = SemanticValueRange{state_->geometry.lower,state_->geometry.upper,state_->step};
        if (effective_enabled()) info.actions.push_back(SemanticAction::Focus);
        if (effective_enabled() && !info.read_only) {
            info.actions.push_back(SemanticAction::Increment);
            info.actions.push_back(SemanticAction::Decrement);
            info.actions.push_back(SemanticAction::SetValue);
        }
        return info;
    }
    EventResult input(const InputEvent& event, InputContext& context) override {
        const auto state = state_;
        const bool editable = effective_enabled() && !effective_read_only() && state->source.valid();
        if (event.type == InputType::PointerCancel) {
            if (!state->dragging) return EventResult::Ignored;
            const auto origin = state->origin_value;
            const bool rollback = event.cancel_reason == PointerCancelReason::Native && editable;
            const auto generation = stop_drag(state);
            if (rollback && state->generation == generation && std::isfinite(origin))
                (void)publish(state,origin);
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyDown && event.key == Key::Escape && state->dragging) {
            const auto origin = state->origin_value;
            const auto generation = stop_drag(state);
            if (editable && state->generation == generation && std::isfinite(origin)) (void)publish(state,origin);
            return EventResult::Handled;
        }
        if (!editable) {
            if (state->dragging) stop_drag(state);
            return EventResult::Ignored;
        }
        if (event.type == InputType::PointerDown) {
            auto release = context.pointer_releaser();
            ++state->generation;
            state->origin_pointer = coordinate(event,*state);
            state->origin_value = std::isfinite(state->source.get()) ? state->source.get() : state->geometry.first;
            state->origin_effective = state->geometry.first;
            state->dragging = true;
            state->modified = false;
            state->release_pointer = std::move(release);
            const auto generation = state->generation;
            try {
                context.capture_pointer();
                context.invalidate();
            } catch (...) {
                if (state->generation == generation) stop_drag_noexcept(state);
                throw;
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerMove) {
            if (state->dragging) {
                const auto next = state->origin_effective + coordinate(event,*state) - state->origin_pointer;
                if (std::isfinite(next))
                    (void)publish(state,std::clamp(next,state->geometry.lower,state->geometry.upper));
                return EventResult::Handled;
            }
            const bool hovered = context.bounds().contains(event.position);
            if (hovered_ != hovered) { hovered_ = hovered; context.invalidate(); }
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerLeave) {
            if (hovered_) { hovered_ = false; context.invalidate(); }
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerUp && state->dragging) {
            const bool modified = state->modified;
            const auto value = state->source.get();
            const auto generation = stop_drag(state);
            if (modified && state->mounted && state->generation == generation && state->source.valid()) {
                auto callback = state->on_commit;
                if (callback && state->mounted && state->generation == generation && state->source.valid()) callback(value);
            }
            return EventResult::Handled;
        }
        if (event.type != InputType::KeyDown) return EventResult::Ignored;
        // Keyboard edits start from the authoritative model even when several
        // keys arrive before painting/layout. Bounds come from committed geometry.
        const auto current_geometry = geometry_for(state->geometry.bounds,*state);
        double candidate = current_geometry.first;
        bool recognized = true;
        const bool increasing = horizontal(*state) ? event.key == Key::Right : event.key == Key::Down;
        const bool decreasing = horizontal(*state) ? event.key == Key::Left : event.key == Key::Up;
        if (event.key == Key::Home) candidate = current_geometry.lower;
        else if (event.key == Key::End) candidate = current_geometry.upper;
        else if (increasing || decreasing) {
            const auto step = state->step * (event.shift && state->step <=
                std::numeric_limits<double>::max() / 10.0 ? 10.0 : 1.0);
            const auto range = current_geometry.upper - current_geometry.lower;
            candidate += (increasing ? 1.0 : -1.0) * std::min(step,range);
            candidate = std::clamp(candidate,current_geometry.lower,current_geometry.upper);
        } else recognized = false;
        if (!recognized) return EventResult::Ignored;
        const auto generation = state->generation;
        double accepted = candidate;
        const bool changed = publish(state,candidate,&accepted);
        if (changed && state->mounted && state->generation == generation && !state->dragging &&
            state->source.valid() && state->source.get() == accepted) {
            auto callback = state->on_commit;
            if (callback && state->mounted && state->generation == generation && !state->dragging &&
                state->source.valid() && state->source.get() == accepted) callback(accepted);
        }
        return EventResult::Handled;
    }
    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto& palette = current_theme().palette;
        auto color = state_->style.color.value_or(palette.border);
        if (hovered_) color = state_->style.hover_color.value_or(palette.accent);
        if (state_->dragging) color = state_->style.drag_color.value_or(palette.accent);
        if (!effective_enabled()) color = palette.disabled;
        const auto line = state_->geometry.line;
        auto& painter = context.painter();
        if (horizontal(*state_)) {
            const auto x = bounds.x + (bounds.w - line) * 0.5f;
            painter.fill_rounded_rect({x,bounds.y,line,bounds.h},0.0f,color);
        } else {
            const auto y = bounds.y + (bounds.h - line) * 0.5f;
            painter.fill_rounded_rect({bounds.x,y,bounds.w,line},0.0f,color);
        }
        if (context.focused()) painter.stroke_rounded_rect(
            {bounds.x+0.5f,bounds.y+0.5f,std::max(0.0f,bounds.w-1.0f),std::max(0.0f,bounds.h-1.0f)},
            2.0f,1.0f,state_->style.focus_color.value_or(palette.focus));
    }
private:
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability& next) noexcept override {
        if (!next.interactive() || next.read_only) stop_drag_noexcept(state_);
    }
    std::shared_ptr<detail::SplitViewState> state_;
    bool hovered_{};
    bool focused_{};
};

} // namespace

SplitViewComponent::SplitViewComponent(
    Binding<double> first_extent, std::shared_ptr<const Spec> first, std::shared_ptr<const Spec> second,
    SplitOrientation orientation, double minimum_first, double minimum_second, double step,
    std::function<void(double)> on_change, std::function<void(double)> on_commit, SplitViewStyle style)
    : state_(std::make_shared<detail::SplitViewState>(std::move(first_extent))), first_(std::move(first)), second_(std::move(second)) {
    if (!first_ || !second_) throw std::invalid_argument("SplitView requires two child specifications");
    validate_orientation(orientation);
    validate_nonnegative(minimum_first,"SplitView first minimum must be finite and nonnegative");
    validate_nonnegative(minimum_second,"SplitView second minimum must be finite and nonnegative");
    if (!std::isfinite(step) || step <= 0.0) throw std::invalid_argument("SplitView step must be finite and positive");
    validate_style(style);
    state_->orientation = orientation;
    state_->minimum_first = minimum_first;
    state_->minimum_second = minimum_second;
    state_->step = step;
    state_->on_change = std::move(on_change);
    state_->on_commit = std::move(on_commit);
    state_->style = std::move(style);
}
std::vector<Spec> SplitViewComponent::compile_children() const {
    const auto state = state_;
    return {
        Spec{[] { return std::make_unique<SplitPaneComponent>(); },{*first_}},
        Spec{[state] { return std::make_unique<SplitHandleComponent>(state); },{}},
        Spec{[] { return std::make_unique<SplitPaneComponent>(); },{*second_}}};
}
std::optional<std::size_t> SplitViewComponent::foreground_child_index() const noexcept { return 1; }
Size SplitViewComponent::measure(const std::vector<ChildMetrics>& children) const {
    if (children.size() < 3) return {};
    const auto a = children[0].preferred;
    const auto b = children[2].preferred;
    const auto line = state_->style.line_thickness;
    return horizontal(*state_) ? Size{logical_extent(static_cast<double>(a.w)+b.w+line),std::max(a.h,b.h)} :
        Size{std::max(a.w,b.w),logical_extent(static_cast<double>(a.h)+b.h+line)};
}
Size SplitViewComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    const auto cross = children.size() >= 3 ? (horizontal(*state_) ?
        std::max(children[0].minimum.h,children[2].minimum.h) :
        std::max(children[0].minimum.w,children[2].minimum.w)) : 0.0f;
    const auto along = logical_extent(state_->minimum_first+state_->minimum_second+state_->style.line_thickness);
    return horizontal(*state_) ? Size{along,cross} : Size{cross,along};
}
Constraints SplitViewComponent::child_constraints(const Constraints& constraints,std::size_t,std::size_t) const {
    return horizontal(*state_) ? Constraints::loose({kUnboundedExtent,constraints.max.h}) :
                                Constraints::loose({constraints.max.w,kUnboundedExtent});
}
void SplitViewComponent::layout_children(Rect bounds,const std::vector<ChildMetrics>& children,
                                        std::vector<ChildPlacement>& placements) const {
    if (children.size() < 3) return;
    auto candidate = std::make_shared<const detail::SplitGeometry>(geometry_for(bounds,*state_));
    const auto& geometry = *candidate;
    const auto width = logical_extent(bounds.w);
    const auto height = logical_extent(bounds.h);
    if (horizontal(*state_)) {
        placements[0].bounds = {bounds.x,bounds.y,logical_extent(geometry.first),height};
        placements[1].bounds = {logical_coordinate(bounds.x+geometry.first+geometry.line*0.5-geometry.grip*0.5),
                                bounds.y,geometry.grip,height};
        placements[2].bounds = {logical_coordinate(bounds.x+geometry.first+geometry.line),bounds.y,
                                logical_extent(geometry.second),height};
    } else {
        placements[0].bounds = {bounds.x,bounds.y,width,logical_extent(geometry.first)};
        placements[1].bounds = {bounds.x,logical_coordinate(bounds.y+geometry.first+geometry.line*0.5-geometry.grip*0.5),
                                width,geometry.grip};
        placements[2].bounds = {bounds.x,logical_coordinate(bounds.y+geometry.first+geometry.line),width,
                                logical_extent(geometry.second)};
    }
    candidate_geometry_ = std::move(candidate);
}
void SplitViewComponent::layout_committed(Rect,Rect) noexcept {
    if (candidate_geometry_) state_->geometry = *candidate_geometry_;
}
void SplitViewComponent::paint(PaintContext&) const {}
void SplitViewComponent::mount(MountContext& context) {
    state_->mounted = true;
    state_->invalidate_layout = context.layout_invalidator();
    const std::weak_ptr<detail::SplitViewState> weak = state_;
    state_->subscription = state_->source.observe([weak](const double& value) {
        const auto state = weak.lock();
        if (!state || !state->mounted) return;
        if ((!state->writing || state->expected != value) && state->dragging) stop_drag(state);
        if (state->invalidate_layout) state->invalidate_layout();
    });
}
void SplitViewComponent::unmount(LifecycleContext&) {
    state_->mounted = false;
    state_->subscription.reset();
    stop_drag_noexcept(state_);
    state_->invalidate_layout = {};
}
SplitView&& SplitView::orientation(SplitOrientation value) && { validate_orientation(value); orientation_=value; return std::move(*this); }
SplitView&& SplitView::minimum_panes(double first,double second) && {
    validate_nonnegative(first,"SplitView first minimum must be finite and nonnegative");
    validate_nonnegative(second,"SplitView second minimum must be finite and nonnegative");
    minimum_first_=first; minimum_second_=second; return std::move(*this);
}
SplitView&& SplitView::step(double value) && {
    if (!std::isfinite(value) || value<=0.0) throw std::invalid_argument("SplitView step must be finite and positive");
    step_=value; return std::move(*this);
}
SplitView&& SplitView::on_change(std::function<void(double)> callback) && { on_change_=std::move(callback); return std::move(*this); }
SplitView&& SplitView::on_commit(std::function<void(double)> callback) && { on_commit_=std::move(callback); return std::move(*this); }
SplitView&& SplitView::style(SplitViewStyle value) && { validate_style(value); style_=std::move(value); return std::move(*this); }
Spec SplitView::spec() && {
    auto first = std::make_shared<const Spec>(std::move(first_));
    auto second = std::make_shared<const Spec>(std::move(second_));
    return {[source=first_extent_,first,second,orientation=orientation_,a=minimum_first_,b=minimum_second_,
             step=step_,change=std::move(on_change_),commit=std::move(on_commit_),style=std::move(style_)] {
                return std::make_unique<SplitViewComponent>(source,first,second,orientation,a,b,step,change,commit,style);
            },{},[](Component& component) {
                return static_cast<SplitViewComponent&>(component).compile_children();
            }};
}
} // namespace ui
