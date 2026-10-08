#include <nativeui/detail/collection_model_kernel.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/scroll_view.hpp>
#include <nativeui/sidebar.hpp>
#include <nativeui/text.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ui::detail {
namespace {
float coordinate(double value) noexcept {
  const double limit = std::numeric_limits<float>::max();
  return static_cast<float>(std::clamp(value, -limit, limit));
}
float extent(double value) noexcept { return coordinate(std::max(0.0, value)); }
struct SidebarContact {
  std::optional<std::size_t> hovered, pressed, active;
  std::function<void()> release;
  std::uint64_t serial{};
  bool mounted{}, focused{};
};
void stop_contact(const std::shared_ptr<SidebarContact> &contact) {
  ++contact->serial;
  contact->pressed.reset();
  auto release = std::exchange(contact->release, {});
  if (release)
    release();
}
void stop_contact_noexcept(
    const std::shared_ptr<SidebarContact> &contact) noexcept {
  try {
    stop_contact(contact);
  } catch (...) {
  }
}
class SidebarRuntime final
    : public std::enable_shared_from_this<SidebarRuntime> {
public:
  explicit SidebarRuntime(std::shared_ptr<const SidebarRecipe> recipe)
      : recipe(std::move(recipe)), selection(this->recipe->selection()),
        open(this->recipe->sections.size(), true),
        row_bounds(this->recipe->rows.size()), scroll(ScrollAxis::Vertical) {
    if (!selection)
      throw std::invalid_argument("Sidebar selection source missing");
    open_valid.assign(open.size(), true);
    sync(true);
  }
  std::shared_ptr<const SidebarRecipe> recipe;
  std::shared_ptr<SidebarSelection> selection;
  std::vector<bool> open;
  std::vector<bool> open_valid;
  std::vector<Rect> row_bounds;
  std::optional<std::size_t> selected;
  std::uint64_t selection_revision{}, generation{};
  std::shared_ptr<SidebarContact> contact{std::make_shared<SidebarContact>()};
  ScrollState scroll;
  std::vector<Binding<bool>::Subscription> open_subscriptions;
  std::unique_ptr<SidebarSelectionSubscription> selection_subscription;
  // Each compiled Sidebar owns independent weak subscriptions.
  std::function<void()> invalidate_layout, invalidate_paint,
      invalidate_availability, request_focus;
  const Theme *theme{};
  ComponentAvailability availability;
  std::function<ComponentAvailability()> availability_reader;
  std::function<bool()> mutation_guard;
  [[nodiscard]] ComponentAvailability current_availability() const noexcept {
    return availability_reader ? availability_reader() : availability;
  }
  Rect viewport;
  Dispatcher dispatcher;
  TimerHandle timer;
  CollectionTypeahead typeahead;
  std::uint64_t search_serial{};
  unsigned pending{};
  bool syncing{}, preparing{}, selection_valid{true};

  [[nodiscard]] bool visible(std::size_t row) const noexcept {
    if (row >= recipe->rows.size())
      return false;
    const auto &item = recipe->rows[row];
    return item.header || !item.section || open[*item.section];
  }
  [[nodiscard]] bool eligible(std::size_t row) const noexcept {
    return visible(row) && recipe->rows[row].enabled;
  }
  [[nodiscard]] bool writable() const noexcept {
    const auto value = current_availability();
    return contact->mounted && value.interactive() && !value.read_only &&
           (!mutation_guard || mutation_guard());
  }
  [[nodiscard]] std::function<bool()>
  guard(std::uint64_t serial, std::uint64_t current_generation,
        const std::function<bool()> &permission = {}) {
    const std::weak_ptr<SidebarRuntime> weak = shared_from_this();
    return [weak, serial, current_generation, permission] {
      const auto current = weak.lock();
      return current && current->writable() &&
             current->contact->serial == serial &&
             current->generation == current_generation &&
             (!permission || permission());
    };
  }
  [[nodiscard]] ResolvedListViewStyle
  row_style(std::size_t row) const noexcept {
    static const Theme fallback = default_theme();
    VisualState state;
    state.enabled = availability.enabled && recipe->rows[row].enabled;
    state.read_only = availability.read_only ||
                      (!recipe->rows[row].header && !selection->valid());
    if (recipe->rows[row].header && recipe->rows[row].section) {
      const auto &binding = recipe->sections[*recipe->rows[row].section].open;
      state.read_only = state.read_only || (binding && !binding->valid());
    }
    state.selected =
        recipe->rows[row].item && selected == recipe->rows[row].item;
    state.focused = contact->focused && contact->active == row;
    state.hovered = contact->hovered == row;
    state.pressed = contact->pressed == row;
    return resolve_list_view_style(
        default_list_view_style(theme ? *theme : fallback), recipe->style.rows,
        state);
  }
  void mark(unsigned value) {
    pending |= value;
    flush();
  }
  void flush() {
    if (!contact->mounted || syncing || preparing)
      return;
    syncing = true;
    struct Guard {
      bool &value;
      ~Guard() { value = false; }
    } restore{syncing};
    const auto current_generation = generation;
    const auto effect = [&](unsigned bit, const std::function<void()> &target) {
      if (!contact->mounted || generation != current_generation ||
          !(pending & bit))
        return;
      const auto callback = target;
      pending &= ~bit;
      if (callback)
        callback();
    };
    effect(1, invalidate_availability);
    effect(2, invalidate_layout);
    effect(4, invalidate_paint);
  }
  void clear_search() noexcept {
    ++search_serial;
    typeahead.clear();
    auto old = std::exchange(timer, {});
    if (old)
      try {
        (void)dispatcher.cancel(old);
      } catch (...) {
      }
  }
  void expire_search() {
    const auto timing = dispatcher;
    const auto serial = ++search_serial;
    auto old = std::exchange(timer, {});
    if (old)
      (void)timing.cancel(old);
    if (!timing.valid())
      return;
    const std::weak_ptr<SidebarRuntime> weak = shared_from_this();
    try {
      auto next = timing.schedule_after(
          std::chrono::duration<double>{0.7}, [weak, serial] {
            const auto current = weak.lock();
            if (!current || !current->contact->mounted ||
                current->search_serial != serial)
              return;
            current->timer = {};
            current->typeahead.clear();
          });
      if (contact->mounted && search_serial == serial)
        timer = next;
      else if (next)
        (void)timing.cancel(next);
    } catch (...) {
      typeahead.clear();
      throw;
    }
  }
  void sync(bool initial = false) {
    const auto owner =
        initial ? std::shared_ptr<SidebarRuntime>{} : shared_from_this();
    if (preparing || syncing)
      return;
    preparing = true;
    struct Guard {
      bool &value;
      ~Guard() { value = false; }
    } restore{preparing};
    bool changed = false;
    for (std::size_t i = 0; i < open.size(); ++i) {
      const auto &binding = recipe->sections[i].open;
      const bool value = binding ? binding->snapshot() : true;
      const bool valid = !binding || binding->valid();
      if (open_valid[i] != valid) {
        open_valid[i] = valid;
        pending |= 4;
      }
      if (open[i] != value) {
        open[i] = value;
        changed = true;
      }
    }
    if (changed) {
      ++generation;
      pending |= 7;
      if (contact->pressed && !visible(*contact->pressed))
        stop_contact_noexcept(contact);
      if (contact->active && !visible(*contact->active)) {
        const auto section = recipe->rows[*contact->active].section;
        contact->active.reset();
        for (std::size_t i = 0; i < recipe->rows.size(); ++i)
          if (recipe->rows[i].header && recipe->rows[i].section == section) {
            contact->active = i;
            break;
          }
      }
    }
    if (initial || selection_revision != selection->revision()) {
      for (unsigned retry = 0; retry < 2; ++retry)
        if (const auto value = selection->read()) {
          const bool presentation = selected != value->selected;
          selected = value->selected;
          selection_revision = value->revision;
          if (presentation)
            pending |= 6;
          if (selected)
            for (std::size_t i = 0; i < recipe->rows.size(); ++i)
              if (recipe->rows[i].item == selected && visible(i)) {
                contact->active = i;
                break;
              }
          break;
        }
    }
    if (selection_valid != selection->valid()) {
      selection_valid = selection->valid();
      pending |= 4;
    }
    if (!contact->active || !eligible(*contact->active)) {
      contact->active.reset();
      for (std::size_t i = 0; i < recipe->rows.size(); ++i)
        if (eligible(i)) {
          contact->active = i;
          break;
        }
    }
    preparing = false;
    flush();
  }
  void ensure(std::size_t row) {
    if (row < row_bounds.size() && visible(row))
      (void)ensure_visible(scroll, row_bounds[row], ScrollAlignment::Nearest);
  }
  void move(std::size_t row, bool choose_item,
            const std::function<bool()> &permission = {}) {
    if (!eligible(row))
      return;
    const auto owner = shared_from_this();
    contact->active = row;
    const auto serial = contact->serial, current_generation = generation;
    const auto before = recipe->rows[row].item;
    if (choose_item && before && writable() && selection->valid())
      choose(row, false, permission);
    if (contact->mounted && contact->serial == serial &&
        generation == current_generation) {
      ensure(row);
      mark(4);
    }
  }
  void choose(std::size_t row, bool activate,
              const std::function<bool()> &permission = {}) {
    const auto owner = shared_from_this();
    if (!writable() || !eligible(row) || !recipe->rows[row].item ||
        !selection->valid())
      return;
    const auto serial = contact->serial, current_generation = generation;
    const auto allowed = guard(serial, current_generation, permission);
    const auto revision = selection->revision();
    if (!allowed())
      return;
    if (selected == recipe->rows[row].item && !activate) {
      ensure(row);
      return;
    }
    const auto layout = selected != recipe->rows[row].item
                            ? invalidate_layout
                            : std::function<void()>{};
    if (layout)
      layout();
    if (!allowed() || !selection->valid() || selection->revision() != revision)
      return;
    selection->choose(*recipe->rows[row].item, activate, allowed);
    if (allowed()) {
      sync();
      if (allowed())
        ensure(row);
    }
  }
  void toggle(std::size_t row, std::optional<bool> desired = {},
              const std::function<bool()> &permission = {}) {
    const auto owner = shared_from_this();
    if (!writable() || !eligible(row) || !recipe->rows[row].header)
      return;
    const auto section = recipe->rows[row].section;
    if (!section)
      return;
    const auto source = recipe->sections[*section].open;
    if (!source || !source->valid())
      return;
    auto binding = *source;
    const auto revision = binding.revision();
    const bool value = desired.value_or(!open[*section]);
    if (value == open[*section])
      return;
    const auto allowed = guard(contact->serial, generation, permission);
    if (!allowed())
      return;
    const auto layout = invalidate_layout;
    if (layout)
      layout();
    if (!allowed() || !binding.valid())
      return;
    binding.set_if(value, [binding, revision, allowed] {
      return binding.valid() && binding.revision() == revision && allowed();
    });
    if (contact->mounted)
      sync();
  }
  [[nodiscard]] std::optional<std::size_t> hit(Point position) const noexcept {
    if (!viewport.contains(position))
      return {};
    const auto offset = scroll.offset();
    const Point point{position.x - viewport.x + offset.x,
                      position.y - viewport.y + offset.y};
    for (std::size_t i = 0; i < row_bounds.size(); ++i)
      if (visible(i) && row_bounds[i].contains(point))
        return i;
    return {};
  }
  void hover(std::optional<std::size_t> row) {
    if (contact->hovered == row)
      return;
    const auto old = contact->hovered;
    const auto old_style = old ? row_style(*old) : ResolvedListViewStyle{};
    const auto next_style = row ? row_style(*row) : ResolvedListViewStyle{};
    contact->hovered = row;
    if ((old && old_style != row_style(*old)) ||
        (row && next_style != row_style(*row)))
      mark(4);
  }
};
class SidebarOwnedSpec final {
public:
  explicit SidebarOwnedSpec(Spec value) : value_(std::move(value)) {}
  Spec spec() && { return std::move(value_); }

private:
  Spec value_;
};
TextStyle row_text_style(const SidebarRuntime &runtime, std::size_t row,
                         const Theme &theme) {
  const bool header = runtime.recipe->rows[row].header;
  TextStyle text;
  text.size = header ? runtime.recipe->style.section_text_size.value_or(
                           theme.typography.control_size)
                     : runtime.recipe->style.text_size.value_or(
                           theme.typography.control_size);
  text.family = theme.typography.family;
  text.fallback_families = theme.typography.fallback_families;
  text.weight = header ? FontWeight::Bold : theme.typography.control_weight;
  text.slant = theme.typography.slant;
  text.color = header ? runtime.recipe->style.section_text.value_or(
                            theme.palette.muted_text)
                      : runtime.recipe->style.text.value_or(theme.palette.text);
  if (!runtime.availability.enabled || !runtime.recipe->rows[row].enabled)
    text.color = theme.palette.disabled;
  return text;
}
std::string fitted_label(const std::string &label, const TextStyle &text,
                         double width) {
  if (!(width > 0))
    return {};
  if (TextService::measure(label, text).width <= width)
    return label;
  const std::string ellipsis = "…";
  const double remaining = width - TextService::measure(ellipsis, text).width;
  if (!(remaining > 0))
    return {};
  std::size_t low = 0, high = label.size();
  std::string result;
  while (low < high) {
    const auto middle = low + (high - low + 1) / 2;
    const auto prefix = text::utf8_prefix(label, middle);
    const auto bytes = prefix ? prefix->size() : 0;
    if (TextService::measure(std::string_view{label}.substr(0, bytes), text)
            .width <= remaining)
      low = middle;
    else
      high = middle - 1;
  }
  const auto prefix = text::utf8_prefix(label, low);
  if (prefix)
    result.assign(*prefix);
  result += ellipsis;
  return result;
}
class SidebarRow final : public Component, public ThemeBinding {
public:
  SidebarRow(std::shared_ptr<SidebarRuntime> runtime, std::size_t row)
      : runtime_(std::move(runtime)), row_(row) {}
  [[nodiscard]] ComponentAvailability
  local_availability() const noexcept override {
    return {runtime_->visible(row_) ? VisibilityMode::Visible
                                    : VisibilityMode::Collapsed,
            runtime_->recipe->rows[row_].enabled, false};
  }
  [[nodiscard]] bool clips_children() const noexcept override { return true; }
  [[nodiscard]] Size
  measure(const std::vector<ChildMetrics> &children) const override {
    const auto &style = runtime_->recipe->style;
    const auto &row = runtime_->recipe->rows[row_];
    const auto metrics = TextService::measure(
        row.label, row_text_style(*runtime_, row_, current_theme()));
    const auto accessory =
        children.empty() ? Size{} : children.front().preferred;
    const auto indent = row.section && !row.header ? style.indentation
                        : row.header               ? 16.0
                                                   : 0.0;
    return {extent(metrics.width + accessory.w + indent + 8),
            extent(std::max<double>(
                row.header ? style.section_height : style.row_height,
                std::max(metrics.height, accessory.h) + 4))};
  }
  [[nodiscard]] Size
  minimum_size(const std::vector<ChildMetrics> &) const override {
    return {0, extent(runtime_->recipe->rows[row_].header
                          ? runtime_->recipe->style.section_height
                          : runtime_->recipe->style.row_height)};
  }
  [[nodiscard]] Constraints child_constraints(const Constraints &constraints,
                                              std::size_t,
                                              std::size_t) const override {
    return {{0, 0},
            {std::isfinite(constraints.max.w)
                 ? extent(std::max(0.0, constraints.max.w * 0.45))
                 : constraints.max.w,
             kUnboundedExtent}};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override {
    if (placements.empty())
      return;
    const auto natural = children.empty() ? Size{} : children.front().preferred;
    const float width = std::min(extent(bounds.w * 0.45), natural.w),
                height = std::min(bounds.h, natural.h);
    candidate_accessory_width_ = width;
    placements.front().bounds = {coordinate(bounds.x + bounds.w - width - 4),
                                 coordinate(bounds.y + (bounds.h - height) / 2),
                                 width, height};
  }
  [[nodiscard]] SemanticInfo semantics() const override {
    const auto &row = runtime_->recipe->rows[row_];
    SemanticInfo info;
    info.name = row.label;
    info.role = row.header ? SemanticRole::Group : SemanticRole::ListItem;
    info.enabled = effective_enabled();
    info.read_only =
        effective_read_only() || (!row.header && !runtime_->selection->valid());
    info.selected = row.item && runtime_->selected == row.item;
    info.focused =
        runtime_->contact->focused && runtime_->contact->active == row_;
    if (row.header && row.section &&
        runtime_->recipe->sections[*row.section].open &&
        !runtime_->recipe->sections[*row.section].open->valid())
      info.read_only = true;
    if (row.header && row.section)
      info.expanded = runtime_->open[*row.section]
                          ? SemanticExpandedState::Expanded
                          : SemanticExpandedState::Collapsed;
    if (info.enabled)
      info.actions.push_back(SemanticAction::Focus);
    if (info.enabled && !info.read_only) {
      if (row.header && row.section) {
        const auto &binding = runtime_->recipe->sections[*row.section].open;
        if (binding && binding->valid())
          info.actions.push_back(runtime_->open[*row.section]
                                     ? SemanticAction::Collapse
                                     : SemanticAction::Expand);
      } else if (row.item) {
        info.actions.push_back(SemanticAction::Select);
        info.actions.push_back(SemanticAction::Activate);
      }
    }
    return info;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    const auto runtime = runtime_;
    const auto permission = InputMutationAccess::guard(context);
    if (!runtime->eligible(row_))
      return EventResult::Ignored;
    if (action == SemanticAction::Focus) {
      runtime->move(row_, false, permission);
      const auto focus = runtime->request_focus;
      if (focus && runtime->contact->mounted)
        focus();
      return EventResult::Handled;
    }
    if (action == SemanticAction::Select) {
      runtime->choose(row_, false, permission);
      return EventResult::Handled;
    }
    if (action == SemanticAction::Activate) {
      runtime->choose(row_, true, permission);
      return EventResult::Handled;
    }
    if (action == SemanticAction::Expand ||
        action == SemanticAction::Collapse) {
      runtime->toggle(row_, action == SemanticAction::Expand, permission);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  void paint(PaintContext &context) const override {
    const auto runtime = runtime_;
    const auto &row = runtime->recipe->rows[row_];
    const auto style = runtime->row_style(row_);
    const auto bounds = context.bounds();
    const auto clip = context.painter().scoped_clip(bounds);
    context.painter().fill_rounded_rect(bounds, style.row_corner_radius,
                                        style.row_fill);
    if (row.item && runtime->selected == row.item && style.row_accent_width > 0)
      context.painter().fill_rounded_rect(
          {coordinate(bounds.x + style.row_accent_horizontal_inset),
           coordinate(bounds.y + style.row_accent_vertical_inset),
           style.row_accent_width,
           extent(bounds.h - 2 * style.row_accent_vertical_inset)},
          0, style.row_accent);
    double indent = row.section && !row.header
                        ? runtime->recipe->style.indentation
                    : row.header ? 16
                                 : 0;
    double available =
        std::max(0.0, static_cast<double>(bounds.w) - indent - 8);
    if (row.accessory)
      available = std::max(0.0, available - accessory_width_ - 4);
    const auto text = row_text_style(*runtime, row_, current_theme());
    const auto value = fitted_label(row.label, text, available);
    context.painter().text({coordinate(bounds.x + indent + 4),
                            coordinate(bounds.y + bounds.h * 0.5)},
                           value, text);
    if (row.header && row.section &&
        runtime->recipe->sections[*row.section].open)
      context.painter().text(
          {coordinate(bounds.x + 4), coordinate(bounds.y + bounds.h * 0.5)},
          runtime->open[*row.section] ? "v" : ">", 12,
          current_theme().palette.muted_text);
  }

private:
  void layout_committed(Rect, Rect) noexcept override {
    accessory_width_ = candidate_accessory_width_;
  }
  std::shared_ptr<SidebarRuntime> runtime_;
  std::size_t row_{};
  mutable float candidate_accessory_width_{};
  float accessory_width_{};
};
class SidebarContent final : public Component {
public:
  explicit SidebarContent(std::shared_ptr<SidebarRuntime> runtime)
      : runtime_(std::move(runtime)) {}
  [[nodiscard]] Size
  measure(const std::vector<ChildMetrics> &children) const override {
    const auto &style = runtime_->recipe->style;
    double width = 0, height = 2 * style.padding;
    std::size_t count = 0;
    for (const auto &child : children)
      if (child.participates_in_layout) {
        width = std::max<double>(width, child.preferred.w);
        height += child.preferred.h;
        if (count++)
          height += style.gap;
      }
    return {extent(width + 2 * style.padding), extent(height)};
  }
  [[nodiscard]] Size
  minimum_size(const std::vector<ChildMetrics> &) const override {
    return {};
  }
  [[nodiscard]] Constraints child_constraints(const Constraints &constraints,
                                              std::size_t,
                                              std::size_t) const override {
    return {
        {0, 0},
        {std::isfinite(constraints.max.w)
             ? extent(constraints.max.w - 2 * runtime_->recipe->style.padding)
             : constraints.max.w,
         kUnboundedExtent}};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override {
    const auto &style = runtime_->recipe->style;
    auto candidate = std::make_shared<std::vector<Rect>>(placements.size());
    double y = style.padding;
    std::size_t count = 0;
    for (std::size_t i = 0; i < placements.size() && i < children.size(); ++i) {
      if (!children[i].participates_in_layout) {
        placements[i].bounds = {bounds.x, bounds.y, 0, 0};
        continue;
      }
      if (count++)
        y += style.gap;
      const float height = children[i].preferred.h;
      const Rect local{coordinate(style.padding), coordinate(y),
                       extent(bounds.w - 2 * style.padding), height};
      (*candidate)[i] = local;
      placements[i].bounds = {coordinate(bounds.x + local.x),
                              coordinate(bounds.y + local.y), local.w, local.h};
      y += height;
    }
    candidate_ = std::move(candidate);
  }
  void paint(PaintContext &) const override {}

private:
  void layout_committed(Rect, Rect) noexcept override {
    if (candidate_)
      runtime_->row_bounds.swap(*candidate_);
    candidate_.reset();
  }
  std::shared_ptr<SidebarRuntime> runtime_;
  mutable std::shared_ptr<std::vector<Rect>> candidate_;
};
class SidebarComponent final : public Component, public ThemeBinding {
public:
  explicit SidebarComponent(std::shared_ptr<SidebarRuntime> runtime)
      : runtime_(std::move(runtime)) {}
  void bind_theme(const Theme &theme) noexcept override {
    ThemeBinding::bind_theme(theme);
    runtime_->theme = &theme;
  }
  [[nodiscard]] bool focusable() const noexcept override { return true; }
  [[nodiscard]] bool pointer_targetable() const noexcept override {
    return true;
  }
  [[nodiscard]] bool clips_children() const noexcept override { return true; }
  [[nodiscard]] bool uses_retained_checkpoint() const noexcept override {
    return true;
  }
  [[nodiscard]] std::vector<Spec> initial_children() {
    const auto runtime = runtime_;
    const std::weak_ptr<SidebarRuntime> weak = runtime;
    std::vector<Spec> rows;
    rows.reserve(runtime->recipe->rows.size());
    for (std::size_t i = 0; i < runtime->recipe->rows.size(); ++i) {
      Spec row;
      row.factory = [weak, i] {
        const auto current = weak.lock();
        if (!current)
          throw std::logic_error("Sidebar session expired");
        return std::make_unique<SidebarRow>(current, i);
      };
      if (runtime->recipe->rows[i].accessory)
        row.children.push_back(*runtime->recipe->rows[i].accessory);
      rows.push_back(std::move(row));
    }
    Spec content{[weak] {
                   const auto current = weak.lock();
                   if (!current)
                     throw std::logic_error("Sidebar session expired");
                   return std::make_unique<SidebarContent>(current);
                 },
                 std::move(rows)};
    return {std::move(ScrollView{runtime->scroll,
                                 SidebarOwnedSpec{std::move(content)}})
                .spec()};
  }
  [[nodiscard]] Size
  measure(const std::vector<ChildMetrics> &children) const override {
    return {extent(std::max<double>(
                runtime_->recipe->style.preferred_width,
                children.empty() ? 0 : children.front().preferred.w)),
            children.empty() ? 0 : children.front().preferred.h};
  }
  [[nodiscard]] Size
  minimum_size(const std::vector<ChildMetrics> &) const override {
    return {extent(runtime_->recipe->style.minimum_width), 0};
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &placements) const override {
    if (!placements.empty())
      placements.front().bounds = bounds;
  }
  void mount(MountContext &context) override {
    const auto runtime = runtime_;
    runtime->contact->mounted = true;
    runtime->availability = effective_availability();
    runtime->invalidate_layout = context.layout_invalidator();
    runtime->invalidate_paint = context.invalidator();
    runtime->invalidate_availability = context.availability_invalidator();
    runtime->request_focus = context.focus_requester();
    runtime->mutation_guard = InputMutationAccess::guard(context);
    const std::weak_ptr<SidebarRuntime> weak = runtime;
    runtime->availability_reader = [this, weak] {
      const auto current = weak.lock();
      return current && current->contact->mounted
                 ? effective_availability()
                 : ComponentAvailability{VisibilityMode::Collapsed, false,
                                         true};
    };
    runtime->selection_subscription = runtime->selection->observe([weak] {
      if (const auto current = weak.lock())
        current->mark(6);
    });
    runtime->open_subscriptions.reserve(runtime->recipe->sections.size());
    for (const auto &section : runtime->recipe->sections)
      if (section.open) {
        auto binding = *section.open;
        runtime->open_subscriptions.push_back(binding.observe([weak](bool) {
          if (const auto current = weak.lock())
            current->mark(7);
        }));
      }
    runtime->sync();
  }
  void unmount(LifecycleContext &) override {
    const auto runtime = runtime_;
    runtime->contact->mounted = false;
    stop_contact_noexcept(runtime->contact);
    runtime->clear_search();
    runtime->open_subscriptions.clear();
    runtime->selection_subscription.reset();
    runtime->invalidate_layout = {};
    runtime->invalidate_paint = {};
    runtime->invalidate_availability = {};
    runtime->request_focus = {};
    runtime->pending = 0;
    runtime->availability_reader = {};
    runtime->mutation_guard = {};
  }
  void activate(LifecycleContext &context) override {
    runtime_->dispatcher = context.dispatcher();
  }
  void deactivate(LifecycleContext &) override {
    stop_contact_noexcept(runtime_->contact);
    runtime_->contact->hovered.reset();
    runtime_->clear_search();
    runtime_->dispatcher = {};
  }
  void focus_changed(bool focused, FocusContext &context) override {
    const auto runtime = runtime_;
    runtime->contact->focused = focused;
    if (!focused)
      runtime->clear_search();
    context.invalidate();
  }
  [[nodiscard]] SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = "Navigation";
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !runtime_->selection->valid();
    info.focusable = true;
    info.focused = runtime_->contact->focused;
    if (info.enabled)
      info.actions.push_back(SemanticAction::Focus);
    return info;
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto runtime = runtime_;
    const auto contact = runtime->contact;
    const auto permission = InputMutationAccess::guard(context);
    if (event.type == InputType::PointerCancel) {
      const bool armed = contact->pressed.has_value();
      stop_contact(contact);
      runtime->mark(4);
      return armed ? EventResult::Handled : EventResult::Ignored;
    }
    if (!effective_enabled())
      return EventResult::Ignored;
    runtime->availability = effective_availability();
    if (event.type == InputType::PointerLeave) {
      runtime->hover({});
      return EventResult::Ignored;
    }
    if (event.type == InputType::PointerMove) {
      runtime->hover(runtime->hit(event.position));
      return contact->pressed ? EventResult::Handled : EventResult::Ignored;
    }
    if (event.type == InputType::PointerDown) {
      const auto hit = runtime->hit(event.position);
      if (!hit || !runtime->eligible(*hit) || !runtime->writable() ||
          (permission && !permission()))
        return EventResult::Ignored;
      auto release = context.pointer_releaser();
      const auto focus = runtime->request_focus;
      stop_contact(contact);
      if (!runtime->writable() || (permission && !permission()))
        return EventResult::Ignored;
      contact->pressed = hit;
      contact->release = std::move(release);
      const auto serial = contact->serial,
                 current_generation = runtime->generation;
      if (focus)
        focus();
      if (!runtime->writable() || (permission && !permission()) ||
          contact->serial != serial ||
          runtime->generation != current_generation) {
        if (contact->serial == serial)
          stop_contact(contact);
        return EventResult::Handled;
      }
      context.capture_pointer();
      runtime->mark(4);
      return EventResult::Handled;
    }
    if (event.type == InputType::PointerUp) {
      const auto row = contact->pressed;
      const auto hit = runtime->hit(event.position);
      const auto serial = contact->serial + 1,
                 current_generation = runtime->generation;
      const auto selection_revision = runtime->selection->revision();
      stop_contact(contact);
      runtime->mark(4);
      if (!row || row != hit || !runtime->writable() ||
          (permission && !permission()) || contact->serial != serial ||
          runtime->generation != current_generation ||
          runtime->selection->revision() != selection_revision)
        return row ? EventResult::Handled : EventResult::Ignored;
      contact->active = row;
      if (runtime->recipe->rows[*row].header)
        runtime->toggle(*row, {}, permission);
      else
        runtime->choose(*row, false, permission);
      return EventResult::Handled;
    }
    if (event.type == InputType::TextInput) {
      std::vector<CollectionSearchItem> labels;
      labels.reserve(runtime->recipe->rows.size());
      for (std::size_t i = 0; i < runtime->recipe->rows.size(); ++i)
        labels.push_back(
            {runtime->recipe->rows[i].label,
             runtime->eligible(i) && !runtime->recipe->rows[i].header});
      const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch());
      const auto target =
          runtime->typeahead.feed(event.text, now, labels, contact->active);
      runtime->expire_search();
      if (target)
        runtime->move(*target, true, permission);
      return EventResult::Handled;
    }
    if (event.type != InputType::KeyDown)
      return EventResult::Ignored;
    if (event.key == Key::Escape) {
      stop_contact(contact);
      runtime->clear_search();
      runtime->mark(4);
      return EventResult::Handled;
    }
    const auto count = runtime->recipe->rows.size();
    if (!count || !contact->active)
      return EventResult::Ignored;
    const auto active = *contact->active;
    const auto &row = runtime->recipe->rows[active];
    if (event.key == Key::Enter) {
      if (row.header)
        runtime->toggle(active, {}, permission);
      else
        runtime->choose(active, true, permission);
      return EventResult::Handled;
    }
    if (event.key == Key::Left || event.key == Key::Right) {
      if (row.header) {
        const bool is_open = row.section && runtime->open[*row.section];
        if (event.key == Key::Left)
          runtime->toggle(active, false, permission);
        else if (!is_open)
          runtime->toggle(active, true, permission);
        else
          for (std::size_t i = active + 1;
               i < count && runtime->recipe->rows[i].section == row.section;
               ++i)
            if (runtime->eligible(i)) {
              runtime->move(i, true, permission);
              break;
            }
      } else if (row.section && event.key == Key::Left)
        for (std::size_t i = 0; i < count; ++i)
          if (runtime->recipe->rows[i].header &&
              runtime->recipe->rows[i].section == row.section) {
            runtime->move(i, false, permission);
            break;
          }
      return EventResult::Handled;
    }
    int direction = 1;
    std::size_t target = active;
    if (event.key == Key::Home)
      target = 0;
    else if (event.key == Key::End) {
      target = count - 1;
      direction = -1;
    } else if (event.key == Key::Up) {
      target = active - std::min<std::size_t>(active, 1);
      direction = -1;
    } else if (event.key == Key::Down)
      target = std::min(active + 1, count - 1);
    else
      return EventResult::Ignored;
    while (!runtime->eligible(target)) {
      if (direction < 0) {
        if (!target)
          break;
        --target;
      } else {
        if (target + 1 == count)
          break;
        ++target;
      }
    }
    if (runtime->eligible(target))
      runtime->move(target, true, permission);
    return EventResult::Handled;
  }
  void paint(PaintContext &context) const override {
    const auto &theme = current_theme();
    context.painter().fill_rounded_rect(
        context.bounds(), 0,
        runtime_->recipe->style.surface.value_or(theme.palette.surface));
  }

private:
  void retained_checkpoint() override { runtime_->sync(); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &value) noexcept override {
    runtime_->availability = value;
    if (!value.interactive() || value.read_only)
      stop_contact_noexcept(runtime_->contact);
    if (!value.interactive())
      runtime_->clear_search();
  }
  void layout_committed(Rect, Rect bounds) noexcept override {
    runtime_->viewport = bounds;
  }
  std::shared_ptr<SidebarRuntime> runtime_;
};
void validate_recipe(const SidebarRecipe &recipe) {
  if (!recipe.selection)
    throw std::invalid_argument("Sidebar selection factory missing");
  std::unordered_set<std::string> ids;
  for (const auto &section : recipe.sections)
    if (section.id.empty() || !ids.insert(section.id).second)
      throw std::invalid_argument(
          "Sidebar section IDs must be nonempty and unique");
  const auto &s = recipe.style;
  for (const auto value :
       {s.padding, s.indentation, s.gap, s.row_height, s.section_height,
        s.preferred_width, s.minimum_width})
    if (!std::isfinite(value) || value < 0)
      throw std::invalid_argument(
          "Sidebar geometry must be finite and nonnegative");
  if (s.row_height <= 0 || s.section_height <= 0 ||
      s.preferred_width < s.minimum_width ||
      (s.text_size && (!std::isfinite(*s.text_size) || *s.text_size <= 0)) ||
      (s.section_text_size &&
       (!std::isfinite(*s.section_text_size) || *s.section_text_size <= 0)))
    throw std::invalid_argument(
        "Sidebar metrics must be positive and consistent");
}
} // namespace
Spec make_sidebar(SidebarRecipe recipe) {
  validate_recipe(recipe);
  auto owned = std::make_shared<const SidebarRecipe>(std::move(recipe));
  Spec result;
  result.factory = [owned] {
    auto runtime = std::make_shared<SidebarRuntime>(owned);
    return std::make_unique<SidebarComponent>(std::move(runtime));
  };
  result.children_factory = [](Component &component) {
    return static_cast<SidebarComponent &>(component).initial_children();
  };
  return result;
}
} // namespace ui::detail
