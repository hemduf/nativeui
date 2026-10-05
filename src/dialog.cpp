#include <nativeui/dialog.hpp>
#include "detail/layout_support.hpp"

namespace ui::detail {

DialogFixedEnabledComponent::DialogFixedEnabledComponent(bool enabled) : enabled_(enabled) {}

ComponentAvailability DialogFixedEnabledComponent::local_availability() const noexcept  {
        ComponentAvailability availability{};
        availability.enabled = enabled_;
        return availability;
    }

Size DialogFixedEnabledComponent::measure(const std::vector<ChildMetrics>& children) const  {
        return children.empty() ? Size{} : children.front().preferred;
    }

Size DialogFixedEnabledComponent::minimum_size(const std::vector<ChildMetrics>& children) const  {
        return children.empty() ? Size{} : children.front().minimum;
    }

Constraints DialogFixedEnabledComponent::child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const  {
        return constraints;
    }

void DialogFixedEnabledComponent::layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const  {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

void DialogFixedEnabledComponent::paint(PaintContext&) const  {}

DialogBackdropComponent::DialogBackdropComponent(Color color) : color_(color) {}

Constraints DialogBackdropComponent::child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const  {
        return constraints.loosen();
    }

ChildMetrics DialogBackdropComponent::measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const  {
        const auto child = children.empty() ? ChildMetrics{} : children.front();
        Size preferred = child.preferred;
        if (constraints.bounded_width()) preferred.w = constraints.max.w;
        if (constraints.bounded_height()) preferred.h = constraints.max.h;
        preferred = constraints.constrain(preferred);
        return ChildMetrics{constraints.constrain({}), preferred};
    }

Size DialogBackdropComponent::measure(const std::vector<ChildMetrics>& children) const  {
        return children.empty() ? Size{} : children.front().preferred;
    }

void DialogBackdropComponent::layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const  {
        if (children.empty() || placements.empty()) return;
        const auto preferred = children.front().preferred;
        const float width = std::min(bounds.w, preferred.w);
        const float height = std::min(bounds.h, preferred.h);
        placements.front().bounds = Rect{
            bounds.x + (bounds.w - width) * 0.5f,
            bounds.y + (bounds.h - height) * 0.5f,
            width,
            height};
    }

void DialogBackdropComponent::paint(PaintContext& context) const  {
        context.painter().fill_rounded_rect(context.bounds(), 0.0f, color_);
    }

DialogPanelComponent::DialogPanelComponent(
        DialogPanelLayout layout,
        std::shared_ptr<ScrollState> body_scroll,
        std::function<void()> on_default)
        : layout_(std::move(layout)),
          body_scroll_(std::move(body_scroll)),
          on_default_(std::move(on_default)) {}

DialogPanelComponent::DialogPanelComponent(DialogPanelLayout layout,
    std::shared_ptr<ScrollState> scroll, std::function<void()> on_default,
    DialogStyle style, std::string title, std::string description)
    : style_(std::move(style)), title_(std::move(title)), description_(std::move(description)),
      layout_(std::move(layout)), body_scroll_(std::move(scroll)), on_default_(std::move(on_default)) {}

bool DialogPanelComponent::focusable() const noexcept  { return true; }

bool DialogPanelComponent::is_focus_scope() const noexcept  { return true; }

bool DialogPanelComponent::focus_scope_active() const noexcept  { return true; }

bool DialogPanelComponent::focus_scope_traps() const noexcept  { return true; }

std::size_t DialogPanelComponent::focus_scope_default_index() const noexcept  { return 1; }

bool DialogPanelComponent::clips_children() const noexcept  { return true; }

Constraints DialogPanelComponent::child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const  {
        const auto outer = outer_constraints(constraints);
        const float inner_width = std::isfinite(outer.max.w)
            ? std::max(0.0f, outer.max.w - padding() * 2.0f)
            : kUnboundedExtent;
        return Constraints::loose({inner_width, kUnboundedExtent});
    }

ChildMetrics DialogPanelComponent::measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const  {
    (void)body_scroll_.get();const auto outer=outer_constraints(constraints);
    auto natural=panel_extent(children,false);auto minimum=outer.constrain(panel_extent(children,true));
    const float width=std::min(natural.w,outer.max.w),inner=std::max(0.0f,width-padding()*2);
    const auto unwrapped=action_row_height(children,false);
    natural.h+=action_rows_height(action_rows(children,inner,false))-unwrapped;
    auto preferred=outer.constrain(natural);preferred.w=std::max(preferred.w,minimum.w);preferred.h=std::max(preferred.h,minimum.h);
    return ChildMetrics{minimum,preferred};
}

Size DialogPanelComponent::measure(const std::vector<ChildMetrics>& children) const  {
        return panel_extent(children, false);
    }

Size DialogPanelComponent::minimum_size(const std::vector<ChildMetrics>& children) const  {
        return panel_extent(children, true);
    }

void DialogPanelComponent::layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const  {
    if (placements.empty() || layout_.body_index >= placements.size()) return;
    const float px=std::min(padding(),std::max(0.0f,bounds.w)*.5f),py=std::min(padding(),std::max(0.0f,bounds.h)*.5f);
    const float x=bounds.x+px,y=bounds.y+py,w=std::max(0.0f,bounds.w-px*2),h=std::max(0.0f,bounds.h-py*2);
    const auto rows=action_rows(children,w,false);
    const bool title=layout_.title_index && *layout_.title_index<children.size();
    float remaining=h;
    const float actions_h=std::min(action_rows_height(rows),remaining);remaining-=actions_h;
    const float ag=rows.empty()?0:std::min(section_gap(),remaining);remaining-=ag;
    const float title_h=title?std::min(children[*layout_.title_index].preferred.h,remaining):0;remaining-=title_h;
    const float tg=title?std::min(section_gap(),remaining):0;remaining-=tg;
    if(title)placements[*layout_.title_index].bounds={x,y,w,title_h};
    placements[layout_.body_index].bounds={x,y+title_h+tg,w,remaining};
    float row_y=y+h-actions_h,available=actions_h;
    for(const auto& row:rows){
        const float row_h=std::min(row.height,available);float row_x=x+std::max(0.0f,w-row.width);
        for(const auto index:row.indices){
            const float item_w=std::min(children[index].preferred.w,w);
            if(index<placements.size())placements[index].bounds={row_x,row_y,item_w,row_h};
            row_x+=item_w+action_gap();
        }
        available-=row_h;const float gap=std::min(action_gap(),available);available-=gap;row_y+=row_h+gap;
    }
}

EventResult DialogPanelComponent::input(const InputEvent& event, InputContext& context)  {
        // Enter reaches this ancestor only after the focused descendant has
        // returned Ignored. TextInput/TextArea and any other child that owns
        // Enter therefore win before the Default action fallback.
        if (event.type == InputType::KeyDown && event.key == Key::Enter && on_default_) {
            auto callback = on_default_;
            if (InputMutationAccess::action_allowed(context)) callback();
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }

void DialogPanelComponent::paint(PaintContext& context) const  {
    const auto& theme=current_theme();const auto radius=style_.radius.value_or(theme.radii.medium);
    context.painter().fill_rounded_rect(context.bounds(),radius,style_.background.value_or(theme.palette.surface));
    context.painter().stroke_rounded_rect(context.bounds(),radius,style_.border_width.value_or(theme.controls.border_width),style_.border.value_or(theme.palette.border));
}

Constraints DialogPanelComponent::outer_constraints(const Constraints& viewport) const noexcept {
    const float width = style_.width.value_or(kDialogMaximumWidth);
    const float margin = style_.viewport_margin.value_or(kDialogViewportMargin);
    return Constraints::loose({viewport.bounded_width() ? std::min(width,std::max(0.0f,viewport.max.w-margin*2)) : width,
                              viewport.bounded_height() ? std::max(0.0f,viewport.max.h-margin*2) : kUnboundedExtent});
}

float DialogPanelComponent::action_row_width(
        const std::vector<ChildMetrics>& children, bool minimum) const {
        float width = 0.0f;
        std::size_t count = 0;
        for (const auto index : layout_.action_indices) {
            if (index >= children.size()) continue;
            if (count++) width += action_gap();
            width += minimum ? children[index].minimum.w : children[index].preferred.w;
        }
        return width;
    }

float DialogPanelComponent::action_row_height(
        const std::vector<ChildMetrics>& children, bool minimum) const noexcept {
        float height = 0.0f;
        for (const auto index : layout_.action_indices) {
            if (index >= children.size()) continue;
            height = std::max(
                height, minimum ? children[index].minimum.h : children[index].preferred.h);
        }
        return height;
    }

Size DialogPanelComponent::panel_extent(
        const std::vector<ChildMetrics>& children, bool minimum) const {
        const auto extent = [&](std::size_t index) -> Size {
            if (index >= children.size()) return {};
            return minimum ? children[index].minimum : children[index].preferred;
        };

        const auto body = extent(layout_.body_index);
        const auto title = layout_.title_index ? extent(*layout_.title_index) : Size{};
        const float actions_w = action_row_width(children, minimum);
        const float actions_h = action_row_height(children, minimum);
        const float sections = (layout_.title_index ? 1.0f : 0.0f) +
                               (!layout_.action_indices.empty() ? 1.0f : 0.0f);

        return Size{
            std::max({body.w, title.w, actions_w}) + padding() * 2.0f,
            body.h + title.h + actions_h + sections * section_gap() +
                padding() * 2.0f};
    }


float DialogPanelComponent::padding() const noexcept { return style_.padding.value_or(kDialogPadding); }
float DialogPanelComponent::section_gap() const noexcept { return style_.section_gap.value_or(kDialogSectionGap); }
float DialogPanelComponent::action_gap() const noexcept { return style_.action_gap.value_or(kDialogActionGap); }
std::vector<DialogPanelComponent::ActionRow> DialogPanelComponent::action_rows(const std::vector<ChildMetrics>& children,float width,bool minimum) const {
    std::vector<ActionRow> rows;
    for(const auto index:layout_.action_indices){
        if(index>=children.size())continue;
        const auto size=minimum?children[index].minimum:children[index].preferred;
        const float item_w=std::min(size.w,width);
        if(rows.empty() || (!rows.back().indices.empty() && rows.back().width+action_gap()+item_w>width))rows.emplace_back();
        auto& row=rows.back();if(!row.indices.empty())row.width+=action_gap();row.indices.push_back(index);row.width+=item_w;row.height=std::max(row.height,size.h);
    }
    return rows;
}
float DialogPanelComponent::action_rows_height(const std::vector<ActionRow>& rows) const noexcept {
    double height=0;for(const auto& row:rows){if(height>0)height+=action_gap();height+=row.height;}return detail::saturating_extent(height);
}
SemanticInfo DialogPanelComponent::semantics() const {
    SemanticInfo info;info.role=SemanticRole::Dialog;info.name=title_;info.description=description_;info.focusable=true;info.enabled=effective_enabled();return info;
}

} // namespace ui::detail

namespace ui {

Dialog::Dialog(UI& ui)
        : ui_(&ui), state_(ui.dialog_state_), lifetime_(std::make_shared<int>(0)) {}

Dialog::~Dialog() noexcept {
        // Invalidate retained callbacks first. Normal close stays retryable for
        // ordinary callers, but a dying controller has no legal retry owner:
        // contain failures and force the exact overlay/slot terminal before any
        // application completion can escape this destructor.
        lifetime_.reset();
        if (!active()) {
            clear_local_state();
            return;
        }

        try {
            if (close()) return;
        } catch (...) {
        }
        finish_destructor_close_noexcept();
    }

DialogShowResult Dialog::show(DialogSpec spec, Completion completion) {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_) return DialogShowResult::Unavailable;
        if (generation_ != 0 || state->active_generation != 0) return DialogShowResult::Busy;
        if (!valid_spec(spec)) return DialogShowResult::InvalidSpec;

        // Build every allocation-capable piece of local policy before acquiring
        // the per-UI Dialog slot. A construction failure therefore cannot strand
        // the UI in Busy with no visible overlay. Only show_overlay() remains
        // after acquire and is rolled back explicitly if it throws.
        const auto escape_result = escape_result_for(spec);
        OverlaySpec overlay;
        overlay.mode = OverlayMode::Modal;
        overlay.pointer_policy = OverlayPointerPolicy::Normal;
        overlay.placement = OverlayPlacement::Center;
        // T063 owns Escape through DialogState/UI before generic T061 routing.
        overlay.dismiss_on_escape = false;
        overlay.dismiss_on_outside_pointer_down = false;
        overlay.content = build_content(std::move(spec));
        auto escape_handler = guarded_completion(escape_result);
        auto deactivate_handler = guarded_abandon();

        const auto generation = state->acquire();
        if (generation == 0) return DialogShowResult::Unavailable;

        generation_ = generation;
        completion_ = std::move(completion);
        if (!state->bind_handlers(
                generation,
                std::move(escape_handler),
                std::move(deactivate_handler))) {
            (void)state->release(generation);
            clear_local_state();
            return DialogShowResult::Unavailable;
        }

        OverlayHandle handle;
        try {
            handle = ui_->show_overlay(std::move(overlay));
        } catch (...) {
            (void)state->release(generation);
            clear_local_state();
            throw;
        }
        if (!handle.valid()) {
            (void)state->release(generation);
            clear_local_state();
            return DialogShowResult::Unavailable;
        }

        overlay_ = handle;
        return DialogShowResult::Shown;
    }

DialogShowResult Dialog::show_alert(AlertDialogSpec spec, Completion completion) {
    DialogSpec request;request.title=std::move(spec.title);request.description=spec.message;
    request.body=Label{std::move(spec.message)}.spec();request.actions=std::move(spec.actions);
    return show(std::move(request),std::move(completion));
}

bool Dialog::active() const noexcept {
        if (generation_ == 0) return false;
        const auto state = state_.lock();
        return state && !state->ui_tearing_down && state->owns(generation_);
    }

bool Dialog::close() {
        return complete(DialogResult{DialogResultKind::Dismissed, {}});
    }

bool Dialog::valid_spec(const DialogSpec& spec) {
        if (!spec.body.factory) return false;
        if(spec.style){
            const auto& s=*spec.style;
            for(const auto value:{s.width,s.viewport_margin,s.padding,s.section_gap,s.action_gap,s.radius,s.border_width})
                if(value && (!std::isfinite(*value) || *value<0))return false;
            if(s.width && *s.width==0)return false;
        }

        std::unordered_set<DialogActionId> ids;
        bool default_seen = false;
        bool cancel_seen = false;
        for (const auto& action : spec.actions) {
            if (action.id.empty() || !ids.insert(action.id).second) return false;
            switch (action.role) {
                case DialogActionRole::Normal:
                    break;
                case DialogActionRole::Default:
                    if (default_seen) return false;
                    default_seen = true;
                    break;
                case DialogActionRole::Cancel:
                    if (cancel_seen) return false;
                    cancel_seen = true;
                    break;
            }
        }
        return true;
    }

DialogResult Dialog::escape_result_for(const DialogSpec& spec) {
        for (const auto& action : spec.actions) {
            if (action.role == DialogActionRole::Cancel && action.enabled) {
                return DialogResult{DialogResultKind::Action, action.id};
            }
        }
        return DialogResult{DialogResultKind::Dismissed, {}};
    }

std::function<void()> Dialog::guarded_completion(DialogResult result) {
        std::weak_ptr<int> lifetime = lifetime_;
        auto* self = this;
        return [lifetime = std::move(lifetime), self, result = std::move(result)]() mutable {
            if (lifetime.expired()) return;
            (void)self->complete(std::move(result));
        };
    }

std::function<void()> Dialog::guarded_abandon() {
        std::weak_ptr<int> lifetime = lifetime_;
        auto* self = this;
        return [lifetime = std::move(lifetime), self] {
            if (lifetime.expired()) return;
            self->abandon_without_completion();
        };
    }

Spec Dialog::action_spec(const DialogAction& action) {
        auto button = make_spec(Button{action.label,guarded_completion(DialogResult{DialogResultKind::Action,action.id})}
            .variant(action.role==DialogActionRole::Default?ButtonVariant::Primary:ButtonVariant::Standard));
        std::vector<Spec> children;
        children.push_back(std::move(button));
        return Spec{
            [enabled = action.enabled] {
                return std::make_unique<detail::DialogFixedEnabledComponent>(enabled);
            },
            std::move(children)};
    }

Spec Dialog::build_content(DialogSpec spec) {
        std::optional<std::size_t> default_action;
        std::optional<std::size_t> enabled_default;
        for (std::size_t i = 0; i < spec.actions.size(); ++i) {
            if (spec.actions[i].role == DialogActionRole::Default) {
                default_action = i;
                if (spec.actions[i].enabled) enabled_default = i;
            }
        }

        std::vector<Spec> action_specs;
        action_specs.reserve(spec.actions.size());
        for (const auto& action : spec.actions) action_specs.push_back(action_spec(action));

        std::vector<Spec> children;
        std::vector<std::size_t> action_child_indices(spec.actions.size());
        std::vector<bool> action_moved(spec.actions.size(), false);

        // T061's modal focus scope enters its first available descendant. Put
        // the configured Default action first in retained order so the nested
        // Dialog scope enters it deterministically; layout still renders
        // actions in their original visual row order at the bottom.
        if (default_action) {
            action_child_indices[*default_action] = children.size();
            children.push_back(std::move(action_specs[*default_action]));
            action_moved[*default_action] = true;
        }

        auto body_scroll = std::make_shared<ScrollState>(ScrollAxis::Vertical);
        const auto body_index = children.size();
        children.push_back(make_spec(ScrollView{
            *body_scroll, detail::DialogSpecValue{std::move(spec.body)}}));

        const auto semantic_title=spec.title;
        std::optional<std::size_t> title_index;
        if (!spec.title.empty()) {
            title_index = children.size();
            children.push_back(make_spec(
                Label{std::move(spec.title)}.size(18.0f).bold()));
        }

        for (std::size_t i = 0; i < action_specs.size(); ++i) {
            if (action_moved[i]) continue;
            action_child_indices[i] = children.size();
            children.push_back(std::move(action_specs[i]));
        }

        detail::DialogPanelLayout layout;
        layout.body_index = body_index;
        layout.title_index = title_index;
        layout.action_indices.reserve(action_child_indices.size());
        for (const auto index : action_child_indices) layout.action_indices.push_back(index);

        std::function<void()> on_default;
        if (enabled_default) {
            on_default = guarded_completion(DialogResult{
                DialogResultKind::Action, spec.actions[*enabled_default].id});
        }

        Spec panel{
            [layout = std::move(layout),
             body_scroll = std::move(body_scroll),
             on_default = std::move(on_default),style=spec.style.value_or(DialogStyle{}),
             title=semantic_title,description=std::move(spec.description)] {
                return std::make_unique<detail::DialogPanelComponent>(
                    layout,body_scroll,on_default,style,title,description);
            },
            std::move(children)};

        std::vector<Spec> backdrop_children;
        backdrop_children.push_back(std::move(panel));
        return Spec{
            [color = spec.backdrop_color] {
                return std::make_unique<detail::DialogBackdropComponent>(color);
            },
            std::move(backdrop_children)};
    }

bool Dialog::close_transferred(
        const detail::DialogState& state, std::uint64_t generation) noexcept {
        return !state.owns(generation) || state.pending_completion_generation == generation;
    }

bool Dialog::complete(DialogResult result) {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_ || generation_ == 0 ||
            !state->owns(generation_)) {
            return false;
        }

        // Preserve the first requested result across every retry. Once an
        // action has requested completion, a later repair close must not turn
        // that result into Dismissed.
        if (!pending_result_) pending_result_.emplace(std::move(result));
        const auto generation = generation_;

        // A previously accepted close can still be waiting at UI's retained
        // checkpoint if an unrelated exception escaped the outer dispatch.
        // Resume that exact transaction rather than publishing a second
        // completion or replacing the original result.
        if (state->pending_completion_generation == generation &&
            state->pending_completion) {
            if (ui_->tree_.dispatch_depth_ != 0) return true;
            ui_->flush_pending_dialog_completion();
            return !state->owns(generation);
        }

        // Copy every allocation-capable application/handler object before the
        // structural commit point. Failure here leaves the live Dialog intact.
        auto completion = completion_;
        auto completion_result = *pending_result_;

        if (ui_->tree_.dispatch_depth_ != 0) {
            // Closing the logical overlay from inside Tree::dispatch can make
            // the handle stale before T058 reaches its retained safe point. If
            // that later checkpoint throws, no controller remains capable of
            // repair. Defer the *whole* Dialog close transaction instead.
            auto escape_handler = state->escape_handler;
            auto deactivate_handler = state->deactivate_handler;
            auto overlay = overlay_;
            std::weak_ptr<detail::DialogState> weak_state = state_;
            std::weak_ptr<int> lifetime = lifetime_;
            auto* self = this;
            auto* ui = ui_;

            ui_->complete_dialog_close(
                generation,
                [weak_state = std::move(weak_state),
                 lifetime = std::move(lifetime),
                 self,
                 ui,
                 generation,
                 overlay = std::move(overlay),
                 escape_handler = std::move(escape_handler),
                 deactivate_handler = std::move(deactivate_handler),
                 completion = std::move(completion),
                 result = std::move(completion_result)]() mutable {
                    auto state = weak_state.lock();
                    if (!state || state->ui_tearing_down) return;

                    // UI::flush_pending_dialog_completion() releases the slot
                    // immediately before invoking this closure. There is no
                    // callback gap between those two operations, so restore the
                    // same generation/handlers before the first fallible close
                    // step. A failure then leaves one coherent retry owner.
                    if (state->active_generation != 0) return;
                    state->active_generation = generation;
                    state->escape_handler = std::move(escape_handler);
                    state->deactivate_handler = std::move(deactivate_handler);

                    (void)ui->overlay_state_->close_reconciled(
                        overlay, [ui] { ui->prepare_overlay_layout(); });

                    if (!state->release(generation)) return;
                    if (!lifetime.expired() && self->generation_ == generation) {
                        self->clear_local_state();
                    }
                    if (completion) completion(std::move(result));
                });

            return state->pending_completion_generation == generation &&
                   static_cast<bool>(state->pending_completion);
        }

        // Outside retained dispatch, keep the logical entry/lifetime published
        // until retained reconciliation itself reaches the same safe checkpoint.
        // Generic close_overlay() commits removal before that checkpoint and can
        // therefore lose the only repair handle if reconciliation throws.
        if (!ui_->overlay_state_->close_reconciled(
                overlay_, [ui = ui_] { ui->prepare_overlay_layout(); })) {
            return false;
        }

        // The overlay checkpoint succeeded. Make both the UI slot and this
        // controller terminal before application code begins: the completion is
        // allowed to synchronously destroy this Dialog (or the owning UI), so
        // no member access is legal after invoking it.
        if (!state->release(generation)) {
            if (!close_transferred(*state, generation)) return false;
            clear_local_state();
            return true;
        }
        clear_local_state();
        if (completion) completion(std::move(completion_result));
        return true;
    }

void Dialog::abandon_without_completion() {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_ || generation_ == 0 ||
            !state->owns(generation_)) {
            clear_local_state();
            return;
        }

        const auto generation = generation_;
        // Deactivation owns no application completion, but it still follows the
        // same close commit rule: if overlay notification throws, keep the only
        // handle/generation that can repair the transaction.
        (void)ui_->close_overlay(overlay_);
        if (!state->release(generation)) return;
        clear_local_state();
    }

void Dialog::finish_destructor_close_noexcept() noexcept {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_ || generation_ == 0 ||
            !state->owns(generation_)) {
            clear_local_state();
            return;
        }

        auto completion = std::move(completion_);
        DialogResult result = pending_result_
            ? std::move(*pending_result_)
            : DialogResult{DialogResultKind::Dismissed, {}};
        const auto generation = generation_;

        (void)ui_->overlay_state_->close_noexcept(overlay_);
        try {
            ui_->prepare_overlay_layout();
        } catch (...) {
        }

        const bool released = state->release(generation);
        clear_local_state();
        if (!released || !completion) return;

        try {
            completion(std::move(result));
        } catch (...) {
        }
    }

void Dialog::clear_local_state() noexcept {
        generation_ = 0;
        overlay_ = {};
        completion_ = {};
        pending_result_.reset();
    }

} // namespace ui
