#include <nativeui/tabs.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ui::detail {
namespace {
struct TabsContact {
    std::optional<std::size_t> active;
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> pressed;
    std::function<void()> release;
    std::uint64_t serial{};
    bool mounted{};
    bool focused{};
};
void release_contact(const std::shared_ptr<TabsContact>& contact) {
    ++contact->serial;
    contact->pressed.reset();
    auto release = std::exchange(contact->release,{});
    if (release) release();
}
void release_contact_noexcept(const std::shared_ptr<TabsContact>& contact) noexcept {
    try { release_contact(contact); } catch (...) {}
}
struct TabsState : std::enable_shared_from_this<TabsState> {
    TabsState(std::shared_ptr<const TabsRecipe> value,std::unique_ptr<TabsSelection> model)
        : recipe(std::move(value)),selection(std::move(model)) {
        if (!selection) throw std::invalid_argument("Tabs selection must not be null");
        selected = selection->snapshot();
        if (selected.size() != recipe->panels.size()) throw std::logic_error("Tabs key/panel count mismatch");
    }
    std::shared_ptr<const TabsRecipe> recipe;
    std::unique_ptr<TabsSelection> selection;
    std::unique_ptr<TabsSubscription> subscription;
    std::shared_ptr<TabsContact> contact{std::make_shared<TabsContact>()};
    std::vector<bool> selected;
    std::function<void()> invalidate_layout;
    std::function<void()> invalidate_availability;
    std::function<void()> invalidate_paint;
    std::uint64_t selection_serial{};
    unsigned pending{};
    bool syncing{};
    bool interactive{true};

    [[nodiscard]] bool enabled_at(std::size_t index) const noexcept {
        return index < recipe->enabled.size() && recipe->enabled[index];
    }
    [[nodiscard]] bool selected_at(std::size_t index) const noexcept {
        return index < selected.size() && selected[index];
    }
    [[nodiscard]] std::optional<std::size_t> selected_enabled() const noexcept {
        for (std::size_t i = 0; i < selected.size(); ++i)
            if (selected[i] && enabled_at(i)) return i;
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> edge(bool last) const noexcept {
        const auto count = recipe->enabled.size();
        for (std::size_t distance = 0; distance < count; ++distance) {
            const auto i = last ? count-1-distance : distance;
            if (enabled_at(i)) return i;
        }
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> step(std::size_t from,bool backwards) const noexcept {
        const auto count = recipe->enabled.size();
        if (count == 0) return {};
        from %= count;
        for (std::size_t distance = 1; distance <= count; ++distance) {
            const auto i = backwards ? (from+count-(distance%count))%count : (from+distance)%count;
            if (enabled_at(i)) return i;
        }
        return {};
    }
    void sync() {
        auto candidate = selection->snapshot();
        if (candidate.size() != selected.size()) throw std::logic_error("Tabs selection mask mismatch");
        if (candidate != selected) {
            selected.swap(candidate); ++selection_serial;
            pending = 7U;
        }
        if (syncing || !contact->mounted) return;
        syncing = true;
        const auto serial = selection_serial;
        const auto run = [&](unsigned bit,const std::function<void()>& callback) {
            if (!contact->mounted || selection_serial != serial || !(pending & bit)) return;
            const auto invoke = callback;
            pending &= ~bit;
            if (invoke) invoke();
        };
        try {
            run(1U,invalidate_layout);
            run(2U,invalidate_availability);
            run(4U,invalidate_paint);
        } catch (...) { syncing = false; throw; }
        syncing = false;
    }
    void select(std::size_t index) {
        if (!contact->mounted || !interactive || !selection->valid() || !enabled_at(index)) return;
        // Mark geometry dirty before State dispatch can terminate at an earlier
        // application observer, leaving this observer unstarted.
        const auto serial = contact->serial;
        const auto epoch = selection_serial;
        if (invalidate_layout) invalidate_layout();
        if (!contact->mounted || !interactive || !selection->valid() ||
            contact->serial != serial || selection_serial != epoch) return;
        const std::weak_ptr<TabsState> weak = shared_from_this();
        selection->select(index,[weak,serial,epoch] {
            const auto current = weak.lock();
            return current && current->contact->mounted && current->interactive && current->selection->valid() &&
                current->contact->serial == serial && current->selection_serial == epoch;
        });
        sync();
    }
};

class TabMetadata final : public Component {
public:
    TabMetadata(std::shared_ptr<TabsState> state,std::size_t index)
        : state_(std::move(state)),index_(index) {}
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override { return {}; }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = SemanticRole::Tab; info.name = state_->recipe->labels[index_];
        info.selected = state_->selected_at(index_);
        info.enabled = effective_enabled() && state_->enabled_at(index_);
        info.read_only = effective_read_only() || !state_->selection->valid();
        return info;
    }
    void paint(PaintContext&) const override {}
private:
    std::shared_ptr<TabsState> state_;
    std::size_t index_{};
};
class TabPanel final : public Component {
public:
    TabPanel(std::shared_ptr<TabsState> state,std::size_t index) : state_(std::move(state)),index_(index) {}
    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        return {state_->selected_at(index_) ? VisibilityMode::Visible : VisibilityMode::Collapsed,true,false};
    }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info; info.role = SemanticRole::TabPanel;
        info.name = state_->recipe->labels[index_]; info.selected = true;
        info.enabled = effective_enabled(); info.read_only = effective_read_only(); return info;
    }
    void paint(PaintContext&) const override {}
private:
    std::shared_ptr<TabsState> state_;
    std::size_t index_{};
};

class TabsComponent final : public Component,public ThemeBinding {
public:
    explicit TabsComponent(std::shared_ptr<TabsState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return size_for(children,false); }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return size_for(children,true); }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>& placements) const override {
        const auto resolved = container_style(state_->contact->focused);
        const auto count = state_->recipe->panels.size();
        const float header = std::min(bounds.h,resolved.header_height);
        const float gap = std::min(std::max(0.0f,bounds.h-header),resolved.panel_gap);
        const Rect panel{bounds.x,saturating_coordinate(static_cast<double>(bounds.y)+header+gap),
            bounds.w,std::max(0.0f,bounds.h-header-gap)};
        for (std::size_t i = 0; i < placements.size(); ++i)
            placements[i].bounds = i < count ? tab_rect(i,bounds,header,count) : panel;
    }
    void mount(MountContext& context) override {
        const auto state = state_;
        state->invalidate_layout = context.layout_invalidator();
        state->invalidate_availability = context.availability_invalidator();
        state->invalidate_paint = context.invalidator();
        state->contact->mounted = true;
        const std::weak_ptr<TabsState> weak = state;
        state->subscription = state->selection->observe([weak] { if (const auto current = weak.lock()) current->sync(); });
        state->sync();
    }
    void unmount(LifecycleContext&) override {
        const auto state = state_;
        state->contact->mounted = false; release_contact_noexcept(state->contact);
        state->subscription.reset(); state->pending = 0;
        state->invalidate_layout = {}; state->invalidate_availability = {}; state->invalidate_paint = {};
    }
    void deactivate(LifecycleContext& context) override {
        const auto contact = state_->contact;
        const auto before = container_style(contact->focused);
        contact->pressed.reset(); contact->hovered.reset(); contact->focused = false;
        const auto after = container_style(false);
        release_contact_noexcept(contact);
        invalidate_transition(before,after,context);
    }
    void focus_changed(bool focused,FocusContext& context) override {
        const auto contact = state_->contact;
        const auto before = container_style(contact->focused);
        contact->focused = focused;
        if (!focused) contact->active.reset();
        else { contact->active = state_->selected_enabled(); if (!contact->active) contact->active = state_->edge(false); }
        const auto after = container_style(focused);
        invalidate_transition(before,after,context);
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info; info.role = SemanticRole::Tabs; info.focusable = true;
        info.focused = state_->contact->focused; info.enabled = effective_enabled();
        info.read_only = effective_read_only() || !state_->selection->valid();
        if (info.enabled) info.actions.push_back(SemanticAction::Focus);
        return info;
    }
    EventResult input(const InputEvent& event,InputContext& context) override {
        const auto state = state_;
        const auto contact = state->contact;
        if (event.type == InputType::PointerCancel) {
            const bool armed = contact->pressed.has_value();
            release_contact(contact); context.invalidate();
            return armed ? EventResult::Handled : EventResult::Ignored;
        }
        if (!effective_enabled()) { release_contact(contact); return EventResult::Ignored; }
        if (event.type == InputType::KeyDown) {
            if (const auto selected = state->selected_enabled()) contact->active = selected;
            else if (!contact->active) contact->active = state->edge(false);
            std::optional<std::size_t> target;
            switch (event.key) {
            case Key::Right: target = contact->active ? state->step(*contact->active,false) : state->edge(false); break;
            case Key::Left: target = contact->active ? state->step(*contact->active,true) : state->edge(true); break;
            case Key::Home: target = state->edge(false); break;
            case Key::End: target = state->edge(true); break;
            default: return EventResult::Ignored;
            }
            if (target) { contact->active = target; state->select(*target); }
            return EventResult::Handled;
        }
        const auto before = container_style(contact->focused);
        const auto index = tab_at(event.position,context.bounds(),before.header_height,state->recipe->panels.size());
        const auto hovered = index && state->enabled_at(*index) ? index : std::nullopt;
        if (event.type == InputType::PointerMove || event.type == InputType::PointerLeave) {
            contact->hovered = event.type == InputType::PointerLeave ? std::nullopt : hovered;
            const auto after = container_style(contact->focused);
            invalidate_transition(before,after,context); return EventResult::Ignored;
        }
        if (event.type == InputType::PointerDown) {
            auto release = context.pointer_releaser();
            const auto serial = contact->serial+1;
            release_contact(contact);
            if (!contact->mounted || contact->serial != serial || !state->interactive) return EventResult::Handled;
            contact->hovered = hovered;
            if (!hovered) { const auto after = container_style(contact->focused); invalidate_transition(before,after,context); return index ? EventResult::Handled : EventResult::Ignored; }
            contact->pressed = hovered; contact->release = std::move(release);
            const auto armed = ++contact->serial;
            const auto after = container_style(contact->focused);
            try { context.capture_pointer(); invalidate_transition(before,after,context); }
            catch (...) { if (contact->serial == armed) release_contact_noexcept(contact); throw; }
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerUp) {
            const auto armed = contact->pressed;
            contact->hovered = hovered; contact->pressed.reset();
            const auto after = container_style(contact->focused);
            const auto serial = contact->serial+1;
            release_contact(contact);
            invalidate_transition(before,after,context);
            if (!armed) return EventResult::Ignored;
            if (index && index == armed && contact->mounted && contact->serial == serial) {
                contact->active = armed; state->select(*armed);
            }
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }
    std::vector<Spec> children() const {
        const auto state = state_;
        const auto count = state->recipe->panels.size();
        std::vector<Spec> result; result.reserve(2*count);
        for (std::size_t i = 0; i < count; ++i)
            result.push_back({[state,i] { return std::make_unique<TabMetadata>(state,i); },{}});
        for (std::size_t i = 0; i < count; ++i)
            result.push_back({[state,i] { return std::make_unique<TabPanel>(state,i); },{state->recipe->panels[i]}});
        return result;
    }
    void paint(PaintContext& context) const override {
        const auto count = state_->recipe->panels.size();
        if (count == 0) return;
        const auto bounds = context.bounds();
        const auto container = container_style(context.focused());
        const float header_height = std::min(bounds.h,container.header_height);
        if (!(header_height > 0.0f) || !(bounds.w > 0.0f)) return;
        auto& painter = context.painter();
        const Rect header{bounds.x,bounds.y,bounds.w,header_height};
        painter.fill_rounded_rect(header,container.header_corner_radius,container.header_fill);
        painter.stroke_rounded_rect(header,container.header_corner_radius,container.header_border_width,container.header_border);
        for (std::size_t i = 0; i < count; ++i) {
            const auto tab = tab_rect(i,bounds,header_height,count);
            const bool selected = state_->selected_at(i);
            const bool hovered = effective_enabled() && state_->enabled_at(i) && !selected && state_->contact->hovered == i;
            const bool pressed = hovered && state_->contact->pressed == i;
            const auto resolved = resolve_tabs_style(default_tabs_style(current_theme()),state_->recipe->style,
                VisualState{.enabled=effective_enabled() && state_->enabled_at(i),.read_only=effective_read_only(),
                    .hovered=hovered,.pressed=pressed,.focused=context.focused(),.selected=selected});
            const Rect fill{tab.x+resolved.tab_inset,tab.y+resolved.tab_inset,
                std::max(0.0f,tab.w-2*resolved.tab_inset),std::max(0.0f,tab.h-2*resolved.tab_inset)};
            if (!fill.empty()) painter.fill_rounded_rect(fill,resolved.tab_corner_radius,resolved.tab_fill);
            const float underline_width = std::max(0.0f,tab.w-2*resolved.underline_inset);
            if (selected && underline_width > 0.0f && resolved.underline_height > 0.0f)
                painter.fill_rounded_rect({tab.x+resolved.underline_inset,tab.y+tab.h-resolved.underline_height-2,
                    underline_width,resolved.underline_height},resolved.underline_height*0.5f,resolved.underline);
            if (i > 0 && resolved.separator_width > 0 && tab.h > 2*resolved.separator_inset)
                painter.line({tab.x,tab.y+resolved.separator_inset},{tab.x,tab.y+tab.h-resolved.separator_inset},resolved.separator_width,resolved.separator);
            painter.text({tab.x+tab.w*0.5f,tab.y+tab.h*0.5f},state_->recipe->labels[i],resolved.text_size,resolved.text,TextAlign::Center);
        }
        const float gap = std::min(std::max(0.0f,bounds.h-header_height),container.panel_gap);
        const Rect panel{bounds.x,bounds.y+header_height+gap,bounds.w,std::max(0.0f,bounds.h-header_height-gap)};
        if (!panel.empty()) {
            painter.fill_rounded_rect(panel,container.panel_corner_radius,container.panel_fill);
            painter.stroke_rounded_rect(panel,container.panel_corner_radius,container.panel_border_width,container.panel_border);
        }
    }
private:
    void retained_checkpoint() override { state_->sync(); }
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability& next) noexcept override {
        state_->interactive = next.interactive();
        if (!state_->interactive) release_contact_noexcept(state_->contact);
    }
    [[nodiscard]] Size size_for(const std::vector<ChildMetrics>& children,bool minimum) const {
        const auto style = container_style(state_->contact->focused);
        Size result{minimum ? 0.0f : 120.0f,style.header_height};
        for (std::size_t i = state_->recipe->panels.size(); i < children.size(); ++i) {
            if (!children[i].participates_in_layout) continue;
            const auto size = minimum ? children[i].minimum : children[i].preferred;
            result.w = std::max(result.w,size.w);
            result.h = std::max(result.h,saturating_extent(static_cast<double>(style.header_height)+style.panel_gap+size.h));
        }
        return result;
    }
    [[nodiscard]] ResolvedTabsStyle container_style(bool focused) const {
        const auto contact = state_->contact;
        const bool hovered = contact->hovered && state_->enabled_at(*contact->hovered) &&
            effective_enabled() && !state_->selected_at(*contact->hovered);
        return resolve_tabs_style(default_tabs_style(current_theme()),state_->recipe->style,
            VisualState{.enabled=effective_enabled(),.read_only=effective_read_only(),.hovered=hovered,
                .pressed=hovered && contact->pressed == contact->hovered,.focused=focused});
    }
    template <class Context>
    static void invalidate_transition(const ResolvedTabsStyle& before,const ResolvedTabsStyle& after,Context& context) {
        if (before == after) return;
        if (before.header_height != after.header_height || before.panel_gap != after.panel_gap) {
            context.invalidate_layout(); return;
        }
        context.invalidate();
    }
    [[nodiscard]] static Rect tab_rect(std::size_t index,Rect bounds,float header,std::size_t count) noexcept {
        if (count == 0) return {};
        const double width = static_cast<double>(bounds.w)/static_cast<double>(count);
        const auto x = saturating_coordinate(static_cast<double>(bounds.x)+width*static_cast<double>(index));
        const auto end = saturating_coordinate(static_cast<double>(bounds.x)+width*static_cast<double>(index+1));
        return {x,bounds.y,std::max(0.0f,end-x),header};
    }
    [[nodiscard]] static std::optional<std::size_t> tab_at(Point point,Rect bounds,float header,std::size_t count) noexcept {
        if (count == 0 || !(bounds.w > 0.0f) || point.y < bounds.y || point.y >= bounds.y+std::min(bounds.h,header) ||
            point.x < bounds.x || point.x >= bounds.x+bounds.w) return {};
        const double relative = static_cast<double>(point.x-bounds.x)/bounds.w;
        return std::min(count-1,static_cast<std::size_t>(std::floor(relative*static_cast<double>(count))));
    }
    std::shared_ptr<TabsState> state_;
};
} // namespace
Spec make_tabs_spec(std::shared_ptr<const TabsRecipe> recipe,std::function<std::unique_ptr<TabsSelection>()> selection_factory) {
    if (!recipe || !selection_factory) throw std::invalid_argument("Tabs recipe must not be null");
    Spec result{[recipe,selection_factory] {
        return std::make_unique<TabsComponent>(std::make_shared<TabsState>(recipe,selection_factory()));
    },{}};
    result.children_factory = [](Component& component) { return static_cast<TabsComponent&>(component).children(); };
    return result;
}
} // namespace ui::detail
