#include <nativeui/list_view.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/scroll_view.hpp>

#include "detail/layout_support.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace ui::detail {
namespace {
struct ListContact {
    std::optional<std::size_t> active;
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    std::function<void()> release;
    std::uint64_t serial{};
    bool mounted{};
    bool focused{};
};
void clear_contact(const std::shared_ptr<ListContact>& contact) {
    ++contact->serial; contact->pressed.reset();
    auto release=std::exchange(contact->release,{}); if (release) release();
}
void clear_contact_noexcept(const std::shared_ptr<ListContact>& contact) noexcept { try { clear_contact(contact); } catch (...) {} }
struct ListState : std::enable_shared_from_this<ListState> {
    ListState(std::shared_ptr<const ListRecipe> recipe,std::unique_ptr<ListSelection> selection)
        : recipe(std::move(recipe)),selection(std::move(selection)),scroll(ScrollAxis::Vertical) {
        if (!this->selection) throw std::invalid_argument("ListView selection must not be null");
        selected=this->selection->snapshot();
        if (selected.size()!=this->recipe->rows.size()) throw std::logic_error("ListView selection mask/row count mismatch");
    }
    std::shared_ptr<const ListRecipe> recipe;
    std::unique_ptr<ListSelection> selection;
    std::unique_ptr<ListSubscription> subscription;
    ScrollState scroll;
    std::shared_ptr<ListContact> contact{std::make_shared<ListContact>()};
    std::vector<bool> selected;
    std::vector<Rect> row_bounds;
    std::function<void()> invalidate_paint;
    std::uint64_t model_serial{};
    unsigned pending{};
    bool syncing{};
    bool interactive{true};
    [[nodiscard]] bool enabled_at(std::size_t index) const noexcept { return index<recipe->enabled.size() && recipe->enabled[index]; }
    [[nodiscard]] bool selected_at(std::size_t index) const noexcept { return enabled_at(index) && index<selected.size() && selected[index]; }
    [[nodiscard]] std::optional<std::size_t> selected_index() const noexcept {
        for (std::size_t i=0;i<selected.size();++i) if (selected_at(i)) return i;
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> edge(bool last) const noexcept {
        for (std::size_t distance=0;distance<recipe->enabled.size();++distance) {
            const auto index=last?recipe->enabled.size()-1-distance:distance;
            if (enabled_at(index)) return index;
        }
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> step(std::size_t index,bool previous) const noexcept {
        if (previous) { for (std::size_t i=index;i>0;--i) if (enabled_at(i-1)) return i-1; }
        else { for (std::size_t i=index+1;i<recipe->enabled.size();++i) if (enabled_at(i)) return i; }
        return {};
    }
    void sync() {
        auto candidate=selection->snapshot();
        if (candidate.size()!=selected.size()) throw std::logic_error("ListView selection mask mismatch");
        if (candidate!=selected) { selected.swap(candidate); ++model_serial; pending=3U; }
        if (syncing || !contact->mounted) return;
        syncing=true;
        const auto epoch=model_serial;
        try {
            if (pending&1U) {
                const auto index=selected_index();
                if (!index || *index<row_bounds.size()) {
                    pending&=~1U;
                    if (index) (void)ensure_visible(scroll,row_bounds[*index],ScrollAlignment::Nearest);
                }
            }
            if (contact->mounted && model_serial==epoch && (pending&2U)) {
                const auto invalidate=invalidate_paint; pending&=~2U;
                if (invalidate) invalidate();
            }
        } catch (...) { syncing=false; throw; }
        syncing=false;
    }
    void publish(std::size_t index,bool activate) {
        if (!contact->mounted || !interactive || !selection->valid() || !enabled_at(index)) return;
        const auto serial=contact->serial,epoch=model_serial;
        const auto invalidate=invalidate_paint;
        if (invalidate) invalidate();
        if (index<row_bounds.size()) (void)ensure_visible(scroll,row_bounds[index],ScrollAlignment::Nearest);
        const std::weak_ptr<ListState> weak=shared_from_this();
        selection->publish(index,activate,[weak,serial,epoch](bool committed) {
            const auto current=weak.lock();
            return current && current->contact->mounted && current->interactive && current->selection->valid() &&
                current->contact->serial==serial && (committed || current->model_serial==epoch);
        });
        if (contact->mounted) sync();
    }
};
ResolvedListViewStyle row_style(const std::shared_ptr<ListState>& state,const Theme& theme,std::size_t index,
                               ComponentAvailability availability) {
    const bool selected=state->selected_at(index);
    const bool hovered=!selected && state->contact->hovered==index;
    return resolve_list_view_style(default_list_view_style(theme),state->recipe->style,
        VisualState{.enabled=availability.enabled && state->enabled_at(index),.read_only=availability.read_only,
            .hovered=hovered,.pressed=!selected && state->contact->pressed==index,.selected=selected});
}
// Contact changes affect at most the old hover/press and the new hover/press.
// Keep resolved values and the theme defaults owned across release callbacks.
struct ListContactPresentation {
    ListViewStyle defaults;
    ComponentAvailability availability;
    std::array<std::optional<std::size_t>,3> indices;
    std::array<std::optional<ResolvedListViewStyle>,3> before;
    ListContactPresentation(const std::shared_ptr<ListState>& state,const Theme& theme,
        ComponentAvailability availability,std::optional<std::size_t> next)
        : defaults(default_list_view_style(theme)),availability(availability),
          indices{state->contact->hovered,state->contact->pressed,next} {
        for (std::size_t i=0;i<indices.size();++i) if (indices[i]) before[i]=resolve(state,*indices[i]);
    }
    [[nodiscard]] ResolvedListViewStyle resolve(const std::shared_ptr<ListState>& state,std::size_t index) const {
        const bool selected=state->selected_at(index);
        return resolve_list_view_style(defaults,state->recipe->style,
            VisualState{.enabled=availability.enabled && state->enabled_at(index),.read_only=availability.read_only,
                .hovered=!selected && state->contact->hovered==index,
                .pressed=!selected && state->contact->pressed==index,.selected=selected});
    }
    [[nodiscard]] bool changed(const std::shared_ptr<ListState>& state) const {
        for (std::size_t i=0;i<indices.size();++i)
            if (indices[i] && before[i]!=resolve(state,*indices[i])) return true;
        return false;
    }
};
class ListRow final : public Component,public ThemeBinding {
public:
    ListRow(std::shared_ptr<ListState> state,std::size_t index) : state_(std::move(state)),index_(index) {}
    [[nodiscard]] ComponentAvailability local_availability() const noexcept override { return {VisibilityMode::Visible,state_->enabled_at(index_),false}; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return children.empty()?Size{}:children.front().preferred; }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return children.empty()?Size{}:children.front().minimum; }
    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,const std::vector<ChildMetrics>& children) const override {
        auto result=Component::measure_constrained(constraints,children);
        if (!children.empty() && children.front().participates_in_layout) result.first_baseline=children.front().first_baseline;
        return result;
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>& placements) const override { if (!placements.empty()) placements.front().bounds=bounds; }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo result; result.role=SemanticRole::ListItem; result.selected=state_->selected_at(index_);
        result.enabled=effective_enabled(); result.read_only=effective_read_only() || !state_->selection->valid(); return result;
    }
    void paint(PaintContext& context) const override {
        const auto bounds=context.bounds(); const auto resolved=row_style(state_,current_theme(),index_,effective_availability());
        auto& painter=context.painter();
        const Rect highlight{bounds.x+resolved.row_horizontal_inset,bounds.y+resolved.row_vertical_inset,
            std::max(0.0f,bounds.w-2*resolved.row_horizontal_inset),std::max(0.0f,bounds.h-2*resolved.row_vertical_inset)};
        if (!highlight.empty()) painter.fill_rounded_rect(highlight,resolved.row_corner_radius,resolved.row_fill);
        if (state_->selected_at(index_) && resolved.row_accent_width>0.0f) {
            const Rect accent{highlight.x+resolved.row_accent_horizontal_inset,highlight.y+resolved.row_accent_vertical_inset,
                resolved.row_accent_width,std::max(0.0f,highlight.h-2*resolved.row_accent_vertical_inset)};
            if (!accent.empty()) painter.fill_rounded_rect(accent,resolved.row_accent_width*0.5f,resolved.row_accent);
        }
        if (index_+1<state_->recipe->rows.size() && resolved.separator_width>0.0f && bounds.w>2*resolved.separator_inset) {
            const auto y=bounds.y+bounds.h-resolved.separator_width*0.5f;
            painter.line({bounds.x+resolved.separator_inset,y},{bounds.x+bounds.w-resolved.separator_inset,y},resolved.separator_width,resolved.separator);
        }
    }
private:
    std::shared_ptr<ListState> state_;
    std::size_t index_{};
};
class ListContent final : public Component {
public:
    explicit ListContent(std::shared_ptr<ListState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return false; }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return false; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return size_for(children,false); }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return size_for(children,true); }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements) const override {
        auto candidate=std::make_shared<std::vector<Rect>>(placements.size());
        double y=bounds.y;
        for (std::size_t i=0;i<std::min(children.size(),placements.size());++i) {
            const auto height=children[i].participates_in_layout?children[i].preferred.h:0.0f;
            placements[i].bounds={bounds.x,saturating_coordinate(y),bounds.w,height};
            (*candidate)[i]={0.0f,saturating_extent(y-bounds.y),bounds.w,height}; y+=height;
        }
        candidate_=std::move(candidate);
    }
    void paint(PaintContext&) const override {}
private:
    void layout_committed(Rect,Rect) noexcept override {
        if (candidate_) state_->row_bounds.swap(*candidate_);
        candidate_.reset(); state_->pending|=1U;
    }
    static Size size_for(const std::vector<ChildMetrics>& children,bool minimum) {
        double width=0.0,height=0.0;
        for (const auto& child:children) {
            if (!child.participates_in_layout) continue;
            const auto size=minimum?child.minimum:child.preferred; width=std::max(width,static_cast<double>(size.w)); height+=size.h;
        }
        return {saturating_extent(width),saturating_extent(height)};
    }
    std::shared_ptr<ListState> state_;
    mutable std::shared_ptr<std::vector<Rect>> candidate_;
};
class OwnedListSpec {
public:
    explicit OwnedListSpec(Spec value):value_(std::move(value)) {}
    Spec spec() && { return std::move(value_); }
private:
    Spec value_;
};
class ListComponent final : public Component,public ThemeBinding {
public:
    explicit ListComponent(std::shared_ptr<ListState> state):state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return children.empty()?Size{}:children.front().preferred; }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return children.empty()?Size{}:children.front().minimum; }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>& placements) const override { if (!placements.empty()) placements.front().bounds=bounds; }
    void mount(MountContext& context) override {
        const auto state=state_; state->invalidate_paint=context.invalidator(); state->contact->mounted=true;
        const std::weak_ptr<ListState> weak=state;
        state->subscription=state->selection->observe([weak] { if (const auto current=weak.lock()) current->sync(); });
        state->pending|=3U; state->sync();
    }
    void unmount(LifecycleContext&) override {
        const auto state=state_; state->contact->mounted=false; clear_contact_noexcept(state->contact);
        state->subscription.reset(); state->invalidate_paint={}; state->contact->hovered.reset(); state->contact->active.reset();
    }
    void deactivate(LifecycleContext&) override {
        const auto contact=state_->contact; contact->focused=false; contact->hovered.reset(); contact->active.reset(); clear_contact_noexcept(contact);
    }
    void focus_changed(bool focused,FocusContext& context) override {
        const auto state=state_; const auto contact=state->contact;
        const auto before=surface_style(); contact->focused=focused;
        if (!focused) contact->active.reset();
        else { contact->active=state->selected_index(); if (!contact->active) contact->active=state->edge(false); }
        const auto after=surface_style();
        if (!focused) clear_contact(contact);
        if (contact->mounted && before!=after) context.invalidate();
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo result; result.role=SemanticRole::ListView; result.enabled=effective_enabled();
        result.read_only=effective_read_only() || !state_->selection->valid(); return result;
    }
    EventResult input(const InputEvent& event,InputContext& context) override {
        const auto state=state_; const auto contact=state->contact;
        if (event.type==InputType::PointerCancel) {
            const ListContactPresentation presentation{state,current_theme(),effective_availability(),{}};
            const bool pressed=contact->pressed.has_value(); const auto serial=contact->serial+1;
            clear_contact(contact);
            if (contact->mounted && contact->serial==serial && presentation.changed(state)) context.invalidate();
            return pressed?EventResult::Handled:EventResult::Ignored;
        }
        if (event.type==InputType::KeyDown) {
            state->sync(); if (!contact->mounted || !state->interactive) return EventResult::Ignored;
            if (const auto selected=state->selected_index()) contact->active=selected;
            else if (!contact->active) contact->active=state->edge(false);
            if (event.key==Key::Enter || event.key==Key::Space) {
                if (!contact->active) return EventResult::Ignored;
                ++contact->serial; state->publish(*contact->active,true); return EventResult::Handled;
            }
            std::optional<std::size_t> target;
            switch (event.key) {
            case Key::Down: target=contact->active?state->step(*contact->active,false):state->edge(false); break;
            case Key::Up: target=contact->active?state->step(*contact->active,true):state->edge(true); break;
            case Key::Home: target=state->edge(false); break;
            case Key::End: target=state->edge(true); break;
            default: return EventResult::Ignored;
            }
            if (target) { ++contact->serial; contact->active=target; state->publish(*target,false); }
            return EventResult::Handled;
        }
        const auto index=row_at(event.position,context.bounds());
        const auto hovered=index && state->enabled_at(*index)?index:std::optional<std::size_t>{};
        if (event.type==InputType::PointerMove || event.type==InputType::PointerLeave) {
            const auto next=event.type==InputType::PointerLeave?std::optional<std::size_t>{}:hovered;
            if (contact->hovered!=next) {
                const ListContactPresentation presentation{state,current_theme(),effective_availability(),next};
                contact->hovered=next;
                if (presentation.changed(state)) context.invalidate();
            }
            return EventResult::Ignored;
        }
        if (event.type==InputType::PointerDown) {
            const ListContactPresentation presentation{state,current_theme(),effective_availability(),hovered};
            auto release=context.pointer_releaser(); const auto expected=contact->serial+1; clear_contact(contact);
            if (!contact->mounted || !state->interactive || contact->serial!=expected) return EventResult::Ignored;
            contact->hovered=hovered;
            if (!hovered) {
                if (presentation.changed(state)) context.invalidate();
                return index?EventResult::Handled:EventResult::Ignored;
            }
            contact->pressed=hovered; contact->release=std::move(release); const auto serial=++contact->serial;
            try {
                context.capture_pointer();
                if (contact->mounted && contact->serial==serial && presentation.changed(state)) context.invalidate();
            }
            catch (...) { if (contact->serial==serial) clear_contact_noexcept(contact); throw; }
            return EventResult::Handled;
        }
        if (event.type==InputType::PointerUp) {
            const ListContactPresentation presentation{state,current_theme(),effective_availability(),hovered};
            const auto pressed=contact->pressed; const auto serial=contact->serial+1;
            contact->hovered=hovered; clear_contact(contact);
            if (!pressed) return EventResult::Ignored;
            if (contact->mounted && contact->serial==serial && presentation.changed(state)) context.invalidate();
            if (index==pressed && contact->mounted && contact->serial==serial) { contact->active=pressed; state->publish(*pressed,true); }
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }
    std::vector<Spec> children() const {
        const auto state=state_; std::vector<Spec> rows; rows.reserve(state->recipe->rows.size());
        for (std::size_t i=0;i<state->recipe->rows.size();++i)
            rows.push_back({[state,i] { return std::make_unique<ListRow>(state,i); },{state->recipe->rows[i]}});
        Spec content{[state] { return std::make_unique<ListContent>(state); },std::move(rows)};
        return {std::move(ScrollView{state->scroll,OwnedListSpec{std::move(content)}}).spec()};
    }
    void paint(PaintContext& context) const override {
        const auto resolved=surface_style(); auto& painter=context.painter(); const auto bounds=context.bounds();
        painter.fill_rounded_rect(bounds,resolved.surface_corner_radius,resolved.surface_fill);
        painter.stroke_rounded_rect(bounds,resolved.surface_corner_radius,resolved.surface_border_width,resolved.surface_border);
    }
private:
    void retained_checkpoint() override { state_->sync(); }
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability& next) noexcept override {
        state_->interactive=next.interactive(); if (!state_->interactive) clear_contact_noexcept(state_->contact);
    }
    [[nodiscard]] ResolvedListViewStyle surface_style() const {
        return resolve_list_view_style(default_list_view_style(current_theme()),state_->recipe->style,
            VisualState{.enabled=effective_enabled(),.read_only=effective_read_only(),.focused=state_->contact->focused});
    }
    [[nodiscard]] std::optional<std::size_t> row_at(Point position,Rect bounds) const noexcept {
        if (!bounds.contains(position)) return {};
        const auto offset=state_->scroll.offset(); const Point content{position.x-bounds.x+offset.x,position.y-bounds.y+offset.y};
        for (std::size_t i=0;i<state_->row_bounds.size();++i) if (state_->row_bounds[i].contains(content)) return i;
        return {};
    }
    std::shared_ptr<ListState> state_;
};
} // namespace
Spec make_list_view_spec(std::shared_ptr<const ListRecipe> recipe,std::function<std::unique_ptr<ListSelection>()> selection_factory) {
    if (!recipe || !selection_factory) throw std::invalid_argument("ListView recipe must not be null");
    Spec result{[recipe,selection_factory] { return std::make_unique<ListComponent>(std::make_shared<ListState>(recipe,selection_factory())); },{}};
    result.children_factory=[](Component& component) { return static_cast<ListComponent&>(component).children(); };
    return result;
}
} // namespace ui::detail
