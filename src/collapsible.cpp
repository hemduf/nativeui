#include <nativeui/collapsible.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/detail/interaction_observer.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/disclosure_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace ui::detail {
namespace {
void cancel_motion(const std::shared_ptr<DisclosureState>& state) noexcept {
    ++state->generation;
    if (state->animations) (void)state->animations->cancel(state->animation);
    state->animation = {};
}
void snap_motion(const std::shared_ptr<DisclosureState>& state) noexcept {
    cancel_motion(state);
    state->phase = state->open ? 1.0f : 0.0f;
}
bool can_animate(const DisclosureState& state) noexcept {
    return state.mounted && state.active && state.allowed && state.bounds_nonempty &&
           !state.style.reduced_motion && state.dispatcher.valid();
}
void animate_to_target(const std::shared_ptr<DisclosureState>& state) noexcept {
    cancel_motion(state);
    const auto target = state->open ? 1.0f : 0.0f;
    if (!can_animate(*state) || state->phase == target) {
        state->phase = target;
        return;
    }
    try {
        if (!state->animations) state->animations = std::make_unique<AnimationContext>(state->dispatcher);
        const auto generation = state->generation;
        const std::weak_ptr<DisclosureState> weak = state;
        state->animation = state->animations->start_tween(
            state->phase,target,DispatcherDuration{0.15},Easing::EaseInOut,
            AnimationInvalidation::Layout,state->animation_target,
            [weak,generation](float value) {
                const auto current = weak.lock();
                if (!current || current->generation != generation) return;
                current->phase = value;
            },
            [weak,generation] {
                const auto current = weak.lock();
                if (!current || current->generation != generation) return;
                current->animation = {};
                if (current->invalidate_availability) current->invalidate_availability();
            });
        if (!state->animation.valid()) state->phase = target;
    } catch (...) {
        snap_motion(state);
    }
}

class DisclosurePanel final : public Component, public DynamicChildrenSource,
                              public RetainedInteractionObserver {
public:
    DisclosurePanel(std::shared_ptr<DisclosureState> state,std::shared_ptr<const Spec> content)
        : state_(std::move(state)), content_(std::move(content)) {}
    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        return {state_->open || state_->phase > 0.0f ? VisibilityMode::Visible : VisibilityMode::Collapsed,
                state_->open && state_->enabled,false};
    }
    [[nodiscard]] bool clips_children() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }
    [[nodiscard]] Constraints child_constraints(const Constraints& constraints,std::size_t,std::size_t) const override {
        return {{},{constraints.max.w,kUnboundedExtent}};
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override {
        pending_height_ = children.empty() ? state_->cached_height : children.front().preferred.h;
        if (!placements.empty()) placements.front().bounds = {bounds.x,bounds.y,bounds.w,pending_height_};
    }
    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        return retains_content() ? std::vector<std::string>{"content"} : std::vector<std::string>{};
    }
    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        return retains_content() ? std::vector<DynamicChildSpec>{{"content",*content_}} :
                                   std::vector<DynamicChildSpec>{};
    }
    void set_structure_invalidator(std::function<void()> callback) override {
        state_->invalidate_structure = std::move(callback);
    }
    void retained_focus_within_changed(bool focused,bool,Dispatcher) override { state_->focus_within = focused; }
    void unmount(LifecycleContext&) override {
        state_->invalidate_structure = {};
        state_->focus_within = false;
    }
    void paint(PaintContext&) const override {}
private:
    [[nodiscard]] bool retains_content() const noexcept {
        return state_->policy == DisclosureContentPolicy::Retain || state_->open;
    }
    void layout_committed(Rect,Rect) noexcept override { state_->cached_height = pending_height_; }
    std::shared_ptr<DisclosureState> state_;
    std::shared_ptr<const Spec> content_;
    mutable float pending_height_{};
};

struct HeaderContact {
    std::function<void()> release_pointer;
    Key pressed_key{Key::None};
    bool pointer_pressed{};
    bool focused{};
    bool hovered{};
    bool mounted{};
    std::uint64_t serial{};
};

class DisclosureHeader final : public Component, public ThemeBinding, public FocusGroupParticipant {
public:
    explicit DisclosureHeader(std::shared_ptr<DisclosureState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool cancel_capture_on_read_only() const noexcept override { return true; }
    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        return {VisibilityMode::Visible,state_->enabled,false};
    }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto text = TextService::measure(state_->title,text_style());
        return {saturating_extent(static_cast<double>(text.width) + state_->style.chevron_size +
                                 state_->style.chevron_gap + 2.0 * state_->style.padding),
                saturating_extent(std::max(text.height,state_->style.chevron_size) + 2.0 * state_->style.padding)};
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        const auto preferred = measure(children);
        return {saturating_extent(state_->style.chevron_size + state_->style.chevron_gap +
                                 2.0 * state_->style.padding),preferred.h};
    }
    void mount(MountContext& context) override {
        state_->request_focus = context.focus_requester(); contact_->mounted = true;
    }
    void unmount(LifecycleContext&) override {
        contact_->mounted = false; clear_press_noexcept(contact_); state_->request_focus = {};
    }
    void deactivate(LifecycleContext&) override {
        clear_press_noexcept(contact_); contact_->focused = contact_->hovered = false;
    }
    void focus_changed(bool focused,FocusContext& context) override {
        const auto contact = contact_;
        const auto state = state_;
        contact->focused = focused;
        if (!focused) clear_press_noexcept(contact);
        if (focused && state->group_select) state->group_select();
        context.invalidate();
    }
    [[nodiscard]] const void* focus_group_identity() const noexcept override { return state_->group_identity; }
    [[nodiscard]] bool focus_group_selected() const override {
        return state_->group_selected && state_->group_selected();
    }
    void focus_group_select() override { if (state_->group_select) state_->group_select(); }
    [[nodiscard]] bool focus_group_accepts_navigation_key(Key key) const noexcept override {
        return key == Key::Up || key == Key::Down;
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = SemanticRole::Button;
        info.name = state_->style.accessible_name.empty() ? state_->title : state_->style.accessible_name;
        info.expanded = state_->open ? SemanticExpandedState::Expanded : SemanticExpandedState::Collapsed;
        info.focusable = true;
        info.focused = contact_->focused;
        info.enabled = effective_enabled();
        info.read_only = effective_read_only() || !editable_source();
        if (info.enabled) info.actions.push_back(SemanticAction::Focus);
        if (info.enabled && !info.read_only) {
            info.actions.push_back(SemanticAction::Toggle);
            info.actions.push_back(state_->open ? SemanticAction::Collapse : SemanticAction::Expand);
        }
        return info;
    }
    EventResult input(const InputEvent& event,InputContext& context) override {
        // Both objects survive a retained invalidation that removes this header.
        // Nothing below accesses this after crossing an input/context boundary.
        const auto state = state_;
        const auto contact = contact_;
        const bool editable = effective_enabled() && !effective_read_only() && editable_source();
        const auto can_edit = [&] {
            return contact->mounted && state->mounted && state->can_write && state->can_write();
        };
        if (event.type == InputType::PointerCancel) {
            clear_press(contact); context.invalidate(); return EventResult::Handled;
        }
        if (event.type == InputType::PointerLeave) {
            if (contact->hovered) { contact->hovered = false; context.invalidate(); }
            return EventResult::Ignored;
        }
        if (event.type == InputType::KeyDown && event.key == Key::Escape &&
            (contact->pointer_pressed || contact->pressed_key != Key::None)) {
            clear_press(contact); context.invalidate(); return EventResult::Handled;
        }
        if (event.type == InputType::KeyDown && state->group_edge &&
            (event.key == Key::Home || event.key == Key::End)) {
            const auto edge = state->group_edge;
            edge(event.key); return EventResult::Handled;
        }
        if (!editable) { clear_press(contact); return EventResult::Ignored; }
        if (event.type == InputType::PointerDown) {
            auto release = context.pointer_releaser();
            const auto cleared = contact->serial+1;
            clear_press(contact);
            if (!can_edit() || contact->serial != cleared) return EventResult::Handled;
            contact->release_pointer = std::move(release);
            contact->pointer_pressed = true;
            const auto serial = ++contact->serial;
            try {
                context.capture_pointer();
                if (can_edit() && contact->serial == serial) context.invalidate();
            }
            catch (...) {
                if (contact->serial == serial) clear_press_noexcept(contact);
                throw;
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerMove) {
            const bool hovered = context.bounds().contains(event.position);
            if (contact->pointer_pressed && !hovered) clear_press(contact);
            const bool changed = contact->hovered != hovered;
            contact->hovered = hovered;
            const auto result = contact->pointer_pressed ? EventResult::Handled : EventResult::Ignored;
            if (changed) context.invalidate();
            return result;
        }
        if (event.type == InputType::PointerUp && contact->pointer_pressed) {
            const bool toggle = context.bounds().contains(event.position);
            const bool next = !state->open;
            const auto transition = state->transition_serial;
            const auto serial = contact->serial+1;
            clear_press(contact);
            if (!can_edit() || contact->serial != serial || state->transition_serial != transition)
                return EventResult::Handled;
            // Terminal state is already clean if a callable copy throws.
            const auto setter = toggle ? state->set_open : std::function<void(bool)>{};
            if (!can_edit() || contact->serial != serial || state->transition_serial != transition)
                return EventResult::Handled;
            context.invalidate();
            if (toggle && can_edit() && contact->serial == serial &&
                state->transition_serial == transition && setter) setter(next);
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyDown && (event.key == Key::Space || event.key == Key::Enter)) {
            if (contact->pressed_key == Key::None) {
                contact->pressed_key = event.key; ++contact->serial; context.invalidate();
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyUp && event.key == contact->pressed_key &&
            contact->pressed_key != Key::None) {
            const bool next = !state->open;
            const auto transition = state->transition_serial;
            const auto serial = contact->serial+1;
            clear_press(contact);
            if (!can_edit() || contact->serial != serial || state->transition_serial != transition)
                return EventResult::Handled;
            const auto setter = state->set_open;
            if (!can_edit() || contact->serial != serial || state->transition_serial != transition)
                return EventResult::Handled;
            context.invalidate();
            if (can_edit() && contact->serial == serial &&
                state->transition_serial == transition && setter) setter(next);
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyDown && (event.key == Key::Left || event.key == Key::Right)) {
            const auto transition = state->transition_serial;
            const auto serial = contact->serial+1;
            clear_press(contact);
            if (!can_edit() || contact->serial != serial || state->transition_serial != transition)
                return EventResult::Handled;
            const auto setter = state->set_open;
            if (!can_edit() || contact->serial != serial || state->transition_serial != transition)
                return EventResult::Handled;
            context.invalidate();
            if (can_edit() && contact->serial == serial &&
                state->transition_serial == transition && setter) setter(event.key == Key::Right);
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }
    void paint(PaintContext& context) const override {
        auto& painter = context.painter();
        const auto bounds = context.bounds();
        auto clip = painter.scoped_clip(bounds);
        const auto& style = state_->style;
        const auto& palette = current_theme().palette;
        auto background = style.background.value_or(palette.surface);
        if (contact_->pointer_pressed || contact_->pressed_key != Key::None) background = style.pressed_background.value_or(palette.border);
        else if (contact_->hovered) background = style.hover_background.value_or(palette.control_hover);
        painter.fill_rounded_rect(bounds,style.corner_radius,background);
        const auto color = effective_enabled() ? style.chevron_color.value_or(palette.text) : palette.disabled;
        const float cx = saturating_coordinate(static_cast<double>(bounds.x) + style.padding + style.chevron_size * 0.5);
        const float cy = saturating_coordinate(static_cast<double>(bounds.y) + bounds.h * 0.5);
        const float radius = style.chevron_size * 0.3f;
        const float angle = state_->phase * 1.57079632679f;
        auto vertex = [&](float x,float y) {
            return Point{saturating_coordinate(static_cast<double>(cx) + x * std::cos(angle) - y * std::sin(angle)),
                         saturating_coordinate(static_cast<double>(cy) + x * std::sin(angle) + y * std::cos(angle))};
        };
        painter.line(vertex(-radius,-radius),vertex(radius,0.0f),1.5f,color);
        painter.line(vertex(radius,0.0f),vertex(-radius,radius),1.5f,color);
        painter.text({saturating_coordinate(static_cast<double>(bounds.x) + style.padding + style.chevron_size + style.chevron_gap),cy},
                     state_->title,text_style());
        if (contact_->focused && bounds.w > 4.0f && bounds.h > 4.0f)
            painter.stroke_rounded_rect({bounds.x+2.0f,bounds.y+2.0f,bounds.w-4.0f,bounds.h-4.0f},
                style.corner_radius,1.0f,style.focus_color.value_or(palette.focus));
    }
private:
    [[nodiscard]] bool editable_source() const { return state_->can_write && state_->can_write(); }
    [[nodiscard]] TextStyle text_style() const {
        TextStyle text;
        text.size = current_theme().typography.control_size;
        text.color = effective_enabled() ? state_->style.text_color.value_or(current_theme().palette.text) :
                                          current_theme().palette.disabled;
        return text;
    }
    static void clear_press(const std::shared_ptr<HeaderContact>& contact) {
        ++contact->serial;
        contact->pointer_pressed = false; contact->pressed_key = Key::None;
        auto release = std::exchange(contact->release_pointer,{});
        if (release) release();
    }
    static void clear_press_noexcept(const std::shared_ptr<HeaderContact>& contact) noexcept {
        try { clear_press(contact); } catch (...) {}
    }
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability& next) noexcept override {
        if (!next.interactive() || next.read_only) clear_press_noexcept(contact_);
    }
    std::shared_ptr<DisclosureState> state_;
    std::shared_ptr<HeaderContact> contact_{std::make_shared<HeaderContact>()};
};

class DisclosureItem final : public Component {
public:
    DisclosureItem(std::shared_ptr<DisclosureState> state,std::shared_ptr<const Spec> content,std::shared_ptr<void> owner)
        : state_(std::move(state)), content_(std::move(content)), owner_(std::move(owner)) {}
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return size_for(children,false); }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return size_for(children,true); }
    [[nodiscard]] Constraints child_constraints(const Constraints& constraints,std::size_t index,std::size_t) const override {
        const auto width = index == 1 ? saturating_extent(static_cast<double>(constraints.max.w) - 2.0 * state_->style.padding) :
                                       constraints.max.w;
        return {{},{width,kUnboundedExtent}};
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements) const override {
        if (children.empty() || placements.empty()) return;
        const auto header = children.front().preferred.h;
        placements[0].bounds = {bounds.x,bounds.y,bounds.w,header};
        if (children.size() < 2 || placements.size() < 2) return;
        const auto natural = panel_height(children,false);
        placements[1].bounds = {
            saturating_coordinate(static_cast<double>(bounds.x) + state_->style.padding),
            saturating_coordinate(static_cast<double>(bounds.y) + header + state_->phase * state_->style.gap),
            saturating_extent(static_cast<double>(bounds.w) - 2.0 * state_->style.padding),
            saturating_extent(static_cast<double>(state_->phase) * natural)};
    }
    void mount(MountContext& context) override {
        state_->invalidate_layout = context.layout_invalidator();
        state_->invalidate_availability = context.availability_invalidator();
        state_->animation_target = {context.invalidator(),state_->invalidate_layout};
        state_->mounted = true;
        if (state_->mounted_callback) state_->mounted_callback();
    }
    void activate(LifecycleContext& context) override {
        state_->dispatcher = context.dispatcher(); state_->active = true;
    }
    void deactivate(LifecycleContext&) override { state_->active = false; snap_motion(state_); }
    void unmount(LifecycleContext&) override {
        state_->mounted = false; state_->pending_effects = 0; snap_motion(state_);
        state_->animations.reset(); state_->dispatcher = {};
        if (state_->unmounted_callback) state_->unmounted_callback();
        state_->invalidate_layout = {}; state_->invalidate_availability = {}; state_->animation_target = {};
    }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override {
        return static_cast<bool>(state_->retained_checkpoint);
    }
    void paint(PaintContext&) const override {}
    std::vector<Spec> children() const {
        auto state = state_;
        auto content = content_;
        Spec header{[state] { return std::make_unique<DisclosureHeader>(state); },{}};
        Spec panel{[state,content] { return std::make_unique<DisclosurePanel>(state,content); },{}};
        panel.children_factory = [state,content](Component&) {
            return state->policy == DisclosureContentPolicy::Retain || state->open ?
                std::vector<Spec>{*content} : std::vector<Spec>{};
        };
        return {std::move(header),std::move(panel)};
    }
private:
    void retained_checkpoint() override {
        if (state_->retained_checkpoint) state_->retained_checkpoint();
    }
    float panel_height(const std::vector<ChildMetrics>& children,bool minimum) const {
        if (children.size() < 2) return 0.0f;
        if (state_->policy == DisclosureContentPolicy::UnmountWhenClosed && !state_->open && state_->phase > 0.0f)
            return state_->cached_height;
        return children[1].participates_in_layout ? (minimum ? children[1].minimum.h : children[1].preferred.h) : 0.0f;
    }
    Size size_for(const std::vector<ChildMetrics>& children,bool minimum) const {
        if (children.empty()) return {};
        const auto header = minimum ? children.front().minimum : children.front().preferred;
        const auto panel = children.size() > 1 && children[1].participates_in_layout ?
            (minimum ? children[1].minimum : children[1].preferred) : Size{};
        return {saturating_extent(std::max(static_cast<double>(header.w),
                    state_->phase > 0.0f ? static_cast<double>(panel.w) + 2.0 * state_->style.padding : 0.0)),
                saturating_extent(static_cast<double>(header.h) + state_->phase *
                    (static_cast<double>(panel_height(children,minimum)) + state_->style.gap + state_->style.padding))};
    }
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability& next) noexcept override {
        state_->allowed = next.visibility == VisibilityMode::Visible;
        if (!state_->allowed) snap_motion(state_);
    }
    void layout_committed(Rect,Rect after) noexcept override {
        state_->bounds_nonempty = after.w > 0.0f && after.h > 0.0f;
        if (!state_->bounds_nonempty) snap_motion(state_);
    }
    std::shared_ptr<DisclosureState> state_;
    std::shared_ptr<const Spec> content_;
    std::shared_ptr<void> owner_;
};

struct BoolDisclosureController {
    explicit BoolDisclosureController(Binding<bool> value) : source(std::move(value)) {}
    Binding<bool> source;
    Binding<bool>::Subscription subscription;
    std::shared_ptr<DisclosureState> state;
    std::function<void(bool)> callback;
    bool syncing{};
    std::uint64_t write_serial{};
    void sync() {
        prepare_disclosure_change(state,source.get());
        if (syncing) return;
        syncing = true;
        try { flush_disclosure_change(state); }
        catch (...) { syncing = false; throw; }
        syncing = false;
    }
    void write(bool next) {
        if (!state->mounted || !source.valid() || source.get() == next) return;
        const bool before = source.get();
        const auto serial = ++write_serial;
        if (state->invalidate_layout) state->invalidate_layout();
        if (!state->mounted || !source.valid() || write_serial != serial) return;
        try { source.set(next); }
        catch (...) {
            const auto failure = std::current_exception();
            try { sync(); } catch (...) {}
            std::rethrow_exception(failure);
        }
        sync();
        if (!state->mounted || !source.valid() || write_serial != serial) return;
        const bool accepted = source.get();
        if (accepted == before) return;
        const auto notify = callback;
        if (notify && state->mounted && source.valid() && write_serial == serial &&
            source.get() == accepted) notify(accepted);
    }
};
} // namespace

void validate_disclosure(std::string_view title,const CollapsibleStyle& style,DisclosureContentPolicy policy) {
    if (policy != DisclosureContentPolicy::Retain && policy != DisclosureContentPolicy::UnmountWhenClosed)
        throw std::invalid_argument("Disclosure content policy is invalid");
    for (const auto value : {style.gap,style.padding,style.chevron_size,style.chevron_gap,style.corner_radius})
        if (!std::isfinite(value) || value < 0.0f) throw std::invalid_argument("Disclosure style extents must be finite and nonnegative");
    if (title.empty() && style.accessible_name.empty()) throw std::invalid_argument("Disclosure requires a title or accessible name");
}
namespace {
constexpr unsigned kFocusEffect = 1U;
constexpr unsigned kMotionEffect = 2U;
constexpr unsigned kStructureEffect = 4U;
constexpr unsigned kAvailabilityEffect = 8U;
constexpr unsigned kLayoutEffect = 16U;
}
void prepare_disclosure_change(const std::shared_ptr<DisclosureState>& state,bool next) noexcept {
    if (state->open == next) return;
    const bool was_open = state->open;
    state->open = next;
    ++state->transition_serial;
    state->pending_effects = kMotionEffect | kAvailabilityEffect | kLayoutEffect;
    if (state->policy == DisclosureContentPolicy::UnmountWhenClosed)
        state->pending_effects |= kStructureEffect;
    if (was_open && !next && state->focus_within) state->pending_effects |= kFocusEffect;
}
void flush_disclosure_change(const std::shared_ptr<DisclosureState>& state) {
    const auto serial = state->transition_serial;
    const auto live = [&] { return state->mounted && state->transition_serial == serial; };
    const auto run = [&](unsigned effect,const std::function<void()>& callback) {
        if (!live() || !(state->pending_effects & effect)) return;
        // A copy failure has not started the effect. Once invocation starts,
        // Tree owns any retained retry; never replay an application callback.
        const auto invoke = callback;
        state->pending_effects &= ~effect;
        if (invoke) invoke();
    };
    run(kFocusEffect,state->request_focus);
    if (live() && (state->pending_effects & kMotionEffect)) {
        state->pending_effects &= ~kMotionEffect;
        animate_to_target(state);
    }
    run(kStructureEffect,state->invalidate_structure);
    run(kAvailabilityEffect,state->invalidate_availability);
    run(kLayoutEffect,state->invalidate_layout);
}
Spec disclosure_spec(std::shared_ptr<DisclosureState> state,std::shared_ptr<const Spec> content,std::shared_ptr<void> owner) {
    Spec result{[state,content,owner] { return std::make_unique<DisclosureItem>(state,content,owner); },{}};
    result.children_factory = [](Component& component) { return static_cast<DisclosureItem&>(component).children(); };
    return result;
}
} // namespace ui::detail

namespace ui {
Collapsible&& Collapsible::content_policy(DisclosureContentPolicy value) && {
    policy_ = value; return std::move(*this);
}
Collapsible&& Collapsible::on_change(std::function<void(bool)> callback) && {
    on_change_ = std::move(callback); return std::move(*this);
}
Collapsible&& Collapsible::style(CollapsibleStyle value) && {
    style_ = std::move(value); return std::move(*this);
}
Spec Collapsible::spec() && {
    detail::validate_disclosure(title_,style_,policy_);
    const auto title = std::move(title_);
    const auto source = open_;
    const auto policy = policy_;
    const auto callback = std::move(on_change_);
    const auto style = std::move(style_);
    const auto content = std::make_shared<const Spec>(std::move(child_));
    Spec result{[title,source,policy,callback,style,content] {
        auto controller = std::make_shared<detail::BoolDisclosureController>(source);
        controller->callback = callback;
        auto state = std::make_shared<detail::DisclosureState>();
        controller->state = state;
        state->title = title; state->style = style; state->policy = policy;
        state->open = source.get(); state->phase = state->open ? 1.0f : 0.0f;
        const std::weak_ptr<detail::BoolDisclosureController> weak = controller;
        state->can_write = [weak] { const auto current = weak.lock(); return current && current->state->mounted && current->source.valid(); };
        state->set_open = [weak](bool next) { if (const auto current = weak.lock()) current->write(next); };
        state->mounted_callback = [weak] {
            if (const auto current = weak.lock()) {
                current->subscription = current->source.observe([weak](bool) {
                    if (const auto owner = weak.lock()) owner->sync();
                });
                current->sync();
            }
        };
        state->retained_checkpoint = [weak] { if (const auto current = weak.lock()) current->sync(); };
        state->unmounted_callback = [weak] { if (const auto current = weak.lock()) current->subscription.reset(); };
        return std::make_unique<detail::DisclosureItem>(state,content,controller);
    },{}};
    result.children_factory = [](Component& component) { return static_cast<detail::DisclosureItem&>(component).children(); };
    return result;
}
} // namespace ui
