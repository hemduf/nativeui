#include <cmath>
#include <limits>
#include <nativeui/breadcrumbs.hpp>
#include "detail/widget_menu_popup.hpp"
#include <nativeui/detail/dynamic_source.hpp>
#include <unordered_set>
namespace ui {
namespace {
using Path = std::vector<BreadcrumbItem>;
constexpr auto absent = std::numeric_limits<std::size_t>::max();
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
bool valid_path(const Path &path) {
  std::unordered_set<std::string> keys;
  for (const auto &item : path)
    if (item.key.empty() || !keys.insert(item.key).second)
      return false;
  return true;
}
TextStyle text_style(const BreadcrumbsStyle &style, Color color = {}) {
  TextStyle text;
  text.size = logical(style.text_size);
  text.family = style.font_family;
  text.color = color;
  return text;
}
std::string ellipsis(std::string label, float width, const TextStyle &style) {
  if (TextService::measure(label, style).width <= width)
    return label;
  const std::string dots = "…";
  if (TextService::measure(dots, style).width > width)
    return {};
  while (!label.empty()) {
    std::size_t begin = label.size() - 1;
    while (begin > 0 &&
           (static_cast<unsigned char>(label[begin]) & 0xc0u) == 0x80u)
      --begin;
    label.resize(begin);
    if (TextService::measure(label + dots, style).width <= width)
      return label + dots;
  }
  return dots;
}
struct CrumbFit {
  std::shared_ptr<const Path> path;
  Rect bounds;
  std::vector<bool> visible;
  std::vector<Rect> rectangles;
  std::vector<float> separators;
  bool overflow{};
};
struct CrumbRuntime : std::enable_shared_from_this<CrumbRuntime> {
  CrumbRuntime(Binding<Path> value, BreadcrumbsStyle style,
               std::function<void(const std::string &)> navigate)
      : source(std::move(value)), style(std::move(style)),
        navigate(std::move(navigate)),
        menu_anchor(std::make_shared<detail::MenuAnchorRuntime>()) {
    auto initial = source.snapshot();
    if (!valid_path(initial))
      throw std::invalid_argument(
          "Breadcrumb keys must be unique and nonempty");
    accept(std::move(initial));
    revision = source.revision();
  }
  Binding<Path> source;
  BreadcrumbsStyle style;
  std::function<void(const std::string &)> navigate;
  std::shared_ptr<const Path> path;
  std::vector<float> intrinsic;
  std::vector<bool> visible;
  std::vector<Rect> rectangles;
  std::vector<float> separators;
  std::shared_ptr<detail::MenuAnchorRuntime> menu_anchor;
  std::shared_ptr<detail::MenuPopupSession> menu;
  std::optional<detail::OverlayComponentCommand> pending;
  std::function<void()> invalidate, layout, structure, availability;
  std::function<bool()> guard;
  std::string diagnostic;
  std::uint64_t generation{}, revision{}, menu_epoch{};
  enum Effect : unsigned {
    Close = 1, Structure = 2, Layout = 4, Availability = 8, Paint = 16
  };
  unsigned effects{};
  bool mounted{}, mutable_value{true}, overflow{}, live{}, exposing{}, retired{};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && diagnostic.empty() &&
           bool(navigate) && (!guard || guard());
  }
  std::size_t index(std::string_view key) const noexcept {
    for (std::size_t i = 0; i < path->size(); ++i)
      if ((*path)[i].key == key)
        return i;
    return absent;
  }
  bool eligible(std::string_view key) const {
    const auto i = index(key);
    return allowed() && i < path->size() && i + 1 < path->size() &&
           (*path)[i].enabled;
  }
  void accept(Path next) {
    auto owned = std::make_shared<const Path>(std::move(next));
    std::vector<float> widths;
    widths.reserve(owned->size());
    const auto font = text_style(style);
    for (const auto &item : *owned)
      widths.push_back(std::max(logical(style.minimum_item_width),
                                TextService::measure(item.label, font).width +
                                    logical(2 * style.padding)));
    std::vector<bool> shown(owned->size(), true);
    std::vector<Rect> boxes(owned->size() + 1);
    path = std::move(owned);
    intrinsic = std::move(widths);
    visible = std::move(shown);
    rectangles = std::move(boxes);
    separators.clear();
    overflow = false;
    diagnostic.clear();
  }
  void close() {
    if (!live && !menu_anchor->handle.valid() && !pending)
      return;
    ++menu_epoch;
    live = false;
    const auto old = menu_anchor->handle;
    menu_anchor->handle = {};
    pending =
        old.valid()
            ? std::optional{detail::OverlayComponentCommand::close_then_invoke(
                  old, menu_anchor->node_id, true, {})}
            : std::nullopt;
    if (invalidate)
      invalidate();
  }
  void flush_effects() {
    if (!mounted || exposing)
      return;
    const auto keep = shared_from_this();
    const auto serial = generation;
    struct ExposureGuard {
      bool &flag;
      ~ExposureGuard() { flag = false; }
    } exposure{exposing};
    exposing = true;
    const auto run = [&](unsigned bit, const std::function<void()> &handle) {
      if (!mounted || generation != serial || !(effects & bit))
        return;
      const auto callback = handle;
      effects &= ~bit;
      if (callback)
        callback();
    };
    if (effects & Close) {
      effects &= ~Close;
      close();
    }
    run(Structure, structure);
    run(Layout, layout);
    run(Availability, availability);
    run(Paint, invalidate);
  }
  void sync() {
    if (!mounted)
      return;
    if (!source.valid()) {
      if (!retired) {
        retired = true;
        effects |= Close | Availability | Paint;
      }
      flush_effects();
      return;
    }
    if (source.revision() != revision) {
      const auto current_revision = source.revision();
      auto next = source.snapshot();
      if (!mounted || source.revision() != current_revision)
        return;
      if (valid_path(next)) {
        accept(std::move(next));
        ++generation;
      } else
        diagnostic = "Invalid breadcrumb keys";
      revision = current_revision;
      // Retain every unstarted publication effect before closing an exposed
      // menu can invoke an application invalidation observer.
      effects |= Close | Structure | Layout | Availability | Paint;
    }
    if (live && !menu_anchor->handle.valid() && !pending &&
        (!menu || !menu->completion_queued)) {
      live = false;
      ++menu_epoch;
    }
    flush_effects();
  }
  std::shared_ptr<CrumbFit> fit(Rect bounds) const {
    const auto count = path->size();
    std::vector<bool> shown(count, false);
    std::vector<Rect> boxes(count + 1);
    std::vector<float> dividers;
    std::vector<std::size_t> order;
    const float pad =
        std::min(logical(style.padding), std::max(0.f, bounds.w * .5f));
    const float width = std::max(0.f, bounds.w - 2 * pad);
    const float separator = logical(style.chevron_width + 2 * style.gap);
    const float minimum = logical(style.minimum_item_width);
    const float more = logical(style.overflow_width);
    bool collapsed{};
    if (count) {
      const double minima =
          double(count) * minimum + double(count - 1) * separator;
      if (minima <= width) {
        for (std::size_t i = 0; i < count; ++i) {
          shown[i] = true;
          order.push_back(i);
        }
      } else if (count == 1) {
        shown[0] = true;
        order.push_back(0);
      } else {
        collapsed = true;
        if (count > 2 && 2 * minimum + more + 2 * separator <= width) {
          shown[0] = true;
          order.push_back(0);
        }
        order.push_back(count);
        shown[count - 1] = true;
        order.push_back(count - 1);
      }
    }
    const float spacing =
        order.empty()
            ? 0.f
            : std::min(separator, width / static_cast<float>(order.size())) *
                  static_cast<float>(order.size() - 1);
    const float available = std::max(0.f, width - spacing);
    std::vector<float> allocated(order.size());
    float minima{}, desired{};
    for (std::size_t n = 0; n < order.size(); ++n) {
      const bool menu_item = order[n] == count;
      allocated[n] = menu_item ? more : minimum;
      minima += allocated[n];
      desired += menu_item ? more : intrinsic[order[n]];
    }
    if (minima > available && minima > 0) {
      for (auto &value : allocated)
        value *= available / minima;
    } else if (desired > minima) {
      const float ratio =
          std::min(1.f, (available - minima) / (desired - minima));
      for (std::size_t n = 0; n < order.size(); ++n)
        if (order[n] < count)
          allocated[n] += (intrinsic[order[n]] - allocated[n]) * ratio;
    }
    float x = bounds.x + pad;
    const float interval =
        order.size() > 1 ? spacing / static_cast<float>(order.size() - 1) : 0.f;
    for (std::size_t n = 0; n < order.size(); ++n) {
      boxes[order[n]] = {x, bounds.y, allocated[n], bounds.h};
      x += allocated[n];
      if (n + 1 < order.size()) {
        dividers.push_back(x + interval * .5f);
        x += interval;
      }
    }
    return std::make_shared<CrumbFit>(CrumbFit{
        path, bounds, std::move(shown), std::move(boxes),
        std::move(dividers), collapsed});
  }
  void publish_fit(CrumbFit &fit) noexcept {
    const bool changed = visible != fit.visible || overflow != fit.overflow;
    visible.swap(fit.visible);
    rectangles.swap(fit.rectangles);
    separators.swap(fit.separators);
    overflow = fit.overflow;
    if (changed)
      effects |= Close | Availability;
  }
  void invoke(std::string key, std::optional<std::uint64_t> epoch,
              const std::function<bool()> &permission = {}) {
    const auto keep = shared_from_this();
    sync();
    if (epoch) {
      if (!live || menu_epoch != *epoch)
        return;
      live = false;
      *epoch = ++menu_epoch;
      menu_anchor->handle = {};
    }
    if (!eligible(key) || (permission && !permission()))
      return;
    const auto serial = generation, expected = source.revision();
    auto callback = navigate;
    if (invalidate)
      invalidate();
    if (!eligible(key) || generation != serial ||
        source.revision() != expected || (epoch && menu_epoch != *epoch) ||
        (permission && !permission()))
      return;
    if (callback)
      callback(key);
  }
  void open(InputContext &context) {
    sync();
    if (!allowed() || !overflow || live || pending)
      return;
    const auto epoch = ++menu_epoch, expected = source.revision();
    const auto owned = path;
    auto next = std::make_shared<detail::MenuPopupSession>();
    next->anchor = menu_anchor;
    next->item_style = style.overflow;
    const std::weak_ptr<CrumbRuntime> weak = shared_from_this();
    for (std::size_t i = 0; i + 1 < owned->size(); ++i)
      if (!visible[i])
        next->items.push_back(
            PopupMenuItem::action((*owned)[i].label,
                                  [weak, epoch, key = (*owned)[i].key] {
                                    if (const auto state = weak.lock())
                                      state->invoke(key, epoch);
                                  },
                                  (*owned)[i].enabled));
    next->highlighted =
        detail::first_popup_index(next->items.size(), [&](std::size_t i) {
          return next->items[i].actionable();
        });
    OverlaySpec overlay;
    overlay.anchor = menu_anchor->node_id;
    overlay.mode = OverlayMode::Modal;
    overlay.placement = OverlayPlacement::AnchorBelow;
    overlay.dismiss_on_escape = true;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content = Spec{
        [next] { return std::make_unique<detail::MenuPopupComponent>(next); },
        {}};
    context.invalidate();
    if (!allowed() || menu_epoch != epoch || source.revision() != expected ||
        !detail::InputMutationAccess::allowed(context))
      return;
    live = true;
    menu = next;
    pending = detail::OverlayComponentCommand::show(
        std::move(overlay), [weak, next, epoch](OverlayHandle handle) {
          const auto current = weak.lock();
          if (!current)
            return;
          if (!current->allowed() || current->menu_epoch != epoch ||
              !current->live) {
            if (handle.valid())
              current->pending =
                  detail::OverlayComponentCommand::close_then_invoke(
                      handle, current->menu_anchor->node_id, true, {});
            return;
          }
          current->menu_anchor->handle = handle;
          next->handle = handle;
          if (!handle.valid())
            current->live = false;
        });
  }
};
class BreadcrumbPart final : public Component, public detail::ThemeBinding {
public:
  BreadcrumbPart(std::shared_ptr<CrumbRuntime> state, std::string key,
                 bool destination, bool overflow)
      : state_(std::move(state)), key_(std::move(key)),
        destination_(destination), overflow_(overflow) {}
  bool focusable() const noexcept override { return !destination_; }
  bool clips_children() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return true; }
  ComponentAvailability local_availability() const noexcept override {
    const auto index = state_->index(key_);
    const bool shown =
        overflow_ ? state_->overflow
                  : index < state_->visible.size() && state_->visible[index];
    const bool enabled =
        destination_ || (state_->source.valid() && state_->diagnostic.empty() &&
                         bool(state_->navigate) &&
                         (overflow_ || (index < state_->path->size() &&
                                        (*state_->path)[index].enabled)));
    return {shown ? VisibilityMode::Visible : VisibilityMode::Hidden, enabled,
            false};
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto index = state_->index(key_);
    return {overflow_ ? logical(state_->style.overflow_width)
            : index < state_->intrinsic.size() ? state_->intrinsic[index]
                                               : 0.f,
            logical(state_->style.height)};
  }
  void mount(MountContext &) override { mounted_ = true; }
  void unmount(LifecycleContext &) override {
    mounted_ = false;
    stop();
  }
  void deactivate(LifecycleContext &) override {
    stop();
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    if (!focused)
      stop();
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    if (destination_ || !state->allowed()) {
      stop();
      return EventResult::Ignored;
    }
    if (state->menu_anchor->suppress_until_key_up != Key::None &&
        event.key == state->menu_anchor->suppress_until_key_up) {
      if (event.type == InputType::KeyUp)
        state->menu_anchor->suppress_until_key_up = Key::None;
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown && event.key == Key::Escape) {
      stop();
      state->close();
      context.invalidate();
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerDown)
      release_ = context.pointer_releaser();
    detail::PressActivationResult outcome;
    try {
      outcome = press_.input(event, context, true);
    } catch (...) {
      stop();
      throw;
    }
    if (event.type == InputType::PointerUp ||
        event.type == InputType::PointerCancel)
      release_ = {};
    if (!outcome.activate)
      return outcome.result;
    auto permission = detail::InputMutationAccess::guard(context);
    if (mounted_ && (!permission || permission())) {
      if (overflow_)
        state->open(context);
      else
        state->invoke(key_, {}, permission);
    }
    return outcome.result;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action != SemanticAction::Activate || destination_)
      return EventResult::Ignored;
    const auto state = state_;
    const std::string key = key_;
    auto permission = detail::InputMutationAccess::guard(context);
    stop();
    if (mounted_ && (!permission || permission())) {
      if (overflow_)
        state->open(context);
      else
        state->invoke(key, {}, permission);
    }
    return EventResult::Handled;
  }
  SemanticInfo semantics() const override {
    const auto availability = local_availability();
    SemanticInfo info;
    if (availability.visibility != VisibilityMode::Visible)
      return info;
    const auto owned = state_->path;
    const auto index = state_->index(key_);
    info.role = destination_ ? SemanticRole::Text : SemanticRole::Button;
    info.name = overflow_               ? "More ancestors"
                : index < owned->size() ? (*owned)[index].label
                                        : "";
    info.text_value = overflow_ ? "…" : info.name;
    info.focusable = !destination_;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
    if (!destination_ && info.enabled)
      info.actions = {SemanticAction::Focus};
    if (!destination_ && info.enabled && !info.read_only)
      info.actions.push_back(SemanticAction::Activate);
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto state = state_;
    const auto owned = state->path;
    const auto index = state->index(key_);
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds);
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = effective_read_only();
    visual.focused = focused_;
    visual.hovered = press_.hovered();
    visual.pressed = press_.pressed();
    const auto button = resolve_button_style(
        default_button_style(current_theme()), state->style.item, visual);
    if (!destination_) {
      p.fill_rounded_rect(bounds, button.corner_radius, button.fill);
      p.stroke_rounded_rect(bounds, button.corner_radius, button.border_width,
                            button.border);
    }
    const auto color =
        destination_
            ? state->style.destination.value_or(current_theme().palette.accent)
            : state->style.text.value_or(
                  effective_enabled() ? current_theme().palette.text
                                      : current_theme().palette.disabled);
    const auto font = text_style(state->style, color);
    const std::string label = overflow_               ? "…"
                              : index < owned->size() ? (*owned)[index].label
                                                      : "";
    const auto shown = ellipsis(
        label, std::max(0.f, bounds.w - logical(2 * state->style.padding)),
        font);
    p.text(
        {bounds.x + logical(state->style.padding), bounds.y + bounds.h * .5f},
        shown, font);
  }

private:
  void stop() noexcept {
    press_ = {};
    auto release = std::move(release_);
    if (release)
      try {
        release();
      } catch (...) {
      }
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    if (!after.interactive() || after.read_only)
      stop();
  }
  std::shared_ptr<CrumbRuntime> state_;
  std::string key_;
  bool destination_{}, overflow_{}, mounted_{}, focused_{};
  detail::PressActivationState press_;
  std::function<void()> release_;
};
class BreadcrumbFrame final : public Component,
                              public detail::ThemeBinding,
                              public detail::DynamicChildrenSource,
                              public detail::OverlayCommandSource,
                              public detail::OverlayAnchorPolicy {
public:
  BreadcrumbFrame(Binding<Path> path,
                  std::function<void(const std::string &)> callback,
                  std::string label, BreadcrumbsStyle style)
      : state_(std::make_shared<CrumbRuntime>(std::move(path), std::move(style),
                                              std::move(callback))),
        label_(std::move(label)) {}
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool clips_children() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override { return state_->live; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    double width = 2 * state_->style.padding;
    for (auto value : state_->intrinsic)
      width += value;
    if (!state_->path->empty())
      width += (state_->path->size() - 1) *
               (state_->style.chevron_width + 2 * state_->style.gap);
    return {logical(width), logical(state_->style.height)};
  }
  std::vector<std::string> desired_keys() const override {
    std::vector<std::string> keys;
    for (std::size_t i = 0; i < state_->path->size(); ++i)
      keys.push_back(
          (i + 1 == state_->path->size() ? "destination:" : "ancestor:") +
          (*state_->path)[i].key);
    keys.push_back("overflow");
    return keys;
  }
  std::vector<detail::DynamicChildSpec> desired_children() const override {
    std::vector<detail::DynamicChildSpec> children;
    const auto keys = desired_keys();
    for (std::size_t i = 0; i < state_->path->size(); ++i)
      children.push_back(
          {keys[i], Spec{[state = state_, key = (*state_->path)[i].key,
                          destination = i + 1 == state_->path->size()] {
                           return std::make_unique<BreadcrumbPart>(
                               state, key, destination, false);
                         },
                         {}}});
    children.push_back(
        {"overflow", Spec{[state = state_] {
                            return std::make_unique<BreadcrumbPart>(
                                state, "", false, true);
                          },
                          {}}});
    return children;
  }
  void set_structure_invalidator(std::function<void()> callback) override {
    state_->structure = std::move(callback);
  }
  std::vector<Spec> initial_children() const {
    auto desired = desired_children();
    std::vector<Spec> children;
    children.reserve(desired.size());
    for (auto &child : desired) {
      child.spec.retained_key = child.key;
      children.push_back(std::move(child.spec));
    }
    return children;
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &children) const override {
    auto prepared = state_->fit(bounds);
    for (std::size_t i = 0;
         i < children.size() && i < prepared->rectangles.size(); ++i)
      children[i].bounds = prepared->rectangles[i];
    candidate_fit_ = std::move(prepared);
  }
  void layout_committed(Rect, Rect bounds) noexcept override {
    const auto fit = std::exchange(candidate_fit_, {});
    if (!fit || fit->path != state_->path ||
        fit->bounds.x != bounds.x || fit->bounds.y != bounds.y ||
        fit->bounds.w != bounds.w || fit->bounds.h != bounds.h)
      return;
    state_->publish_fit(*fit);
  }
  void activate(LifecycleContext &) override { state_->sync(); }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->menu_anchor->mounted = true;
    state_->menu_anchor->node_id = context.node_id();
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    state_->layout = context.layout_invalidator();
    state_->availability = context.availability_invalidator();
    const std::weak_ptr<CrumbRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    state_->menu_anchor->mounted = false;
    ++state_->generation;
    ++state_->menu_epoch;
    state_->live = false;
    state_->pending.reset();
    state_->effects = 0;
    candidate_fit_.reset();
    state_->menu_anchor->handle = {};
    state_->invalidate = {};
    state_->layout = {};
    state_->structure = {};
    state_->availability = {};
    state_->guard = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &) override { state_->close(); }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return std::exchange(state_->pending, {});
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = label_;
    info.description = state_->diagnostic;
    return info;
  }
  void paint(PaintContext &context) const override {
    auto &p = context.painter();
    const auto bounds = context.bounds();
    auto clip = p.scoped_clip(bounds);
    const auto color =
        state_->style.separator.value_or(current_theme().palette.muted_text);
    for (auto x : state_->separators)
      p.text({x, bounds.y + bounds.h * .5f}, "›",
             logical(state_->style.text_size), color, TextAlign::Center);
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    if (after.visibility == VisibilityMode::Collapsed)
      candidate_fit_.reset();
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value)
      state_->live = false;
  }
  std::shared_ptr<CrumbRuntime> state_;
  mutable std::shared_ptr<CrumbFit> candidate_fit_;
  std::string label_;
  Binding<Path>::Subscription subscription_;
};
} // namespace
Breadcrumbs::Breadcrumbs(Binding<std::vector<BreadcrumbItem>> path)
    : path_(std::move(path)) {}
Breadcrumbs::Breadcrumbs(State<std::vector<BreadcrumbItem>> &path)
    : Breadcrumbs(path.binding()) {}
Breadcrumbs &&
Breadcrumbs::on_navigate(std::function<void(const std::string &)> value) && {
  navigate_ = std::move(value);
  return std::move(*this);
}
Breadcrumbs &&Breadcrumbs::label(std::string value) && {
  label_ = std::move(value);
  return std::move(*this);
}
Breadcrumbs &&Breadcrumbs::style(BreadcrumbsStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec Breadcrumbs::spec() && {
  if (!valid_path(path_.snapshot()))
    throw std::invalid_argument("Breadcrumb keys must be unique and nonempty");
  for (double metric :
       {style_.text_size, style_.minimum_item_width, style_.overflow_width,
        style_.height, style_.padding, style_.gap, style_.chevron_width})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "Breadcrumb metrics must be finite and nonnegative");
  Spec result{
      [path = path_, navigate = navigate_, label = label_, style = style_] {
        return std::make_unique<BreadcrumbFrame>(path, navigate, label, style);
      },
      {}};
  result.children_factory = [](Component &component) {
    return static_cast<BreadcrumbFrame &>(component).initial_children();
  };
  return result;
}
} // namespace ui
