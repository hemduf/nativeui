#include "detail/widget_input_action.hpp"
#include "detail/widget_search_field_policy.hpp"
#include <nativeui/find_bar.hpp>
namespace ui {
namespace {
struct FindState : std::enable_shared_from_this<FindState> {
  FindState(Binding<bool> opened, Binding<std::string> query_value,
            Binding<std::size_t> count,
            Binding<std::optional<std::size_t>> index)
      : source_open(std::move(opened)), query(std::move(query_value)),
        matches(std::move(count)), current(std::move(index)),
        seen_count(matches.get()) {}
  Binding<bool> source_open;
  Binding<std::string> query;
  Binding<std::size_t> matches;
  Binding<std::optional<std::size_t>> current;
  std::shared_ptr<detail::SearchFieldBridge> bridge;
  std::function<void(std::size_t)> navigate_callback;
  std::function<void()> invalidate, layout, availability, focus;
  std::string status, description;
  std::size_t seen_count{};
  std::uint64_t count_generation{}, action_generation{};
  bool mounted{}, open{}, mutable_value{true}, pending_selection{};
  bool valid() const noexcept {
    return source_open.valid() && query.valid() && matches.valid() &&
           current.valid();
  }
  bool can_navigate() const noexcept {
    return mounted && open && mutable_value && valid() && matches.get() > 0;
  }
  void sync() {
    if (!mounted)
      return;
    const auto count = matches.get();
    const auto index = current.get();
    std::string next_status, next_description;
    if (!query.get().empty()) {
      if (count == 0)
        next_status = "Aucun résultat";
      else {
        const auto effective = index ? std::min(*index, count - 1) : 0;
        next_status =
            std::to_string(effective + 1) + " sur " + std::to_string(count);
        if (!index || *index >= count)
          next_description = "Sélection externe absente ou invalide";
      }
    }
    const bool next_open = source_open.valid() && source_open.get();
    const bool open_changed = next_open != open;
    const bool count_changed = count != seen_count;
    const bool status_changed =
        status != next_status || description != next_description;
    if (count_changed) {
      seen_count = count;
      ++count_generation;
      ++action_generation;
    }
    status = std::move(next_status);
    description = std::move(next_description);
    if (open_changed) {
      open = next_open;
      ++action_generation;
      pending_selection = open;
    }
    if (open_changed) {
      if (availability)
        availability();
      if (focus)
        focus();
      if (layout)
        layout();
    } else if (count_changed || !valid()) {
      if (availability)
        availability();
    }
    if (status_changed && invalidate)
      invalidate();
    // FocusScope owns focus transfer/restoration. An explicit request before
    // scope activation would overwrite the previous-focus handle with the
    // field.
    if (open && pending_selection)
      if (auto editor = bridge->session.lock(); editor && editor->mounted())
        editor->select(0, query.get().size());
  }
  void navigate(int direction) {
    if (!can_navigate())
      return;
    sync();
    if (!can_navigate())
      return;
    const auto count = matches.get();
    const auto previous = current.get();
    std::size_t target{};
    if (!previous)
      target = direction > 0 ? 0 : count - 1;
    else {
      const auto index = std::min(*previous, count - 1);
      target = direction > 0 ? (index == count - 1 ? 0 : index + 1)
                             : (index == 0 ? count - 1 : index - 1);
    }
    const auto count_serial = count_generation,
               action_serial = action_generation;
    auto callback = navigate_callback;
    auto binding = current;
    if (!can_navigate() || count_generation != count_serial ||
        action_generation != action_serial)
      return;
    if (auto editor = bridge->session.lock())
      editor->cancel_capture();
    if (invalidate)
      invalidate();
    if (!can_navigate() || count_generation != count_serial ||
        action_generation != action_serial || current.get() != previous)
      return;
    binding.set(std::optional<std::size_t>{target});
    sync();
    if (can_navigate() && count_generation == count_serial &&
        action_generation == action_serial && matches.get() == count &&
        current.get() == target && callback)
      callback(target);
  }
  void close() {
    if (!mounted || !open || !source_open.valid())
      return;
    auto source = source_open;
    if (auto editor = bridge->session.lock()) {
      editor->cancel_capture();
      editor->cancel_composition();
    }
    open = false;
    pending_selection = false;
    ++action_generation;
    if (availability)
      availability();
    if (focus)
      focus();
    if (layout)
      layout();
    if (mounted && source.valid())
      source.set(false);
  }
};
class FindStatus final : public Component {
public:
  FindStatus(std::shared_ptr<FindState> state, FindBarStyle style)
      : state_(std::move(state)), style_(std::move(style)) {}
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {style_.status_width,
            TextService::measure("Aucun résultat", style_.status).height};
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Text;
    info.text_value = state_->status;
    info.description = state_->description;
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    painter.text({bounds.x, bounds.y + bounds.h * 0.5f}, state_->status,
                 style_.status);
  }

private:
  std::shared_ptr<FindState> state_;
  FindBarStyle style_;
};
class FindComponent final : public Component, public detail::ThemeBinding {
public:
  FindComponent(Binding<bool> open, Binding<std::string> query,
                Binding<std::size_t> matches,
                Binding<std::optional<std::size_t>> current, std::string label,
                std::function<void(std::size_t)> navigate, FindBarStyle style)
      : label_(std::move(label)), style_(std::move(style)),
        state_(std::make_shared<FindState>(std::move(open), std::move(query),
                                           std::move(matches),
                                           std::move(current))) {
    state_->navigate_callback = std::move(navigate);
    state_->bridge = std::make_shared<detail::SearchFieldBridge>();
    const std::weak_ptr<FindState> weak = state_;
    state_->bridge->before_input =
        [weak](const InputEvent &event, InputContext &context,
               const detail::TextInputSnapshot &snapshot)
        -> std::optional<EventResult> {
      const auto state = weak.lock();
      if (!state || !state->mounted || !state->open)
        return EventResult::Ignored;
      const bool close =
          (event.type == InputType::KeyDown && event.key == Key::Escape) ||
          (event.type == InputType::Command &&
           event.command == Command::Cancel);
      const bool next =
          (event.type == InputType::KeyDown && event.key == Key::Enter) ||
          (event.type == InputType::Command &&
           (event.command == Command::Submit ||
            event.command == Command::FindNext));
      const bool previous = event.type == InputType::Command &&
                            event.command == Command::FindPrevious;
      if (snapshot.composition_active) {
        if (close) {
          if (auto editor = state->bridge->session.lock()) {
            editor->cancel_capture();
            editor->cancel_composition();
          }
          context.invalidate();
          return EventResult::Handled;
        }
        if (next || previous)
          return EventResult::Handled;
        return {};
      }
      if (close) {
        state->close();
        return EventResult::Handled;
      }
      if (next || previous) {
        state->navigate(previous || event.shift ? -1 : 1);
        return EventResult::Handled;
      }
      return {};
    };
    state_->bridge->focus_changed = [weak](bool focused,
                                           detail::TextInputSnapshot) {
      const auto state = weak.lock();
      if (!state || !focused || !state->pending_selection)
        return;
      state->pending_selection = false;
      if (auto editor = state->bridge->session.lock())
        editor->select(0, state->query.get().size());
      if (state->invalidate)
        state->invalidate();
    };
  }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  ComponentAvailability local_availability() const noexcept override {
    return {state_->open ? VisibilityMode::Visible : VisibilityMode::Collapsed,
            true, false};
  }
  bool is_focus_scope() const noexcept override { return true; }
  bool focus_scope_active() const noexcept override { return state_->open; }
  bool focus_scope_traps() const noexcept override { return false; }
  bool clips_children() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    if (!state_->open || children.size() != 5)
      return {};
    double width =
        2.0 * style_.padding + 4.0 * style_.gap +
        std::min(children[0].preferred.w, style_.maximum_field_width) +
        style_.status_width;
    float height{};
    for (std::size_t i = 0; i < children.size(); ++i) {
      if (i >= 2)
        width += children[i].preferred.w;
      height = std::max(height, children[i].preferred.h);
    }
    return {static_cast<float>(std::min(
                width, static_cast<double>(std::numeric_limits<float>::max()))),
            height + 2.0f * style_.padding};
  }
  Size minimum_size(const std::vector<ChildMetrics> &children) const override {
    if (!state_->open || children.size() != 5)
      return {};
    float width = 2.0f * style_.padding + 4.0f * style_.gap;
    for (std::size_t i = 2; i < children.size(); ++i)
      width += children[i].minimum.w;
    return {width, measure(children).h};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override {
    if (placements.size() != 5)
      return;
    const float padding = std::min(style_.padding, bounds.w * 0.5f);
    const float width = std::max(0.0f, bounds.w - 2.0f * padding),
                height = std::max(0.0f, bounds.h - 2.0f * style_.padding);
    float action_width{};
    for (std::size_t i = 2; i < children.size(); ++i)
      action_width += children[i].preferred.w;
    const float gap = std::min(style_.gap, width * 0.25f);
    const float rest = std::max(0.0f, width - action_width - 4.0f * gap);
    const float field = std::min(
        style_.maximum_field_width,
        std::max(0.0f, rest - std::min(style_.status_width, rest * 0.35f)));
    const float status =
        std::min(style_.status_width, std::max(0.0f, rest - field));
    float x = bounds.x + padding, remaining = width;
    for (std::size_t i = 0; i < placements.size(); ++i) {
      const float desired = i == 0   ? field
                            : i == 1 ? status
                                     : children[i].preferred.w;
      const float part = std::min(desired, remaining);
      placements[i].bounds = {x, bounds.y + style_.padding, part, height};
      remaining = std::max(0.0f, remaining - part);
      const float actual_gap =
          i + 1 < placements.size() ? std::min(gap, remaining) : 0;
      x += part + actual_gap;
      remaining -= actual_gap;
    }
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->mutable_value = effective_enabled() && !effective_read_only();
    state_->invalidate = context.invalidator();
    state_->layout = context.layout_invalidator();
    state_->availability = context.availability_invalidator();
    state_->focus = context.focus_invalidator();
    const std::weak_ptr<FindState> weak = state_;
    open_subscription_ = state_->source_open.observe([weak](bool) {
      if (auto state = weak.lock())
        state->sync();
    });
    query_subscription_ = state_->query.observe([weak](const auto &) {
      if (auto state = weak.lock())
        state->sync();
    });
    count_subscription_ = state_->matches.observe([weak](std::size_t) {
      if (auto state = weak.lock())
        state->sync();
    });
    current_subscription_ = state_->current.observe([weak](const auto &) {
      if (auto state = weak.lock())
        state->sync();
    });
    state_->sync();
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->action_generation;
    state_->invalidate = {};
    state_->layout = {};
    state_->availability = {};
    state_->focus = {};
    open_subscription_.reset();
    query_subscription_.reset();
    count_subscription_.reset();
    current_subscription_.reset();
  }
  EventResult input(const InputEvent &event, InputContext &) override {
    const auto state = state_;
    if (event.type == InputType::Command &&
        (event.command == Command::FindNext ||
         event.command == Command::FindPrevious)) {
      state->navigate(event.command == Command::FindNext ? 1 : -1);
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown && event.key == Key::Escape) {
      state->close();
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = label_;
    info.description = state_->description;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->valid();
    return info;
  }
  std::vector<Spec> children() const {
    const auto state = state_;
    const auto style = style_;
    const std::weak_ptr<FindState> weak = state;
    auto search =
        detail::search_field_spec(label_, state->query, "Rechercher", 0, {},
                                  style.search_field, state->bridge);
    Spec status{
        [state, style] { return std::make_unique<FindStatus>(state, style); },
        {}};
    std::vector<Spec> result{std::move(search), std::move(status)};
    for (int direction : {-1, 1, 0}) {
      auto action = std::make_shared<detail::InputActionState>();
      action->action = [weak, direction] {
        if (auto model = weak.lock()) {
          if (direction)
            model->navigate(direction);
          else
            model->close();
        }
      };
      action->enabled = [weak, direction] {
        const auto model = weak.lock();
        return model && (direction ? model->can_navigate()
                                   : model->mounted && model->open &&
                                         model->source_open.valid());
      };
      action->generation = [weak] {
        const auto model = weak.lock();
        return model ? model->action_generation : 0;
      };
      auto button_style = direction ? style.navigation : style.close;
      if (!button_style.base.minimum_width)
        button_style.base.minimum_width = 32.0f;
      if (!button_style.base.horizontal_padding)
        button_style.base.horizontal_padding = 6.0f;
      const std::string name = direction < 0   ? "Précédent"
                               : direction > 0 ? "Suivant"
                                               : "Terminer";
      const std::string glyph = direction < 0 ? "‹" : direction > 0 ? "›" : "×";
      result.push_back({[name, glyph, button_style, action, direction] {
                          return std::make_unique<detail::InputAction>(
                              name, glyph, button_style, action, true,
                              direction == 0);
                        },
                        {}});
    }
    return result;
  }
  void paint(PaintContext &context) const override {
    auto &painter = context.painter();
    const auto bounds = context.bounds();
    painter.fill_rounded_rect(
        bounds, 0.0f,
        style_.background.value_or(current_theme().palette.surface));
    painter.line({bounds.x, bounds.y + bounds.h},
                 {bounds.x + bounds.w, bounds.y + bounds.h}, 1.0f,
                 style_.border.value_or(current_theme().palette.border));
  }

private:
  void retained_checkpoint() override { state_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value)
      ++state_->action_generation;
  }
  std::string label_;
  FindBarStyle style_;
  std::shared_ptr<FindState> state_;
  Binding<bool>::Subscription open_subscription_;
  Binding<std::string>::Subscription query_subscription_;
  Binding<std::size_t>::Subscription count_subscription_;
  Binding<std::optional<std::size_t>>::Subscription current_subscription_;
};
} // namespace
FindBar::FindBar(Binding<bool> open, Binding<std::string> query,
                 Binding<std::size_t> matches,
                 Binding<std::optional<std::size_t>> current)
    : open_(std::move(open)), query_(std::move(query)),
      matches_(std::move(matches)), current_(std::move(current)) {}
FindBar::FindBar(State<bool> &open, State<std::string> &query,
                 State<std::size_t> &matches,
                 State<std::optional<std::size_t>> &current)
    : FindBar(open.binding(), query.binding(), matches.binding(),
              current.binding()) {}
FindBar &&FindBar::label(std::string value) && {
  label_ = std::move(value);
  return std::move(*this);
}
FindBar &&FindBar::on_navigate(std::function<void(std::size_t)> callback) && {
  on_navigate_ = std::move(callback);
  return std::move(*this);
}
FindBar &&FindBar::style(FindBarStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec FindBar::spec() && {
  for (float value : {style_.gap, style_.padding, style_.maximum_field_width,
                      style_.status_width})
    detail::validate_control_extent(value);
  Spec spec{[open = open_, query = query_, matches = matches_,
             current = current_, label = std::move(label_),
             callback = std::move(on_navigate_), style = std::move(style_)] {
              return std::make_unique<FindComponent>(
                  open, query, matches, current, label, callback, style);
            },
            {}};
  spec.children_factory = [](Component &component) {
    return static_cast<FindComponent &>(component).children();
  };
  return spec;
}
} // namespace ui
