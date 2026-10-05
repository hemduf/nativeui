#include <nativeui/field.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/form_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>

namespace ui {
namespace {
struct FieldContact {
    std::function<void()> release;
    std::uint64_t serial{};
    bool pressed{};
};
void clear_contact(const std::shared_ptr<FieldContact>& contact) {
    ++contact->serial; contact->pressed = false;
    auto release = std::exchange(contact->release,{}); if (release) release();
}
void clear_contact_noexcept(const std::shared_ptr<FieldContact>& contact) noexcept { try { clear_contact(contact); } catch (...) {} }
struct FieldState {
    FieldState(std::string name,std::string help,std::string failure,FieldStyle recipe,bool required,
               std::string target,std::optional<Binding<std::string>> help_source,std::optional<Binding<std::string>> error_source)
        : label(std::move(name)),constant_help(std::move(help)),constant_error(std::move(failure)),style(std::move(recipe)),
          required(required),target(std::move(target)),help_source(std::move(help_source)),error_source(std::move(error_source)) {
        record->label = label;
        if (required && !label.empty()) record->label += " *";
        sync_sources();
    }
    std::string label;
    std::string constant_help;
    std::string constant_error;
    FieldStyle style;
    bool required{};
    std::string target;
    std::optional<Binding<std::string>> help_source;
    std::optional<Binding<std::string>> error_source;
    Binding<std::string>::Subscription help_subscription;
    Binding<std::string>::Subscription error_subscription;
    std::string help;
    std::string error;
    std::shared_ptr<detail::FormLabelRecord> record{std::make_shared<detail::FormLabelRecord>()};
    std::weak_ptr<detail::FormContext> form;
    std::shared_ptr<FieldContact> contact{std::make_shared<FieldContact>()};
    std::function<void()> invalidate_layout;
    std::function<void()> invalidate_paint;
    std::function<void()> request_action;
    bool mounted{};
    bool fonts_dirty{true};
    std::shared_ptr<const TextStyle> help_style;
    std::shared_ptr<const TextStyle> error_style;
    [[nodiscard]] std::string read_help() const {
        return help_source ? (help_source->valid() ? help_source->get() : std::string{}) : constant_help;
    }
    [[nodiscard]] std::string read_error() const {
        return error_source ? (error_source->valid() ? error_source->get() : std::string{}) : constant_error;
    }
    void sync_sources() {
        auto next_help = read_help();
        auto next_error = read_error();
        if (next_help == help && next_error == error) return;
        help.swap(next_help); error.swap(next_error);
        const auto invalidate = invalidate_layout;
        if (mounted && invalidate) invalidate();
    }
    void sync_fonts(const Theme& theme,bool publish_invalidation=true) {
        if (!fonts_dirty) return;
        const auto next_label = std::make_shared<const TextStyle>(detail::form_text_style(theme,style.label_text,theme.palette.text,true));
        const auto next_help = std::make_shared<const TextStyle>(detail::form_text_style(theme,style.description_text,theme.palette.muted_text));
        const auto next_error = std::make_shared<const TextStyle>(detail::form_text_style(theme,style.error_text,Color{0.8f,0.18f,0.18f,1.0f}));
        const bool metrics_changed = !record->text_style || !help_style || !error_style ||
            !detail::form_font_metrics_equal(*record->text_style,*next_label) ||
            !detail::form_font_metrics_equal(*help_style,*next_help) || !detail::form_font_metrics_equal(*error_style,*next_error);
        record->text_style = next_label; help_style = next_help; error_style = next_error; fonts_dirty = false;
        const auto invalidate = metrics_changed ? invalidate_layout : invalidate_paint;
        if (publish_invalidation && mounted && invalidate) invalidate();
    }
    [[nodiscard]] std::string description() const {
        std::string result = read_help();
        const auto append = [&](std::string_view text) {
            if (text.empty()) return;
            if (!result.empty()) result += '\n';
            result += text;
        };
        append(read_error()); if (required) append("Required field.");
        return result;
    }
};
struct FieldGeometry {
    std::string label;
    std::string help;
    std::string error;
    TextStyle label_style;
    TextStyle help_style;
    TextStyle error_style;
    detail::Paragraph label_lines;
    detail::Paragraph help_lines;
    detail::Paragraph error_lines;
    Rect label_bounds{};
    Rect control_bounds{};
    Rect help_bounds{};
    Rect error_bounds{};
    std::optional<float> baseline;
    Size minimum{};
    Size preferred{};
    TextAlign alignment{TextAlign::Left};
};
class FieldComponent final : public Component,public detail::ThemeBinding,public detail::FormBindable {
public:
    explicit FieldComponent(std::shared_ptr<FieldState> state) : state_(std::move(state)) {
        state_->sync_fonts(default_theme());
    }
    void bind_theme(const Theme& theme) noexcept override { ThemeBinding::bind_theme(theme); state_->fonts_dirty = true; }
    void bind_form(std::weak_ptr<detail::FormContext> context,float leading,float trailing) override {
        state_->form = std::move(context); state_->record->leading_inset = leading; state_->record->trailing_inset = trailing;
    }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return calculate(Constraints::unbounded(),children)->preferred; }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return calculate(Constraints::unbounded(),children)->minimum; }
    [[nodiscard]] ChildMetrics measure_constrained(const Constraints& constraints,const std::vector<ChildMetrics>& children) const override {
        const auto form = state_->form.lock();
        if (!form || form->collecting_minimum()) state_->record->control_minimum = children.empty() ? 0.0f : children.front().minimum.w;
        const auto geometry = calculate(constraints,children);
        ChildMetrics result{constraints.constrain(geometry->minimum),constraints.constrain(geometry->preferred)};
        result.first_baseline = geometry->baseline;
        return result;
    }
    [[nodiscard]] Constraints child_constraints(const Constraints& constraints,std::size_t,std::size_t) const override {
        float width = constraints.max.w;
        const auto form = state_->form.lock();
        if (form && form->collecting_minimum()) width = kUnboundedExtent;
        else if (form && !form->stacked() && constraints.bounded_width()) {
            const auto label = std::max(0.0f,form->label_width()-state_->record->leading_inset);
            width = std::max(state_->record->control_minimum,detail::saturating_extent(static_cast<double>(width)-label-(form->has_labels()?form->style().column_gap:0.0f)));
        }
        return {{},{width,kUnboundedExtent}};
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements) const override {
        auto geometry = calculate(Constraints::tight({bounds.w,bounds.h}),children);
        const auto translate = [bounds](Rect& rect) {
            rect.x = detail::saturating_coordinate(static_cast<double>(rect.x)+bounds.x);
            rect.y = detail::saturating_coordinate(static_cast<double>(rect.y)+bounds.y);
        };
        translate(geometry->label_bounds); translate(geometry->control_bounds); translate(geometry->help_bounds); translate(geometry->error_bounds);
        if (!placements.empty()) placements.front().bounds = geometry->control_bounds;
        candidate_ = std::move(geometry);
    }
    void mount(MountContext& context) override {
        const auto state = state_;
        state->invalidate_layout = context.layout_invalidator(); state->invalidate_paint = context.invalidator();
        state->request_action = context.descendant_action_requester(state->target);
        state->record->participates = effective_visibility() != VisibilityMode::Collapsed;
        state->mounted = true;
        state->sync_fonts(current_theme());
        if (const auto form = state->form.lock()) state->record->registration = form->add(state->record);
        const std::weak_ptr<FieldState> weak = state;
        if (state->help_source) state->help_subscription = state->help_source->observe([weak](const auto&) { if (const auto current = weak.lock()) current->sync_sources(); });
        if (state->error_source) state->error_subscription = state->error_source->observe([weak](const auto&) { if (const auto current = weak.lock()) current->sync_sources(); });
        state->sync_sources();
    }
    void unmount(LifecycleContext&) override {
        const auto state = state_;
        state->mounted = false; clear_contact_noexcept(state->contact);
        state->help_subscription.reset(); state->error_subscription.reset();
        if (const auto form = state->form.lock()) form->remove(state->record->registration);
        state->record->registration = 0;
        state->invalidate_layout = {}; state->invalidate_paint = {}; state->request_action = {};
    }
    void deactivate(LifecycleContext&) override { clear_contact_noexcept(state_->contact); }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info; info.role = SemanticRole::Group; info.name = state_->label; info.description = state_->description();
        info.enabled = effective_enabled(); info.read_only = effective_read_only(); return info;
    }
    EventResult input(const InputEvent& event,InputContext& context) override {
        const auto state = state_; const auto contact = state->contact;
        if (event.type == InputType::PointerCancel) { clear_contact(contact); return EventResult::Handled; }
        if (!effective_enabled() || !published_) return EventResult::Ignored;
        const bool inside = !published_->label_bounds.empty() && published_->label_bounds.contains(event.position);
        if (event.type == InputType::PointerDown && inside) {
            auto release = context.pointer_releaser(); const auto cleared = contact->serial+1;
            clear_contact(contact);
            if (!state->mounted || contact->serial != cleared) return EventResult::Handled;
            contact->pressed = true; contact->release = std::move(release); const auto serial = ++contact->serial;
            try { context.capture_pointer(); if (state->mounted && contact->serial == serial) context.invalidate(); }
            catch (...) { if (contact->serial == serial) clear_contact_noexcept(contact); throw; }
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerMove && contact->pressed && !inside) { clear_contact(contact); return EventResult::Handled; }
        if (event.type == InputType::PointerUp && contact->pressed) {
            const auto serial = contact->serial+1; clear_contact(contact);
            if (!inside || !state->mounted || contact->serial != serial) return EventResult::Handled;
            const auto request = state->request_action;
            if (!state->mounted || contact->serial != serial) return EventResult::Handled;
            context.invalidate();
            if (state->mounted && contact->serial == serial && request) request();
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }
    void paint(PaintContext& context) const override {
        const auto geometry = published_;
        if (!geometry) return;
        auto label = geometry->label_style,help = geometry->help_style,error = geometry->error_style;
        if (!state_->style.label_text) label.color = current_theme().palette.text;
        if (!state_->style.description_text) help.color = current_theme().palette.muted_text;
        if (!effective_enabled()) label.color = help.color = error.color = current_theme().palette.disabled;
        auto& painter = context.painter();
        detail::paint_form_text(painter,geometry->label_bounds,geometry->label,label,geometry->label_lines,geometry->alignment);
        detail::paint_form_text(painter,geometry->help_bounds,geometry->help,help,geometry->help_lines);
        detail::paint_form_text(painter,geometry->error_bounds,geometry->error,error,geometry->error_lines);
        if (!geometry->error.empty() && state_->style.error_indicator_width > 0.0f)
            painter.fill_rounded_rect({geometry->error_bounds.x,geometry->error_bounds.y,state_->style.error_indicator_width,geometry->error_bounds.h},0.0f,
                state_->style.error_indicator_color.value_or(error.color));
    }
private:
    void retained_checkpoint() override {
        const auto state = state_;
        state->sync_fonts(current_theme()); state->sync_sources();
    }
    [[nodiscard]] std::optional<detail::DescendantSemanticDecoration> descendant_semantic_decoration() const override {
        return detail::DescendantSemanticDecoration{state_->target,state_->label,state_->description()};
    }
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability& next) noexcept override {
        state_->record->participates = next.visibility != VisibilityMode::Collapsed;
        if (!next.interactive()) clear_contact_noexcept(state_->contact);
    }
    void layout_committed(Rect,Rect) noexcept override { published_ = std::move(candidate_); }
    [[nodiscard]] std::shared_ptr<FieldGeometry> calculate(const Constraints& constraints,const std::vector<ChildMetrics>& children) const {
        state_->sync_fonts(current_theme(),false);
        auto geometry = std::make_shared<FieldGeometry>();
        geometry->label = state_->record->label; geometry->help = state_->read_help(); geometry->error = state_->read_error();
        geometry->label_style = *state_->record->text_style; geometry->help_style = *state_->help_style; geometry->error_style = *state_->error_style;
        const auto control = children.empty() ? Size{} : children.front().preferred;
        const auto minimum = children.empty() ? Size{} : children.front().minimum;
        const auto form = state_->form.lock(); const bool stacked = !form || form->stacked();
        const auto natural_label = detail::wrap_form_text(geometry->label,geometry->label_style,kUnboundedExtent);
        const auto column_gap=form && form->has_labels()?form->style().column_gap:0.0f;
        const auto width = constraints.bounded_width() ? constraints.max.w :
            (stacked ? std::max(control.w,natural_label.width) : detail::saturating_extent(
                static_cast<double>(std::max(0.0f,form->label_width()-state_->record->leading_inset))+column_gap+control.w));
        const auto label_width = stacked ? width : std::max(0.0f,form->label_width()-state_->record->leading_inset);
        const auto gap = stacked ? state_->style.label_gap : column_gap;
        const auto control_x = stacked ? 0.0f : detail::saturating_extent(static_cast<double>(label_width)+gap);
        const auto control_width = std::max(minimum.w,detail::saturating_extent(static_cast<double>(width)-control_x));
        geometry->label_lines = detail::wrap_form_text(geometry->label,geometry->label_style,label_width);
        geometry->help_lines = detail::wrap_form_text(geometry->help,geometry->help_style,control_width);
        geometry->error_lines = detail::wrap_form_text(geometry->error,geometry->error_style,control_width);
        float label_y = 0.0f,control_y = 0.0f;
        if (stacked) {
            if (!geometry->label.empty()) control_y = detail::saturating_extent(static_cast<double>(geometry->label_lines.height)+gap);
        } else if (!geometry->label.empty()) {
            const auto baseline = children.empty() ? std::optional<float>{} : children.front().first_baseline;
            const auto label_baseline = geometry->label_lines.first_baseline;
            if (baseline) { label_y = std::max(0.0f,*baseline-label_baseline); control_y = std::max(0.0f,label_baseline-*baseline); }
            else { label_y = std::max(0.0f,(control.h-geometry->label_lines.line_height)*0.5f); control_y = std::max(0.0f,(geometry->label_lines.line_height-control.h)*0.5f); }
            geometry->alignment = form->style().label_alignment;
        }
        geometry->label_bounds = {0.0f,label_y,label_width,geometry->label_lines.height};
        geometry->control_bounds = {control_x,control_y,control_width,control.h};
        double bottom = std::max(static_cast<double>(control_y)+control.h,static_cast<double>(label_y)+geometry->label_lines.height);
        if (!geometry->help.empty()) {
            bottom += state_->style.description_gap;
            geometry->help_bounds = {control_x,detail::saturating_extent(bottom),control_width,geometry->help_lines.height}; bottom += geometry->help_lines.height;
        }
        if (!geometry->error.empty()) {
            bottom += state_->style.error_gap;
            geometry->error_bounds = {control_x,detail::saturating_extent(bottom),control_width,geometry->error_lines.height}; bottom += geometry->error_lines.height;
        }
        if (!children.empty() && children.front().first_baseline)
            geometry->baseline = detail::saturating_extent(static_cast<double>(control_y)+*children.front().first_baseline);
        geometry->preferred = {width,detail::saturating_extent(bottom)};
        const auto minimum_control_y = control_y;
        double minimum_bottom = std::max(static_cast<double>(minimum_control_y)+minimum.h,
            static_cast<double>(label_y)+geometry->label_lines.height);
        if (!geometry->help.empty()) minimum_bottom += state_->style.description_gap+geometry->help_lines.height;
        if (!geometry->error.empty()) minimum_bottom += state_->style.error_gap+geometry->error_lines.height;
        geometry->minimum = {detail::saturating_extent(static_cast<double>(control_x)+minimum.w),detail::saturating_extent(minimum_bottom)};
        return geometry;
    }
    std::shared_ptr<FieldState> state_;
    mutable std::shared_ptr<FieldGeometry> candidate_;
    std::shared_ptr<const FieldGeometry> published_;
};
}
Field&& Field::description(std::string text) && { description_ = std::move(text); description_binding_.reset(); return std::move(*this); }
Field&& Field::description(Binding<std::string> text) && { description_binding_ = std::move(text); description_.clear(); return std::move(*this); }
Field&& Field::description(State<std::string>& text) && { return std::move(*this).description(text.binding()); }
Field&& Field::error(std::string text) && { error_ = std::move(text); error_binding_.reset(); return std::move(*this); }
Field&& Field::error(Binding<std::string> text) && { error_binding_ = std::move(text); error_.clear(); return std::move(*this); }
Field&& Field::error(State<std::string>& text) && { return std::move(*this).error(text.binding()); }
Field&& Field::target(std::string key) && { target_ = std::move(key); return std::move(*this); }
Field&& Field::required(bool value) && { required_ = value; return std::move(*this); }
Field&& Field::style(FieldStyle value) && { style_ = std::move(value); return std::move(*this); }
Spec Field::spec() && {
    for (float extent : {style_.label_gap,style_.description_gap,style_.error_gap,style_.error_indicator_width}) detail::validate_form_extent(extent);
    detail::validate_form_text_style(style_.label_text); detail::validate_form_text_style(style_.description_text); detail::validate_form_text_style(style_.error_text);
    const auto label = std::move(label_),help = std::move(description_),error = std::move(error_),target = std::move(target_);
    const auto style = std::move(style_); const auto required = required_;
    const auto help_source = description_binding_,error_source = error_binding_;
    return {[label,help,error,target,style,required,help_source,error_source] {
        return std::make_unique<FieldComponent>(std::make_shared<FieldState>(label,help,error,style,required,target,help_source,error_source));
    },{std::move(child_)}};
}
} // namespace ui
