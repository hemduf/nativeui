#include <nativeui/tooltip.hpp>
#include <cmath>

namespace ui::detail {
namespace {
float extent(float value, float fallback) noexcept {
    return std::isfinite(value) && value >= 0 ? value : fallback;
}
}

TooltipController::TooltipController(Dispatcher dispatcher,
                      DispatcherDuration delay,
                      Callback show,
                      Callback hide)
        : state_(std::make_shared<State>(
              std::move(dispatcher),
              delay.count() < 0.0 ? DispatcherDuration::zero() : delay,
              std::move(show),
              std::move(hide))) {}

TooltipController::~TooltipController() noexcept { shutdown(); }

void TooltipController::set_hovered(bool hovered) { set_trigger(Trigger::Hover, hovered); }

void TooltipController::set_focused(bool focused) { set_trigger(Trigger::Focus, focused); }

void TooltipController::set_pointer_interaction_active(bool active) {
        if (!state_ || state_->shutting_down ||
            state_->pointer_interaction_active == active) {
            return;
        }

        state_->pointer_interaction_active = active;
        if (active) {
            state_->suppressed = true;
            cancel_pending(*state_);
            hide_visible(*state_);
        }
    }

void TooltipController::set_anchor_available(bool available) {
        if (!state_ || state_->shutting_down || state_->anchor_available == available) {
            return;
        }

        state_->anchor_available = available;
        if (!available) {
            state_->suppressed = true;
            cancel_pending(*state_);
            hide_visible(*state_);
        }
    }

void TooltipController::notify_presentation_closed() noexcept {
        if (!state_ || !state_->visible) return;
        state_->visible = false;
    }

void TooltipController::dismiss_until_eligibility_transition() {
        if (!state_) return;
        state_->suppressed = true;
        cancel_pending(*state_);
        hide_visible(*state_);
    }

void TooltipController::cancel() {
        if (!state_) return;
        cancel_pending(*state_);
        hide_visible(*state_);
    }

bool TooltipController::pending() const noexcept {
        return state_ && state_->timer.valid();
    }

bool TooltipController::visible() const noexcept {
        return state_ && state_->visible;
    }

bool TooltipController::eligible() const noexcept {
        return state_ && state_->eligible();
    }

void TooltipController::cancel_pending(State& state) {
        if (!state.timer.valid()) return;
        (void)state.dispatcher.cancel(state.timer);
        state.timer = {};
    }

void TooltipController::hide_visible(State& state) {
        if (!state.visible) return;

        // Publish the hidden state before crossing the integration callback so
        // a reentrant dismissal cannot observe a half-hidden controller. If the
        // overlay close itself fails, roll back the exact publication and keep
        // the only state that can drive a later retry.
        auto hide = state.hide;
        state.visible = false;
        try {
            if (hide) hide();
        } catch (...) {
            state.visible = true;
            throw;
        }
    }

void TooltipController::arm(const std::shared_ptr<State>& state) {
        if (!state || state->shutting_down || state->suppressed ||
            !state->eligible() || state->visible || state->timer.valid()) {
            return;
        }

        std::weak_ptr<State> weak = state;
        state->timer = state->dispatcher.schedule_after(state->delay, [weak] {
            const auto locked = weak.lock();
            if (!locked) return;

            locked->timer = {};
            if (locked->shutting_down || locked->suppressed ||
                !locked->eligible() || locked->visible) {
                return;
            }

            // Publish visibility before integration code runs so a reentrant
            // dismissal cannot observe a half-shown controller. A failed
            // OverlayState transaction never commits a usable handle, so roll
            // this flag back before propagating and allow a future eligibility
            // transition to retry normally.
            auto show = locked->show;
            locked->visible = true;
            try {
                if (show) show();
            } catch (...) {
                locked->visible = false;
                throw;
            }
        });
    }

void TooltipController::set_trigger(Trigger trigger, bool value) {
        const auto state = state_;
        if (!state || state->shutting_down) return;

        const bool was_eligible = state->eligible();
        bool& field = trigger == Trigger::Hover ? state->hovered : state->focused;
        if (field == value) return;
        field = value;
        const bool is_eligible = state->eligible();

        if (!is_eligible) {
            cancel_pending(*state);
            hide_visible(*state);
            // Only the retained hover/focus state reaching false clears
            // suppression. Availability restoration itself is deliberately not
            // a new presentation trigger, and neither is the end of a pointer
            // button interaction.
            const auto current_state = state_;
        if (!current_state->triggered() && !current_state->pointer_interaction_active) {
                current_state->suppressed = false;
            }
            return;
        }

        if (!was_eligible && is_eligible) {
            state->suppressed = false;
            arm(state);
        }
    }

void TooltipController::shutdown() noexcept {
        if (!state_) return;
        state_->shutting_down = true;
        cancel_pending(*state_);
        try {
            hide_visible(*state_);
        } catch (...) {
            // Destruction/unmount is a no-unwind boundary. OverlayState owns
            // the authoritative structural lifetime; a failed best-effort hide
            // must never terminate the process while the controller is dying.
        }
        state_->show = {};
        state_->hide = {};
        state_.reset();
    }

TooltipSurfaceComponent::TooltipSurfaceComponent(std::string text, float max_width)
        : text_(std::move(text)), max_width_(extent(max_width,kTooltipDefaultMaxWidth)) {}
TooltipSurfaceComponent::TooltipSurfaceComponent(std::string text, TooltipStyle style)
        : text_(std::move(text)), max_width_(extent(style.max_width.value_or(kTooltipDefaultMaxWidth),kTooltipDefaultMaxWidth)),style_(std::move(style)) {}

Size TooltipSurfaceComponent::measure(const std::vector<ChildMetrics>&) const  {
        const auto style = text_style();
        const float content_max = std::max(1.0f, max_width_ - padding() * 2.0f);
        const auto lines = wrap_tooltip_text(text_, style, content_max);
        float width = 0.0f;
        for (const auto& line : lines) {
            width = std::max(width, TextService::measure(line, style).width);
        }
        const float line_height = TextService::measure("Ag", style).height;
        return Size{
            width + padding() * 2.0f,
            static_cast<float>(lines.size()) * line_height + padding() * 2.0f};
    }

void TooltipSurfaceComponent::paint(PaintContext& context) const  {
        const auto bounds = context.bounds();
        const auto style = text_style();
        auto& painter = context.painter();

        painter.fill_rounded_rect(bounds, radius(), background());
        if (border_width() > 0.0f) {
            painter.stroke_rounded_rect(bounds, radius(), border_width(), border());
        }

        const float inset = padding();
        const float content_max = std::max(1.0f, max_width_ - inset * 2.0f);
        const auto lines = wrap_tooltip_text(text_, style, content_max);
        const float line_height = TextService::measure("Ag", style).height;
        TextStyle line_style = style;
        line_style.align = TextAlign::Left;
        float center_y = bounds.y + inset + line_height * 0.5f;
        for (const auto& line : lines) {
            painter.text({bounds.x + inset, center_y}, line, line_style);
            center_y += line_height;
        }
    }

TextStyle TooltipSurfaceComponent::text_style() const {
        const auto& theme = current_theme();
        TextStyle style = style_.text_style.value_or(TextStyle{});
        if (!style_.text_style) style.size = theme.typography.control_size;
        style.color = style_.text.value_or(style_.text_style ? style.color : theme.palette.text);
        return style;
    }

Color TooltipSurfaceComponent::background() const { return style_.background.value_or(current_theme().palette.surface); }

Color TooltipSurfaceComponent::border() const { return style_.border.value_or(current_theme().palette.border); }

float TooltipSurfaceComponent::border_width() const { return extent(style_.border_width.value_or(current_theme().controls.border_width),current_theme().controls.border_width); }

float TooltipSurfaceComponent::radius() const { return extent(style_.radius.value_or(current_theme().radii.sm),current_theme().radii.sm); }

float TooltipSurfaceComponent::padding() const { return extent(style_.padding.value_or(current_theme().spacing.sm),current_theme().spacing.sm); }

TooltipComponent::TooltipComponent(std::string text, std::chrono::milliseconds delay)
        : TooltipComponent(std::move(text),delay,{}) {}
TooltipComponent::TooltipComponent(std::string text, std::chrono::milliseconds delay, TooltipStyle style)
        : text_(std::move(text)),delay_(delay.count() < 0 ? std::chrono::milliseconds{0} : delay),style_(std::move(style)) {}

Size TooltipComponent::measure(const std::vector<ChildMetrics>& children) const  {
        return children.empty() ? Size{} : children.front().preferred;
    }

Size TooltipComponent::minimum_size(const std::vector<ChildMetrics>& children) const  {
        return children.empty() ? Size{} : children.front().minimum;
    }

Constraints TooltipComponent::child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const  {
        return constraints;
    }

void TooltipComponent::layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const  {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

void TooltipComponent::mount(MountContext& context)  {
        node_id_ = context.node_id();
        overlay_service_ = context.overlay_service();
        layout_invalidator_ = context.layout_invalidator();
        mounted_ = true;
    }

void TooltipComponent::unmount(LifecycleContext&)  {
        mounted_ = false;
        // Retained teardown must not invalidate a dying UI or close an overlay
        // through a platform callback. Drop the borrowed seams first; T061
        // removes any still-registered anchored overlay with the UI/overlay
        // state itself.
        overlay_service_ = nullptr;
        layout_invalidator_ = {};
        controller_.reset();
        overlay_ = {};
    }

void TooltipComponent::activate(LifecycleContext&)  {
        reconcile_presentation();
        refresh_anchor_availability();
    }

void TooltipComponent::deactivate(LifecycleContext&)  {
        dismiss_transient_presentation();
        overlay_ = {};
    }

void TooltipComponent::retained_pointer_hover_changed(
        bool hovered, bool pointer_interaction_active, Dispatcher dispatcher)  {
        reconcile_presentation();
        refresh_anchor_availability();
        hovered_ = hovered;
        pointer_interaction_active_ = pointer_interaction_active;
        ensure_controller(dispatcher);
        if (controller_) {
            controller_->set_pointer_interaction_active(pointer_interaction_active_);
            controller_->set_hovered(hovered);
        }
    }

void TooltipComponent::retained_focus_within_changed(
        bool focused, bool pointer_interaction_active, Dispatcher dispatcher)  {
        reconcile_presentation();
        refresh_anchor_availability();
        focused_ = focused;
        pointer_interaction_active_ = pointer_interaction_active;
        ensure_controller(dispatcher);
        if (controller_) {
            controller_->set_pointer_interaction_active(pointer_interaction_active_);
            controller_->set_focused(focused);
        }
    }

void TooltipComponent::dismiss_transient_presentation()  {
        if (controller_) controller_->dismiss_until_eligibility_transition();
    }

SemanticInfo TooltipComponent::semantics() const  {
        SemanticInfo info;
        if (!text_.empty()) info.description = text_;
        return info;
    }

void TooltipComponent::paint(PaintContext&) const  {}

std::optional<DescendantSemanticDecoration> TooltipComponent::descendant_semantic_decoration() const {
    if (text_.empty()) return std::nullopt;
    return DescendantSemanticDecoration{{},{},text_,true};
}

bool TooltipComponent::anchor_available() const noexcept {
        return effective_availability().interactive();
    }

void TooltipComponent::refresh_anchor_availability() {
        if (!controller_) return;
        controller_->set_anchor_available(anchor_available());
    }

void TooltipComponent::ensure_controller(Dispatcher dispatcher) {
        if (controller_ || text_.empty() || !dispatcher.valid()) return;

        const auto delay = std::chrono::duration<double>(delay_);
        controller_ = std::make_shared<TooltipController>(
            dispatcher,
            delay,
            [this] { present(); },
            [this] { hide(); });
        controller_->set_anchor_available(anchor_available());
        controller_->set_pointer_interaction_active(pointer_interaction_active_);
        if (hovered_) controller_->set_hovered(true);
        if (focused_) controller_->set_focused(true);
    }

void TooltipComponent::reconcile_presentation() {
        if (controller_ && controller_->visible() && !overlay_.valid()) {
            controller_->notify_presentation_closed();
        }
    }

void TooltipComponent::present() {
        if (!mounted_ || text_.empty() || !overlay_service_) {
            if (controller_) controller_->notify_presentation_closed();
            return;
        }
        if (!anchor_available()) {
            // Availability can change while the timer is in flight. Cancel and
            // suppress stationary eligibility instead of showing for a
            // Hidden/Collapsed/Disabled anchor.
            controller_->set_anchor_available(false);
            return;
        }
        if (overlay_.valid()) return;

        OverlaySpec overlay;
        overlay.mode = OverlayMode::NonModal;
        overlay.pointer_policy = OverlayPointerPolicy::Ignore;
        overlay.anchor = node_id_;
        overlay.placement = OverlayPlacement::Auto;
        overlay.dismiss_on_escape = false;
        overlay.dismiss_on_outside_pointer_down = false;
        std::string text = text_;
        auto style = style_;
        overlay.content = Spec{
            [text = std::move(text),style=std::move(style)] {
                return std::make_unique<TooltipSurfaceComponent>(text,style);
            },
            {}};
        overlay_ = overlay_service_->present(std::move(overlay));

        // OverlayState::show() is the transaction commit point. A secondary
        // layout notification after that point must not make the controller
        // roll visibility back while a valid committed handle is retained.
        try {
            if (layout_invalidator_) layout_invalidator_();
        } catch (...) {
        }
    }

void TooltipComponent::hide() {
        if (overlay_service_ && overlay_.valid()) {
            (void)overlay_service_->dismiss(overlay_);
        }
        overlay_ = {};

        // As above, OverlayState::close() is the commit point. Keep any later
        // notification failure from resurrecting the controller-visible state
        // after its only overlay handle has been deterministically cleared.
        try {
            if (layout_invalidator_) layout_invalidator_();
        } catch (...) {
        }
    }

[[nodiscard]] std::size_t tooltip_utf8_sequence_length(unsigned char lead) noexcept {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;
}

[[nodiscard]] std::vector<std::string> wrap_tooltip_text(
    std::string_view text, const TextStyle& style, float max_width) {
    const float limit = max_width > 0.0f ? max_width : 1.0f;
    std::vector<std::string> lines;
    std::string current;

    auto flush_current = [&] {
        while (!current.empty() && current.back() == ' ') current.pop_back();
        lines.push_back(std::move(current));
        current.clear();
    };
    auto fits = [&](std::string_view candidate) {
        return TextService::measure(candidate, style).width <= limit;
    };

    std::size_t index = 0;
    while (index < text.size()) {
        const char lead = text[index];
        if (lead == '\n') {
            flush_current();
            ++index;
            continue;
        }
        if (lead == ' ') {
            ++index;
            continue;
        }

        const std::size_t word_start = index;
        while (index < text.size() && text[index] != ' ' && text[index] != '\n') {
            const auto length = tooltip_utf8_sequence_length(
                static_cast<unsigned char>(text[index]));
            index += std::min(length, text.size() - index);
        }
        std::string_view word = text.substr(word_start, index - word_start);

        if (!current.empty()) {
            std::string candidate = current;
            candidate.push_back(' ');
            candidate.append(word);
            if (!fits(candidate)) flush_current();
        }

        if (current.empty() && !fits(word)) {
            // Hard-break an unbreakable word on codepoint boundaries.
            while (!word.empty()) {
                std::size_t bytes = 0;
                while (bytes < word.size()) {
                    const auto length = std::min(
                        tooltip_utf8_sequence_length(static_cast<unsigned char>(word[bytes])),
                        word.size() - bytes);
                    if (bytes > 0 && !fits(word.substr(0, bytes + length))) break;
                    bytes += length;
                }
                if (bytes == 0) bytes = 1;
                if (bytes >= word.size()) {
                    current.assign(word);
                    word = {};
                    break;
                }
                lines.push_back(std::string{word.substr(0, bytes)});
                word.remove_prefix(bytes);
            }
            continue;
        }

        if (!current.empty()) current.push_back(' ');
        current.append(word);
    }

    while (!current.empty() && current.back() == ' ') current.pop_back();
    if (!current.empty() || lines.empty()) lines.push_back(std::move(current));
    return lines;
}

} // namespace ui::detail

namespace ui {
Tooltip&& Tooltip::style(TooltipStyle value) && { style_ = std::move(value); return std::move(*this); }

Tooltip&& Tooltip::delay(std::chrono::milliseconds value) && noexcept {
        set_delay(value);
        return std::move(*this);
    }

Tooltip& Tooltip::delay(std::chrono::milliseconds value) & noexcept {
        set_delay(value);
        return *this;
    }

std::chrono::milliseconds Tooltip::delay() const noexcept { return delay_; }

const std::string& Tooltip::text() const noexcept { return text_; }

Spec Tooltip::spec() && {
        auto text = std::move(text_);
        const auto delay_value = delay_;
        auto style = std::move(style_);
        return Spec{
            [text = std::move(text), delay_value,style=std::move(style)] {
                return std::make_unique<detail::TooltipComponent>(text, delay_value,style);
            },
            std::move(children_)};
    }

void Tooltip::set_delay(std::chrono::milliseconds value) noexcept {
        delay_ = value.count() < 0 ? std::chrono::milliseconds{0} : value;
    }

} // namespace ui
