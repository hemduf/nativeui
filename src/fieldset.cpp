#include <nativeui/fieldset.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/form_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <memory>

namespace ui {
namespace {
struct FieldsetGeometry {
    TextStyle legend_style;
    TextStyle description_style;
    detail::Paragraph legend_lines;
    detail::Paragraph description_lines;
    Rect legend_bounds;
    Rect description_bounds;
    float children_y{};
    Size preferred;
    Size minimum;
};
struct FieldsetState {
    FieldsetState(std::string legend,std::string description,FieldsetStyle style,
                  std::optional<Binding<bool>> enabled,std::optional<Binding<bool>> read_only)
        : legend(std::move(legend)),description(std::move(description)),style(std::move(style)),
          enabled_source(std::move(enabled)),read_only_source(std::move(read_only)) {
        enabled_value = !enabled_source || enabled_source->get();
        read_only_value = read_only_source && read_only_source->get();
    }
    std::string legend;
    std::string description;
    FieldsetStyle style;
    std::optional<Binding<bool>> enabled_source;
    std::optional<Binding<bool>> read_only_source;
    Binding<bool>::Subscription enabled_subscription;
    Binding<bool>::Subscription read_only_subscription;
    std::weak_ptr<detail::FormContext> form;
    float leading_inset{};
    float trailing_inset{};
    bool enabled_value{true};
    bool read_only_value{};
    bool mounted{};
    bool fonts_dirty{true};
    std::function<void()> invalidate_layout;
    std::function<void()> invalidate_paint;
    std::function<void()> invalidate_availability;
    std::shared_ptr<const TextStyle> legend_style;
    std::shared_ptr<const TextStyle> description_style;
    void sync_sources() {
        const bool next_enabled = !enabled_source || enabled_source->get();
        const bool next_read_only = read_only_source && read_only_source->get();
        if (next_enabled == enabled_value && next_read_only == read_only_value) return;
        enabled_value = next_enabled; read_only_value = next_read_only;
        const auto invalidate = invalidate_availability;
        if (mounted && invalidate) invalidate();
    }
    void sync_fonts(const Theme& theme,bool publish_invalidation=true) {
        if (!fonts_dirty) return;
        const auto next_legend = std::make_shared<const TextStyle>(detail::form_text_style(theme,style.legend_text,theme.palette.text,true));
        const auto next_description = std::make_shared<const TextStyle>(detail::form_text_style(theme,style.description_text,theme.palette.muted_text));
        const bool metrics_changed = !legend_style || !description_style ||
            !detail::form_font_metrics_equal(*legend_style,*next_legend) || !detail::form_font_metrics_equal(*description_style,*next_description);
        legend_style = next_legend; description_style = next_description; fonts_dirty = false;
        const auto invalidate = metrics_changed ? invalidate_layout : invalidate_paint;
        if (publish_invalidation && mounted && invalidate) invalidate();
    }
};
class FieldsetComponent final : public Component,public detail::ThemeBinding,public detail::FormBindable {
public:
    explicit FieldsetComponent(std::shared_ptr<FieldsetState> state) : state_(std::move(state)) { state_->sync_fonts(default_theme()); }
    void bind_theme(const Theme& theme) noexcept override { ThemeBinding::bind_theme(theme); state_->fonts_dirty = true; }
    void bind_form(std::weak_ptr<detail::FormContext> context,float leading,float trailing) override {
        state_->form = std::move(context); state_->leading_inset = leading; state_->trailing_inset = trailing;
    }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability result;
        result.enabled = !state_->enabled_source || state_->enabled_source->get();
        result.read_only = state_->read_only_source && state_->read_only_source->get();
        return result;
    }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return calculate(Constraints::unbounded(),children)->preferred; }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return calculate(Constraints::unbounded(),children)->minimum; }
    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,const std::vector<ChildMetrics>& children) const override {
        const auto geometry = calculate(constraints,children);
        return {constraints.constrain(geometry->minimum),constraints.constrain(geometry->preferred)};
    }
    [[nodiscard]] Constraints child_constraints(const Constraints& constraints,std::size_t,std::size_t) const override {
        const auto width = constraints.bounded_width() ? detail::saturating_extent(static_cast<double>(constraints.max.w)-2.0*state_->style.padding) : kUnboundedExtent;
        return {{},{width,kUnboundedExtent}};
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements) const override {
        auto geometry = calculate(Constraints::tight({bounds.w,bounds.h}),children);
        geometry->legend_bounds.x = detail::saturating_coordinate(static_cast<double>(geometry->legend_bounds.x)+bounds.x);
        geometry->legend_bounds.y = detail::saturating_coordinate(static_cast<double>(geometry->legend_bounds.y)+bounds.y);
        geometry->description_bounds.x = detail::saturating_coordinate(static_cast<double>(geometry->description_bounds.x)+bounds.x);
        geometry->description_bounds.y = detail::saturating_coordinate(static_cast<double>(geometry->description_bounds.y)+bounds.y);
        double y = static_cast<double>(bounds.y)+geometry->children_y;
        const auto x = detail::saturating_coordinate(static_cast<double>(bounds.x)+state_->style.padding);
        const auto width = detail::saturating_extent(static_cast<double>(bounds.w)-2.0*state_->style.padding);
        bool previous = false;
        for (std::size_t i = 0; i < std::min(children.size(),placements.size()); ++i) {
            if (!children[i].participates_in_layout) { placements[i].bounds = {x,detail::saturating_coordinate(y),0.0f,0.0f}; continue; }
            if (previous) y += state_->style.row_gap;
            placements[i].bounds = {x,detail::saturating_coordinate(y),width,children[i].preferred.h};
            y += children[i].preferred.h; previous = true;
        }
        candidate_ = std::move(geometry);
    }
    void mount(MountContext& context) override {
        const auto state = state_;
        state->invalidate_layout = context.layout_invalidator(); state->invalidate_paint = context.invalidator(); state->invalidate_availability = context.availability_invalidator();
        state->mounted = true; state->sync_fonts(current_theme());
        const std::weak_ptr<FieldsetState> weak = state;
        if (state->enabled_source) state->enabled_subscription = state->enabled_source->observe([weak](bool) { if (const auto current = weak.lock()) current->sync_sources(); });
        if (state->read_only_source) state->read_only_subscription = state->read_only_source->observe([weak](bool) { if (const auto current = weak.lock()) current->sync_sources(); });
        state->sync_sources();
    }
    void unmount(LifecycleContext&) override {
        const auto state = state_;
        state->mounted = false; state->enabled_subscription.reset(); state->read_only_subscription.reset();
        state->invalidate_layout = {}; state->invalidate_paint = {}; state->invalidate_availability = {};
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo result; result.role = SemanticRole::Group; result.name = state_->legend; result.description = state_->description;
        result.enabled = effective_enabled(); result.read_only = effective_read_only(); return result;
    }
    void paint(PaintContext& context) const override {
        const auto geometry = published_; if (!geometry) return;
        const auto state = state_;
        auto& painter = context.painter(); const auto bounds = context.bounds();
        if (state->style.background) painter.fill_rounded_rect(bounds,state->style.corner_radius,*state->style.background);
        if (state->style.border_width > 0.0f) painter.stroke_rounded_rect(bounds,state->style.corner_radius,state->style.border_width,state->style.border_color.value_or(current_theme().palette.border));
        auto legend = geometry->legend_style,description = geometry->description_style;
        if (!effective_enabled()) legend.color = description.color = current_theme().palette.disabled;
        detail::paint_form_text(painter,geometry->legend_bounds,state->legend,legend,geometry->legend_lines);
        detail::paint_form_text(painter,geometry->description_bounds,state->description,description,geometry->description_lines);
    }
private:
    void bind_descendant_context(Component& descendant) const override {
        if (auto* participant = dynamic_cast<detail::FormBindable*>(&descendant)) {
            participant->bind_form(state_->form,detail::saturating_extent(static_cast<double>(state_->leading_inset)+state_->style.padding),
                detail::saturating_extent(static_cast<double>(state_->trailing_inset)+state_->style.padding));
        }
    }
    void retained_checkpoint() override { const auto state=state_; state->sync_fonts(current_theme()); state->sync_sources(); }
    void layout_committed(Rect,Rect) noexcept override { published_ = std::move(candidate_); }
    [[nodiscard]] std::shared_ptr<FieldsetGeometry> calculate(const Constraints& constraints,const std::vector<ChildMetrics>& children) const {
        state_->sync_fonts(current_theme(),false);
        auto geometry = std::make_shared<FieldsetGeometry>();
        geometry->legend_style = *state_->legend_style; geometry->description_style = *state_->description_style;
        double preferred_width = 0.0,minimum_width = 0.0,preferred_height = 0.0,minimum_height = 0.0; bool previous = false;
        for (const auto& child : children) {
            if (!child.participates_in_layout) continue;
            if (previous) { preferred_height += state_->style.row_gap; minimum_height += state_->style.row_gap; }
            preferred_width = std::max(preferred_width,static_cast<double>(child.preferred.w));
            minimum_width = std::max(minimum_width,static_cast<double>(child.minimum.w));
            preferred_height += child.preferred.h; minimum_height += child.minimum.h; previous = true;
        }
        const auto legend_natural = detail::wrap_form_text(state_->legend,geometry->legend_style,kUnboundedExtent);
        const auto description_natural = detail::wrap_form_text(state_->description,geometry->description_style,kUnboundedExtent);
        const auto width = constraints.bounded_width() ? constraints.max.w : detail::saturating_extent(std::max({preferred_width,static_cast<double>(legend_natural.width),static_cast<double>(description_natural.width)})+2.0*state_->style.padding);
        const auto inner_width = detail::saturating_extent(static_cast<double>(width)-2.0*state_->style.padding);
        geometry->legend_lines = detail::wrap_form_text(state_->legend,geometry->legend_style,inner_width);
        geometry->description_lines = detail::wrap_form_text(state_->description,geometry->description_style,inner_width);
        double y = state_->style.padding;
        geometry->legend_bounds = {state_->style.padding,detail::saturating_extent(y),inner_width,geometry->legend_lines.height}; y += geometry->legend_lines.height;
        if (!state_->legend.empty() && !state_->description.empty()) y += state_->style.description_gap;
        geometry->description_bounds = {state_->style.padding,detail::saturating_extent(y),inner_width,geometry->description_lines.height}; y += geometry->description_lines.height;
        if ((!state_->legend.empty() || !state_->description.empty()) && previous) y += state_->style.legend_gap;
        geometry->children_y = detail::saturating_extent(y);
        geometry->preferred = {width,detail::saturating_extent(y+preferred_height+state_->style.padding)};
        geometry->minimum = {detail::saturating_extent(minimum_width+2.0*state_->style.padding),detail::saturating_extent(y+minimum_height+state_->style.padding)};
        return geometry;
    }
    std::shared_ptr<FieldsetState> state_;
    mutable std::shared_ptr<FieldsetGeometry> candidate_;
    std::shared_ptr<const FieldsetGeometry> published_;
};
}
Fieldset&& Fieldset::enabled(Binding<bool> value) && { enabled_ = std::move(value); return std::move(*this); }
Fieldset&& Fieldset::enabled(State<bool>& value) && { return std::move(*this).enabled(value.binding()); }
Fieldset&& Fieldset::read_only(Binding<bool> value) && { read_only_ = std::move(value); return std::move(*this); }
Fieldset&& Fieldset::read_only(State<bool>& value) && { return std::move(*this).read_only(value.binding()); }
Fieldset&& Fieldset::description(std::string value) && { description_ = std::move(value); return std::move(*this); }
Fieldset&& Fieldset::style(FieldsetStyle value) && { style_ = std::move(value); return std::move(*this); }
Spec Fieldset::spec() && {
    for (float extent : {style_.padding,style_.row_gap,style_.legend_gap,style_.description_gap,style_.border_width,style_.corner_radius}) detail::validate_form_extent(extent);
    detail::validate_form_text_style(style_.legend_text); detail::validate_form_text_style(style_.description_text);
    const auto legend=std::move(legend_),description=std::move(description_); const auto style=std::move(style_);
    const auto enabled=enabled_,read_only=read_only_;
    return {[legend,description,style,enabled,read_only] { return std::make_unique<FieldsetComponent>(std::make_shared<FieldsetState>(legend,description,style,enabled,read_only)); },std::move(children_)};
}
} // namespace ui
