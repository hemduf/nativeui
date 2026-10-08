#include "detail/widget_unicode.hpp"
#include <nativeui/link.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ui {
namespace {
void validate_metric(double value) {
  if (!std::isfinite(value) || value < 0.0)
    throw std::invalid_argument("Link metrics must be finite and nonnegative");
}
void validate_style(const LinkStyle &value) {
  validate_metric(value.focus_padding);
  validate_metric(value.focus_ring_width);
  if (value.text)
    validate_metric(value.text->size);
  for (const auto &color : {value.color, value.hovered_color,
                            value.disabled_color, value.focus_ring})
    if (color && (!std::isfinite(color->r) || !std::isfinite(color->g) ||
                  !std::isfinite(color->b) || !std::isfinite(color->a)))
      throw std::invalid_argument("Link colors must be finite");
}
float logical(double value) noexcept {
  return static_cast<float>(
      std::min(value, static_cast<double>(std::numeric_limits<float>::max())));
}
struct Line {
  std::string_view text;
  float width{};
};
std::vector<Line> lines(std::string_view text, const TextStyle &style,
                        float width, bool wrap) {
  std::vector<Line> result;
  std::size_t paragraph_start{};
  while (paragraph_start <= text.size()) {
    const auto newline = text.find('\n', paragraph_start);
    const auto paragraph_end =
        newline == std::string_view::npos ? text.size() : newline;
    auto paragraph =
        text.substr(paragraph_start, paragraph_end - paragraph_start);
    if (!paragraph.empty() && paragraph.back() == '\r')
      paragraph.remove_suffix(1);
    if (!wrap || !std::isfinite(width) || paragraph.empty())
      result.push_back(
          {paragraph, TextService::measure(paragraph, style).width});
    else {
      const auto graphemes = detail::widget_graphemes(paragraph);
      const auto breaks = detail::widget_line_breaks(paragraph);
      for (std::size_t start = 0; start < paragraph.size();) {
        std::size_t end = start;
        std::size_t break_at = start;
        for (const auto &span : graphemes) {
          if (span.begin < start)
            continue;
          const auto candidate = paragraph.substr(start, span.end - start);
          if (span.begin > start &&
              TextService::measure(candidate, style).width > width)
            break;
          end = span.end;
          if (std::binary_search(breaks.begin(), breaks.end(), end))
            break_at = end;
        }
        if (end < paragraph.size() && break_at > start)
          end = break_at;
        if (end <= start)
          throw std::runtime_error("Link wrapping made no Unicode progress");
        auto fragment = paragraph.substr(start, end - start);
        while (!fragment.empty() &&
               (fragment.back() == ' ' || fragment.back() == '\t'))
          fragment.remove_suffix(1);
        result.push_back(
            {fragment, TextService::measure(fragment, style).width});
        start = end;
        while (start < paragraph.size() &&
               (paragraph[start] == ' ' || paragraph[start] == '\t'))
          ++start;
      }
    }
    if (newline == std::string_view::npos)
      break;
    paragraph_start = newline + 1;
  }
  return result;
}
float line_height(const TextStyle &style) {
  const auto metrics = TextService::measure("Mg", style);
  return std::max(metrics.height, style.size);
}
} // namespace
namespace detail {
struct LinkInteraction {
  PressActivationState press;
  std::function<void()> release_pointer;
  std::uint64_t generation{};
  bool mounted{};
  bool allowed{true};
};
namespace {
void cancel_interaction(const std::shared_ptr<LinkInteraction> &state) {
  state->press = {};
  auto release = std::move(state->release_pointer);
  if (release)
    release();
}
} // namespace
LinkComponent::LinkComponent(std::string label, std::string destination,
                             NavigateCallback navigate, LinkStyle style,
                             bool wrap)
    : label_(widget_repair_utf8(label)), destination_(std::move(destination)),
      navigate_(navigate
                    ? std::make_shared<NavigateCallback>(std::move(navigate))
                    : nullptr),
      style_(std::move(style)), wrap_(wrap),
      interaction_(std::make_shared<LinkInteraction>()) {
  validate_style(style_);
}
bool LinkComponent::roving_focus_target() const noexcept { return true; }

bool LinkComponent::focusable() const noexcept {
  return navigate_ && !destination_.empty();
}
TextStyle LinkComponent::text_style() const {
  TextStyle text = style_.text.value_or(TextStyle{});
  if (!style_.text) {
    text.size = current_theme().typography.control_size;
    text.family = current_theme().typography.family;
    text.fallback_families = current_theme().typography.fallback_families;
    text.weight = current_theme().typography.control_weight;
    text.slant = current_theme().typography.slant;
  }
  text.color = style_.color.value_or(current_theme().palette.accent);
  if (!effective_enabled())
    text.color =
        style_.disabled_color.value_or(current_theme().palette.disabled);
  else if (interaction_->press.hovered())
    text.color = style_.hovered_color.value_or(current_theme().palette.accent);
  return text;
}
Size LinkComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto style = text_style();
  const auto content = lines(label_, style, kUnboundedExtent, false);
  float width{};
  for (const auto &line : content)
    width = std::max(width, line.width);
  const float padding = logical(style_.focus_padding * 2.0);
  return {logical(static_cast<double>(width) + padding),
          logical(static_cast<double>(content.size()) * line_height(style) +
                  padding)};
}
ChildMetrics LinkComponent::measure_constrained(
    const Constraints &constraints,
    const std::vector<ChildMetrics> &children) const {
  if (!wrap_)
    return Component::measure_constrained(constraints, children);
  const auto style = text_style();
  const float padding = logical(style_.focus_padding * 2.0);
  const float available = std::max(0.0f, constraints.max.w - padding);
  const auto content = lines(label_, style, available, true);
  float width{};
  for (const auto &line : content)
    width = std::max(width, line.width);
  const float height = line_height(style);
  return {
      constraints.constrain(
          {0.0f, logical(static_cast<double>(height) + padding)}),
      constraints.constrain(
          {logical(static_cast<double>(width) + padding),
           logical(static_cast<double>(content.size()) * height + padding)})};
}
SemanticInfo LinkComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Custom;
  info.name = label_;
  info.description = destination_;
  info.focusable = focusable();
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  if (info.enabled && info.focusable)
    info.actions = {SemanticAction::Activate, SemanticAction::Focus};
  return info;
}
void LinkComponent::mount(MountContext &) { interaction_->mounted = true; }
void LinkComponent::unmount(LifecycleContext &) {
  interaction_->mounted = false;
  ++interaction_->generation;
  try {
    cancel_interaction(interaction_);
  } catch (...) {
  }
}
void LinkComponent::deactivate(LifecycleContext &context) {
  ++interaction_->generation;
  focused_ = false;
  cancel_interaction(interaction_);
  context.invalidate();
}
void LinkComponent::focus_changed(bool focused, FocusContext &context) {
  focused_ = focused;
  if (!focused) {
    ++interaction_->generation;
    cancel_interaction(interaction_);
  }
  context.invalidate();
}
void LinkComponent::effective_availability_changed(
    const ComponentAvailability &,
    const ComponentAvailability &after) noexcept {
  interaction_->allowed =
      after.enabled && after.visibility == VisibilityMode::Visible;
  if (!interaction_->allowed) {
    ++interaction_->generation;
    try {
      cancel_interaction(interaction_);
    } catch (...) {
    }
  }
}
EventResult LinkComponent::input(const InputEvent &event,
                                 InputContext &context) {
  if (!focusable())
    return EventResult::Ignored;
  const auto state = interaction_;
  if (event.type == InputType::PointerDown) {
    state->release_pointer = context.pointer_releaser();
    ++state->generation;
  }
  const auto generation = state->generation;
  EventResult result{EventResult::Ignored};
  NavigateCallback navigate;
  std::string destination;
  try {
    const auto outcome = state->press.input(event, context, true, false);
    result = outcome.result;
    if (event.type == InputType::PointerCancel) {
      auto release = std::move(state->release_pointer);
      if (release)
        release();
    } else if (event.type == InputType::PointerUp) {
      state->release_pointer = {};
    }
    destination = outcome.activate ? destination_ : std::string{};
    // Callable copying is itself a reentrancy boundary: its target copy
    // constructor may retire this node. No component member is read afterwards.
    const auto callback_owner = outcome.activate ? navigate_ : nullptr;
    navigate = callback_owner ? *callback_owner : NavigateCallback{};
    if (!state->mounted || !state->allowed || state->generation != generation)
      return result;
    if (outcome.result == EventResult::Handled)
      context.invalidate();
  } catch (...) {
    if (state->generation == generation) {
      try {
        cancel_interaction(state);
      } catch (...) {
      }
    }
    throw;
  }
  // Navigation runs after terminal cleanup, outside the guard for preparation
  // failures. A throwing action keeps Enter latched until its real KeyUp.
  // Invalidation can remove/disable the retained link before this call.
  if (navigate && state->mounted && state->allowed &&
      state->generation == generation)
    navigate(destination);
  return result;
}
void LinkComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const auto text = text_style();
  const float padding =
      std::min(logical(style_.focus_padding), bounds.w * 0.5f);
  const float available = std::max(0.0f, bounds.w - padding * 2.0f);
  const auto content = lines(label_, text, available, wrap_);
  const float height = line_height(text);
  float y = bounds.y +
            (bounds.h - static_cast<float>(content.size()) * height) * 0.5f;
  auto &painter = context.painter();
  auto clip = painter.scoped_clip(bounds);
  for (const auto &line : content) {
    const float anchor = text.align == TextAlign::Left ? bounds.x + padding
                         : text.align == TextAlign::Center
                             ? bounds.x + bounds.w * 0.5f
                             : bounds.x + bounds.w - padding;
    painter.text({anchor, y + height * 0.5f}, line.text, text);
    if (style_.underline ||
        (style_.hovered_underline && interaction_->press.hovered())) {
      const float x = text.align == TextAlign::Left ? anchor
                      : text.align == TextAlign::Center
                          ? anchor - line.width * 0.5f
                          : anchor - line.width;
      painter.line({x, y + height - 1.0f}, {x + line.width, y + height - 1.0f},
                   1.0f, text.color);
    }
    y += height;
  }
  if (context.focused() && style_.focus_ring_width > 0.0)
    painter.stroke_rounded_rect(
        bounds, current_theme().radii.sm, logical(style_.focus_ring_width),
        style_.focus_ring.value_or(current_theme().palette.focus));
}
} // namespace detail
Link::Link(std::string label, std::string destination,
           NavigateCallback navigate)
    : label_(std::move(label)), destination_(std::move(destination)),
      navigate_(std::move(navigate)) {}
Link &&Link::style(LinkStyle value) && {
  validate_style(value);
  style_ = std::move(value);
  return std::move(*this);
}
Link &&Link::wrap(bool value) && {
  wrap_ = value;
  return std::move(*this);
}
Spec Link::spec() && {
  return {[label = std::move(label_), destination = std::move(destination_),
           navigate = std::move(navigate_), style = std::move(style_),
           wrap = wrap_] {
            return std::make_unique<detail::LinkComponent>(
                label, destination, navigate, style, wrap);
          },
          {}};
}
} // namespace ui
