#include <nativeui/form.hpp>

#include "detail/form_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ui {
namespace {
struct FormActions {
    std::function<void()> submit;
    std::function<void()> cancel;
    std::uint64_t serial{};
    bool mounted{};
};
class FormComponent final : public Component {
public:
    FormComponent(FormLayout layout,double threshold,FormStyle style,
                  std::function<void()> submit,std::function<void()> cancel)
        : context_(std::make_shared<detail::FormContext>(layout,threshold,std::move(style))),
          actions_(std::make_shared<FormActions>()) { actions_->submit=std::move(submit); actions_->cancel=std::move(cancel); }
    [[nodiscard]] std::size_t child_measurement_passes() const noexcept override { return 2; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return size_for(children,false); }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return size_for(children,true); }
    [[nodiscard]] Constraints child_constraints(const Constraints& constraints,std::size_t,std::size_t) const override {
        return {{},{constraints.max.w,kUnboundedExtent}};
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements) const override {
        double y = bounds.y; bool previous = false;
        for (std::size_t i = 0; i < std::min(children.size(),placements.size()); ++i) {
            if (!children[i].participates_in_layout) { placements[i].bounds = {bounds.x,detail::saturating_coordinate(y),0.0f,0.0f}; continue; }
            if (previous) y += context_->style().row_gap;
            placements[i].bounds = {bounds.x,detail::saturating_coordinate(y),bounds.w,children[i].preferred.h};
            y += children[i].preferred.h; previous = true;
        }
    }
    void mount(MountContext& context) override { context_->layout_invalidator = context.layout_invalidator(); ++actions_->serial; actions_->mounted=true; }
    void unmount(LifecycleContext&) override { actions_->mounted=false; ++actions_->serial; context_->layout_invalidator = {}; }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info; info.role = SemanticRole::Group; info.name = context_->style().accessible_name;
        info.enabled = effective_enabled(); info.read_only = effective_read_only(); return info;
    }
    EventResult input(const InputEvent& event,InputContext& context) override {
        if (!effective_enabled()) return EventResult::Ignored;
        const auto actions=actions_; const auto serial=actions->serial;
        const bool submit=event.type==InputType::Command && event.command==Command::Submit;
        const bool cancel=(event.type==InputType::Command && event.command==Command::Cancel) ||
            (event.type==InputType::KeyDown && event.key==Key::Escape);
        if (!submit && !cancel) return EventResult::Ignored;
        const auto callback=submit?actions->submit:actions->cancel;
        if (!callback) return EventResult::Ignored;
        if (actions->mounted && actions->serial==serial &&
            detail::InputMutationAccess::action_allowed(context)) callback();
        return EventResult::Handled;
    }
    void paint(PaintContext&) const override {}
private:
    void bind_descendant_context(Component& descendant) const override {
        if (auto* participant = dynamic_cast<detail::FormBindable*>(&descendant)) participant->bind_form(context_,0.0f,0.0f);
    }
    void measure_children_pass_started(const Constraints& constraints,std::size_t pass) const override {
        context_->begin_pass(constraints,pass);
    }
    [[nodiscard]] Size size_for(const std::vector<ChildMetrics>& children,bool minimum) const {
        double width = 0.0,height = 0.0; bool previous = false;
        for (const auto& child : children) {
            if (!child.participates_in_layout) continue;
            if (previous) height += context_->style().row_gap;
            const auto size = minimum ? child.minimum : child.preferred;
            width = std::max(width,static_cast<double>(size.w)); height += size.h; previous = true;
        }
        return {detail::saturating_extent(width),detail::saturating_extent(height)};
    }
    std::shared_ptr<detail::FormContext> context_;
    std::shared_ptr<FormActions> actions_;
};
}
Form&& Form::layout(FormLayout value) && { layout_ = value; return std::move(*this); }
Form&& Form::stacked_below(double value) && { threshold_ = value; return std::move(*this); }
Form&& Form::on_submit(std::function<void()> callback) && { submit_ = std::move(callback); return std::move(*this); }
Form&& Form::on_cancel(std::function<void()> callback) && { cancel_ = std::move(callback); return std::move(*this); }
Form&& Form::style(FormStyle value) && { style_ = std::move(value); return std::move(*this); }
Spec Form::spec() && {
    if (layout_ != FormLayout::Aligned && layout_ != FormLayout::Stacked && layout_ != FormLayout::Responsive)
        throw std::invalid_argument("Form layout is invalid");
    if (!std::isfinite(threshold_) || threshold_ < 0.0) throw std::invalid_argument("Form responsive threshold must be finite and nonnegative");
    detail::validate_form_extent(style_.row_gap); detail::validate_form_extent(style_.column_gap);
    if (style_.label_alignment != TextAlign::Left && style_.label_alignment != TextAlign::Right && style_.label_alignment != TextAlign::Center)
        throw std::invalid_argument("Form label alignment is invalid");
    const auto layout = layout_; const auto threshold = threshold_;
    const auto style = std::move(style_); const auto submit = std::move(submit_); const auto cancel = std::move(cancel_);
    return {[layout,threshold,style,submit,cancel] {
        return std::make_unique<FormComponent>(layout,threshold,style,submit,cancel);
    },std::move(children_)};
}
} // namespace ui
