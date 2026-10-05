#include "detail/widget_text_input_policy.hpp"
#include <nativeui/text_input.hpp>
#include <stdexcept>

namespace ui {

namespace detail {
struct TextInputSession::Storage {
  TextInputComponent *editor{};
  bool mounted{};
  std::function<void()> request_focus;
  std::function<void()> invalidate;
  std::function<void()> release_pointer;
  std::uint64_t pointer_generation{};
  std::uint64_t edit_generation{};
  std::uint64_t buffer_generation{};
};
TextInputSession::TextInputSession() : storage_(std::make_unique<Storage>()) {}
TextInputSession::~TextInputSession() = default;
bool TextInputSession::mounted() const noexcept {
  return storage_->mounted && storage_->editor;
}
TextInputSnapshot TextInputSession::snapshot() const {
  return mounted() ? TextInputAccess::snapshot(*storage_->editor)
                   : TextInputSnapshot{};
}
void TextInputSession::replace(
    std::string text,
    std::optional<std::pair<std::size_t, std::size_t>> selection,
    bool reset_history) {
  if (!mounted())
    return;
  auto &editor = *storage_->editor;
  editor.pending_ime_commit_.clear();
  editor.model_.cancel_composition();
  editor.model_.set_text(std::move(text), false, reset_history);
  ++storage_->buffer_generation;
  if (selection) {
    editor.model_.move_to(selection->first);
    editor.model_.move_to(selection->second, true);
  }
  editor.scroll_x_ = 0;
  editor.caret_visible_ = true;
}
void TextInputSession::refresh_source() {
  if (!mounted())
    return;
  auto &editor = *storage_->editor;
  if (editor.model_.text() == editor.state_.get())
    return;
  std::string text = editor.state_.get();
  const auto invalidate = storage_->invalidate;
  editor.pending_ime_commit_.clear();
  editor.model_.cancel_composition();
  editor.model_.set_text(std::move(text), true, true);
  ++storage_->buffer_generation;
  editor.focus_snapshot_ = editor.model_.text();
  editor.scroll_x_ = 0.0f;
  editor.caret_visible_ = true;
  if (invalidate)
    invalidate();
}
void TextInputSession::select(std::size_t begin, std::size_t end) {
  if (!mounted())
    return;
  auto &editor = *storage_->editor;
  editor.pending_ime_commit_.clear();
  editor.model_.cancel_composition();
  editor.model_.move_to(begin);
  editor.model_.move_to(end, true);
  editor.scroll_x_ = 0;
  editor.caret_visible_ = true;
}
void TextInputSession::cancel_composition() {
  if (!mounted())
    return;
  auto &editor = *storage_->editor;
  editor.pending_ime_commit_.clear();
  editor.model_.cancel_composition();
  ++storage_->buffer_generation;
  editor.scroll_x_ = 0;
}
void TextInputSession::cancel_capture() noexcept {
  ++storage_->pointer_generation;
  auto release = std::move(storage_->release_pointer);
  if (storage_->editor) {
    storage_->editor->drag_select_ = false;
    storage_->editor->pressed_ = false;
  }
  if (release) {
    try {
      release();
    } catch (...) {
    }
  }
}
void TextInputSession::reset_baseline() {
  if (mounted())
    storage_->editor->focus_snapshot_ = storage_->editor->model_.text();
}
void TextInputSession::request_focus() {
  if (!mounted())
    return;
  auto request = storage_->request_focus;
  if (request)
    request();
}
void TextInputAccess::initialize(TextInputComponent &editor) {
  editor.session_ = std::shared_ptr<TextInputSession>(new TextInputSession());
  editor.session_->storage_->editor = &editor;
}
std::weak_ptr<TextInputSession>
TextInputAccess::configure(TextInputComponent &editor,
                           std::shared_ptr<TextInputPolicy> policy) {
  if (editor.session_->storage_->mounted || editor.policy_)
    throw std::logic_error(
        "TextInput policy must be configured once before mount");
  if (!policy)
    throw std::invalid_argument("TextInput policy is missing");
  editor.policy_ = std::move(policy);
  return editor.session_;
}
void TextInputAccess::mounted(TextInputComponent &editor,
                              MountContext &context) {
  editor.session_->storage_->request_focus = context.focus_requester();
  editor.session_->storage_->invalidate = context.invalidator();
  editor.session_->storage_->mounted = true;
}
void TextInputAccess::detached(TextInputComponent &editor) noexcept {
  if (!editor.session_)
    return;
  editor.session_->cancel_capture();
  editor.session_->storage_->mounted = false;
  editor.session_->storage_->editor = nullptr;
  editor.session_->storage_->request_focus = {};
  editor.session_->storage_->invalidate = {};
}
TextInputSnapshot TextInputAccess::snapshot(const TextInputComponent &editor) {
  return {editor.model_.text(),
          editor.model_.anchor(),
          editor.model_.cursor(),
          editor.model_.composition_active(),
          editor.focused_,
          editor.effective_read_only() || !editor.state_.valid() ||
              (editor.policy_ && editor.policy_->read_only),
          true,
          editor.session_->storage_->edit_generation,
          {}};
}
} // namespace detail
TextInputComponent::TextInputComponent(
    std::string label, Binding<std::string> state, std::string placeholder,
    std::size_t max_length, SubmitCallback on_submit, TextInputStyle style)
    : TextInputComponent(std::move(label), std::move(state),
                         std::move(placeholder), max_length,
                         std::move(on_submit), {}, std::move(style)) {}

TextInputComponent::TextInputComponent(
    std::string label, Binding<std::string> state, std::string placeholder,
    std::size_t max_length, SubmitCallback on_submit,
    KeyDownCallback on_key_down, TextInputStyle style)
    : label_(std::move(label)), state_(std::move(state)),
      model_(state_.get(), max_length), placeholder_(std::move(placeholder)),
      on_submit_(std::move(on_submit)), on_key_down_(std::move(on_key_down)),
      style_(std::move(style)) {
  detail::TextInputAccess::initialize(*this);
}

TextInputComponent::~TextInputComponent() {
  detail::TextInputAccess::detached(*this);
}
bool TextInputComponent::focusable() const noexcept {
  return !policy_ || policy_->focusable;
}

Size TextInputComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto resolved = resolved_style(focused_);
  return Size{resolved.control_width, resolved.control_height};
}

void TextInputComponent::mount(MountContext &ctx) {
  detail::TextInputAccess::mounted(*this, ctx);
  auto invalidate = ctx.invalidator();
  subscription_ = state_.observe(
      [this, invalidate = std::move(invalidate)](const std::string &value) {
        if (value == model_.text())
          return;
        pending_ime_commit_.clear();
        model_.set_text(value, true, true);
        ++session_->storage_->buffer_generation;
        scroll_x_ = 0.0f;
        caret_visible_ = true;
        invalidate();
      });
}

void TextInputComponent::unmount(LifecycleContext &) {
  detail::TextInputAccess::detached(*this);
  subscription_.reset();
  pending_ime_commit_.clear();
  model_.cancel_composition();
  drag_select_ = false;
}

void TextInputComponent::focus_changed(bool focused, FocusContext &ctx) {
  const auto session = session_;
  const auto before = presentation_signature(focused_);
  if (!focused)
    session->cancel_capture();
  auto callback =
      policy_ ? policy_->focus_changed : decltype(policy_->focus_changed){};
  const bool was_focused = focused_;
  focused_ = focused;
  caret_visible_ = true;
  if (focused_) {
    if (!was_focused)
      focus_snapshot_ = model_.text();
    ensure_cursor_visible(ctx);
  } else {
    pending_ime_commit_.clear();
    model_.cancel_composition();
    drag_select_ = false;
    pressed_ = false;
    ctx.set_text_input(false);
  }
  const auto after = presentation_signature(focused_);
  auto snapshot = detail::TextInputAccess::snapshot(*this);
  snapshot.allow_edit_commit = ctx.allows_edit_commit();
  invalidate_style_transition(before, after, ctx, true);
  if (callback && session->mounted())
    callback(focused, std::move(snapshot));
}

void TextInputComponent::deactivate(LifecycleContext &ctx) {
  const auto before = presentation_signature(focused_);
  session_->cancel_capture();
  focused_ = false;
  hovered_ = false;
  pressed_ = false;
  const auto after = presentation_signature(focused_);
  invalidate_style_transition(before, after, ctx);
}

EventResult TextInputComponent::input(const InputEvent &event,
                                      InputContext &ctx) {
  const auto session = session_;
  if (event.type == InputType::PointerDown) {
    auto release = ctx.pointer_releaser();
    ++session->storage_->pointer_generation;
    session->storage_->release_pointer = std::move(release);
  }
  const auto generation = session->storage_->pointer_generation;
  try {
    if (event.type == InputType::PointerUp ||
        event.type == InputType::PointerCancel) {
      auto release = std::move(session->storage_->release_pointer);
      if (release)
        release();
    }
    return input_impl(event, ctx);
  } catch (...) {
    if (session->storage_->pointer_generation == generation)
      session->cancel_capture();
    throw;
  }
}
EventResult TextInputComponent::input_impl(const InputEvent &event,
                                           InputContext &ctx) {
  auto intercept =
      policy_ ? policy_->before_input : decltype(policy_->before_input){};
  if (intercept) {
    const auto snapshot = detail::TextInputAccess::snapshot(*this);
    const auto result = intercept(event, ctx, snapshot);
    if (result)
      return *result;
  }
  bool read_only = effective_read_only() || !state_.valid() ||
                   (policy_ && policy_->read_only);

  if (event.type == InputType::PointerMove) {
    const bool inside = ctx.bounds().contains(event.position);
    if (hovered_ != inside) {
      const auto before = presentation_signature(focused_);
      hovered_ = inside;
      const auto after = presentation_signature(focused_);
      invalidate_style_transition(before, after, ctx);
    }
  } else if (event.type == InputType::PointerUp) {
    const bool inside = ctx.bounds().contains(event.position);
    if (pressed_ || hovered_ != inside) {
      const auto before = presentation_signature(focused_);
      pressed_ = false;
      hovered_ = inside;
      const auto after = presentation_signature(focused_);
      invalidate_style_transition(before, after, ctx);
    }
  } else if (event.type == InputType::PointerCancel) {
    if (pressed_ || hovered_) {
      const auto before = presentation_signature(focused_);
      pressed_ = false;
      hovered_ = false;
      const auto after = presentation_signature(focused_);
      invalidate_style_transition(before, after, ctx);
    }
  }

  if (event.type == InputType::Tick) {
    if (!focused_)
      return EventResult::Ignored;
    if (read_only && model_.composition_active()) {
      pending_ime_commit_.clear();
      model_.cancel_composition();
      ensure_cursor_visible(ctx);
    }
    caret_visible_ = !caret_visible_;
    ctx.invalidate();
    return EventResult::Handled;
  }

  caret_visible_ = true;

  if (event.type == InputType::Composition) {
    if (read_only) {
      pending_ime_commit_.clear();
      model_.cancel_composition();
      ensure_cursor_visible(ctx);
      ctx.invalidate();
      return EventResult::Handled;
    }

    switch (event.composition.type) {
    case CompositionType::Start:
      pending_ime_commit_.clear();
      model_.begin_composition();
      ensure_cursor_visible(ctx);
      ctx.invalidate();
      return EventResult::Handled;
    case CompositionType::Update:
      pending_ime_commit_.clear();
      model_.update_composition(event.composition.text,
                                event.composition.cursor_byte,
                                event.composition.selection_bytes);
      ensure_cursor_visible(ctx);
      ctx.invalidate();
      return EventResult::Handled;
    case CompositionType::Commit:
      if (model_.commit_composition(event.composition.text)) {
        pending_ime_commit_ = event.composition.text;
        commit(ctx);
      } else {
        pending_ime_commit_.clear();
        ensure_cursor_visible(ctx);
        ctx.invalidate();
      }
      return EventResult::Handled;
    case CompositionType::Cancel:
      pending_ime_commit_.clear();
      model_.cancel_composition();
      ensure_cursor_visible(ctx);
      ctx.invalidate();
      return EventResult::Handled;
    }
  }

  if (event.type == InputType::TextInput) {
    if (event.text.empty())
      return EventResult::Ignored;
    if (read_only) {
      pending_ime_commit_.clear();
      return EventResult::Handled;
    }
    const bool duplicate_commit =
        !pending_ime_commit_.empty() && event.text == pending_ime_commit_;
    pending_ime_commit_.clear();
    if (!duplicate_commit)
      insert_text(event.text, ctx);
    return EventResult::Handled;
  }

  if (read_only && model_.composition_active()) {
    pending_ime_commit_.clear();
    model_.cancel_composition();
    ensure_cursor_visible(ctx);
    ctx.invalidate();
  }

  // Native input methods own editing/navigation keys while marked text is
  // active. Swallow the translated KeyDown here so the platform can
  // update its candidate/preedit state without a second retained-editor
  // command cancelling or moving the composition snapshot.
  if (event.type == InputType::KeyDown && model_.composition_active()) {
    return EventResult::Handled;
  }

  if (event.type == InputType::PointerDown) {
    const auto before = presentation_signature(focused_);
    pending_ime_commit_.clear();
    model_.cancel_composition();
    hovered_ = true;
    pressed_ = true;
    const auto clicked = index_from_x(ctx, event.position.x);
    if (event.clicks >= 3) {
      model_.select_all();
    } else if (event.clicks == 2) {
      model_.select_word_at(clicked);
    } else {
      model_.move_to(clicked, event.shift);
    }

    // A multi-click establishes a complete word/all selection. Do not
    // immediately collapse it again on PointerUp. Single-click drags
    // retain pointer capture for normal range selection.
    drag_select_ = event.clicks == 1;
    if (drag_select_)
      ctx.capture_pointer();
    ensure_cursor_visible(ctx);
    const auto after = presentation_signature(focused_);
    invalidate_style_transition(before, after, ctx, true);
    return EventResult::Handled;
  }

  if (event.type == InputType::PointerMove && drag_select_) {
    model_.move_to(index_from_x(ctx, event.position.x), true);
    ensure_cursor_visible(ctx);
    ctx.invalidate();
    return EventResult::Handled;
  }

  if (event.type == InputType::PointerUp && drag_select_) {
    model_.move_to(index_from_x(ctx, event.position.x), true);
    drag_select_ = false;
    ctx.release_pointer();
    ensure_cursor_visible(ctx);
    ctx.invalidate();
    return EventResult::Handled;
  }

  if (event.type == InputType::PointerCancel && drag_select_) {
    drag_select_ = false;
    ctx.invalidate();
    return EventResult::Handled;
  }

  if (event.type == InputType::Command) {
    pending_ime_commit_.clear();
    switch (event.command) {
    case Command::SelectAll:
      model_.select_all();
      ensure_cursor_visible(ctx);
      ctx.invalidate();
      return EventResult::Handled;
    case Command::Copy:
      copy_selection(ctx);
      return EventResult::Handled;
    case Command::Cut:
      if (read_only)
        return EventResult::Handled;
      if (model_.has_selection()) {
        copy_selection(ctx);
        if (model_.erase_selection())
          commit(ctx);
      }
      return EventResult::Handled;
    case Command::Paste:
      if (!read_only)
        ctx.request_clipboard_text();
      return EventResult::Handled;
    case Command::Undo:
      if (!read_only && model_.undo())
        commit(ctx);
      return EventResult::Handled;
    case Command::Redo:
      if (!read_only && model_.redo())
        commit(ctx);
      return EventResult::Handled;
    case Command::Submit:
    case Command::Cancel:
    case Command::FindNext:
    case Command::FindPrevious:
    case Command::None:
      return EventResult::Ignored;
    }
  }

  if (event.type != InputType::KeyDown)
    return EventResult::Ignored;
  // The callable's copy and invocation may both replace this retained editor.
  // Keep independent session/source identity before either exposure; an Ignored
  // result may continue the built-in commands only for the same live buffer.
  const auto key_session = session_;
  const auto key_source = state_;
  const bool source_valid = key_source.valid();
  const auto edit_generation = key_session->storage_->edit_generation;
  const auto buffer_generation = key_session->storage_->buffer_generation;
  auto permission = detail::InputMutationAccess::action_guard(ctx);
  auto key_callback = on_key_down_;
  const auto current = [&] {
    return key_session->mounted() && key_session->storage_->editor == this &&
           key_session->storage_->edit_generation == edit_generation &&
           key_session->storage_->buffer_generation == buffer_generation &&
           key_source.valid() == source_valid && (!permission || permission());
  };
  if (!current())
    return EventResult::Handled;
  if (key_callback) {
    const auto result = key_callback(event);
    if (!current())
      return EventResult::Handled;
    if (result == EventResult::Handled) {
      pending_ime_commit_.clear();
      return EventResult::Handled;
    }
  }
  if (model_.composition_active())
    return EventResult::Handled;
  read_only = effective_read_only() || !key_source.valid() ||
              (policy_ && policy_->read_only);
  pending_ime_commit_.clear();

  switch (event.key) {
  case Key::Left:
    move_left(event, ctx);
    return EventResult::Handled;
  case Key::Right:
    move_right(event, ctx);
    return EventResult::Handled;
  case Key::Home:
  case Key::Up:
    model_.move_to(0, event.shift);
    ensure_cursor_visible(ctx);
    ctx.invalidate();
    return EventResult::Handled;
  case Key::End:
  case Key::Down:
    model_.move_to(model_.text().size(), event.shift);
    ensure_cursor_visible(ctx);
    ctx.invalidate();
    return EventResult::Handled;
  case Key::Backspace:
    if (!read_only)
      backspace(event, ctx);
    return EventResult::Handled;
  case Key::Delete:
    if (!read_only)
      delete_forward(event, ctx);
    return EventResult::Handled;
  case Key::Enter: {
    focus_snapshot_ = model_.text();
    const auto value = model_.text();
    const auto source = state_;
    const auto session = session_;
    model_.move_to(model_.cursor());
    // A user's callable copy constructor may reenter and retire this editor.
    // This is the final component member read before any such copy runs.
    auto callback = on_submit_;
    if (!session->mounted())
      return EventResult::Handled;
    ctx.invalidate();
    if (source.valid() && session->mounted() && callback)
      callback(value);
    return EventResult::Handled;
  }
  case Key::Escape:
    if (read_only)
      return EventResult::Handled;
    if (model_.text() != focus_snapshot_) {
      model_.set_text(focus_snapshot_, false, false);
      commit(ctx);
    } else {
      model_.move_to(model_.cursor());
      ctx.invalidate();
    }
    return EventResult::Handled;
  default:
    return EventResult::Ignored;
  }
}

SemanticInfo TextInputComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::TextInput;
  info.name = label_;
  info.text_value = model_.text();
  info.focusable = focusable();
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only() || !state_.valid() ||
                   (policy_ && policy_->read_only);
  if (info.enabled)
    info.actions = {SemanticAction::Focus};
  return info;
}

void TextInputComponent::paint(PaintContext &p) const {
  const auto b = p.bounds();
  auto &dl = p.painter();
  const auto resolved = resolved_style(p.focused());
  const bool show_composition = model_.composition_active() &&
                                !effective_read_only() &&
                                !(policy_ && policy_->read_only);

  TextStyle label_style{};
  label_style.size = resolved.label_size;
  label_style.color = resolved.label;
  label_style.align = TextAlign::Left;
  label_style.weight = resolved.text_weight;
  label_style.slant = resolved.text_slant;
  label_style.family = resolved.font_family;
  label_style.fallback_families = resolved.fallback_families;
  if (!policy_ || policy_->paint_label)
    dl.text(Point{b.x, b.y + resolved.label_offset_y}, label_, label_style);

  const auto field = field_rect(b, resolved);
  if (!policy_ || policy_->paint_chrome) {
    dl.fill_rounded_rect(field, resolved.corner_radius, resolved.field_fill);
    dl.stroke_rounded_rect(field, resolved.corner_radius, resolved.border_width,
                           policy_ && policy_->border_override
                               ? *policy_->border_override
                               : resolved.border);
  }

  const auto content = content_rect(field, resolved);
  auto content_clip = dl.scoped_clip(content);

  const float base_x = content.x - scroll_x_;
  const float center_y = field.y + field.h * 0.5f;

  if (model_.has_selection() && !show_composition) {
    const auto begin = model_.selection_begin();
    const auto end = model_.selection_end();
    const float x1 = base_x + p.text_width(prefix(begin), resolved.text_size);
    const float x2 = base_x + p.text_width(prefix(end), resolved.text_size);
    dl.fill_rounded_rect(
        Rect{
            x1, field.y + resolved.selection_vertical_inset,
            std::max(1.0f, x2 - x1),
            std::max(1.0f, field.h - resolved.selection_vertical_inset * 2.0f)},
        resolved.selection_corner_radius, resolved.selection);
  }

  if (show_composition) {
    const auto committed = std::string_view{model_.text()};
    const auto replace_begin = model_.selection_begin();
    const auto replace_end = model_.selection_end();
    const auto before = committed.substr(0, replace_begin);
    const auto after = committed.substr(replace_end);
    const auto &preedit = model_.composition_text();

    float x = base_x;
    if (!before.empty()) {
      dl.text(Point{x, center_y}, before, resolved.text_size, resolved.text,
              TextAlign::Left);
      x += p.text_width(before, resolved.text_size);
    }

    if (!preedit.empty()) {
      dl.text(Point{x, center_y}, preedit, resolved.text_size, resolved.text,
              TextAlign::Left);
      const float preedit_width = p.text_width(preedit, resolved.text_size);
      const float underline_y =
          field.y + field.h - resolved.composition_underline_inset;
      dl.line(Point{x, underline_y},
              Point{x + std::max(1.0f, preedit_width), underline_y},
              resolved.composition_underline_width,
              resolved.composition_underline);
      x += preedit_width;
    }

    if (!after.empty()) {
      dl.text(Point{x, center_y}, after, resolved.text_size, resolved.text,
              TextAlign::Left);
    }
  } else if (model_.text().empty() && !placeholder_.empty()) {
    dl.text(Point{base_x, center_y}, placeholder_, resolved.text_size,
            resolved.placeholder, TextAlign::Left);
  } else {
    dl.text(Point{base_x, center_y}, model_.text(), resolved.text_size,
            resolved.text, TextAlign::Left);
  }

  if (p.focused() && caret_visible_) {
    float caret_x =
        base_x + p.text_width(prefix(model_.cursor()), resolved.text_size);
    if (show_composition) {
      const auto before =
          std::string_view{model_.text()}.substr(0, model_.selection_begin());
      const auto preedit_cursor =
          std::string_view{model_.composition_text()}.substr(
              0, std::min(model_.composition_cursor_byte(),
                          model_.composition_text().size()));
      caret_x = base_x + p.text_width(before, resolved.text_size) +
                p.text_width(preedit_cursor, resolved.text_size);
    }
    dl.line(Point{caret_x, field.y + resolved.caret_vertical_inset},
            Point{caret_x, field.y + field.h - resolved.caret_vertical_inset},
            resolved.caret_width, resolved.caret);
  }
}

void TextInputComponent::apply_patch(
    PresentationSignature &target, const TextInputStylePatch &patch) noexcept {
  if (patch.field_fill)
    target.field_fill = *patch.field_fill;
  if (patch.border)
    target.border = *patch.border;
  if (patch.label)
    target.label = *patch.label;
  if (patch.text)
    target.text = *patch.text;
  if (patch.placeholder)
    target.placeholder = *patch.placeholder;
  if (patch.selection)
    target.selection = *patch.selection;
  if (patch.caret)
    target.caret = *patch.caret;
  if (patch.composition_underline) {
    target.composition_underline = *patch.composition_underline;
  }
  if (patch.border_width)
    target.border_width = *patch.border_width;
  if (patch.corner_radius)
    target.corner_radius = *patch.corner_radius;
  if (patch.control_width)
    target.control_width = *patch.control_width;
  if (patch.control_height)
    target.control_height = *patch.control_height;
  if (patch.field_top)
    target.field_top = *patch.field_top;
  if (patch.field_height)
    target.field_height = *patch.field_height;
  if (patch.horizontal_padding)
    target.horizontal_padding = *patch.horizontal_padding;
  if (patch.content_vertical_inset) {
    target.content_vertical_inset = *patch.content_vertical_inset;
  }
  if (patch.label_offset_y)
    target.label_offset_y = *patch.label_offset_y;
  if (patch.label_size)
    target.label_size = *patch.label_size;
  if (patch.text_size)
    target.text_size = *patch.text_size;
  if (patch.selection_corner_radius) {
    target.selection_corner_radius = *patch.selection_corner_radius;
  }
  if (patch.selection_vertical_inset) {
    target.selection_vertical_inset = *patch.selection_vertical_inset;
  }
  if (patch.caret_width)
    target.caret_width = *patch.caret_width;
  if (patch.caret_vertical_inset)
    target.caret_vertical_inset = *patch.caret_vertical_inset;
  if (patch.composition_underline_width) {
    target.composition_underline_width = *patch.composition_underline_width;
  }
  if (patch.composition_underline_inset) {
    target.composition_underline_inset = *patch.composition_underline_inset;
  }
  if (patch.text_weight)
    target.text_weight = *patch.text_weight;
  if (patch.text_slant)
    target.text_slant = *patch.text_slant;
  if (patch.font_family)
    target.font_family = *patch.font_family;
  if (patch.fallback_families)
    target.fallback_families = &*patch.fallback_families;
}

TextInputComponent::PresentationSignature
TextInputComponent::presentation_signature(bool focused) const noexcept {
  const auto &theme = current_theme();
  PresentationSignature result{
      .field_fill = theme.palette.control_background,
      .border = theme.palette.border,
      .label = theme.palette.muted_text,
      .text = theme.palette.text,
      .placeholder = theme.palette.muted_text,
      .selection = theme.palette.selection,
      .caret = colors::caret,
      .composition_underline = theme.palette.focus,
      .border_width = theme.controls.border_width,
      .corner_radius = theme.radii.medium,
      .control_width = 420.0f,
      .control_height = 82.0f,
      .field_top = 24.0f,
      .field_height = 46.0f,
      .horizontal_padding = 12.0f,
      .content_vertical_inset = 4.0f,
      .label_offset_y = 10.0f,
      .label_size = 12.0f,
      .text_size = 15.0f,
      .selection_corner_radius = 3.0f,
      .selection_vertical_inset = 8.0f,
      .caret_width = 1.5f,
      .caret_vertical_inset = 9.0f,
      .composition_underline_width = 1.5f,
      .composition_underline_inset = 9.0f,
      .text_weight = theme.typography.control_weight,
      .text_slant = theme.typography.slant,
      .font_family = theme.typography.family,
      .fallback_families = &theme.typography.fallback_families,
  };
  apply_patch(result, style_.base);

  const auto state = current_visual_state(focused);
  switch (resolve_interaction_state(state)) {
  case InteractionVisualState::Normal:
    break;
  case InteractionVisualState::Hovered:
    result.border = theme.palette.control_hover;
    apply_patch(result, style_.hovered);
    break;
  case InteractionVisualState::Pressed:
    result.border = theme.palette.accent;
    apply_patch(result, style_.pressed);
    break;
  case InteractionVisualState::Disabled:
    result.text = theme.palette.disabled;
    result.label = theme.palette.disabled;
    result.placeholder = theme.palette.disabled;
    result.caret = theme.palette.disabled;
    apply_patch(result, style_.disabled);
    break;
  }
  if (state.read_only)
    apply_patch(result, style_.read_only);
  if (state.focused) {
    result.border = theme.palette.focus;
    result.border_width = theme.controls.focus_ring_width;
    apply_patch(result, style_.focused);
  }
  return result;
}

bool TextInputComponent::same_color(Color lhs, Color rhs) noexcept {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool TextInputComponent::same_fallbacks(const PresentationSignature &lhs,
                                        const PresentationSignature &rhs) {
  return lhs.fallback_families == rhs.fallback_families ||
         (lhs.fallback_families != nullptr &&
          rhs.fallback_families != nullptr &&
          *lhs.fallback_families == *rhs.fallback_families);
}

bool TextInputComponent::same_layout(const PresentationSignature &lhs,
                                     const PresentationSignature &rhs) {
  return lhs.control_width == rhs.control_width &&
         lhs.control_height == rhs.control_height &&
         lhs.field_top == rhs.field_top &&
         lhs.field_height == rhs.field_height &&
         lhs.horizontal_padding == rhs.horizontal_padding &&
         lhs.content_vertical_inset == rhs.content_vertical_inset &&
         lhs.label_offset_y == rhs.label_offset_y &&
         lhs.label_size == rhs.label_size && lhs.text_size == rhs.text_size &&
         lhs.text_weight == rhs.text_weight &&
         lhs.text_slant == rhs.text_slant &&
         lhs.font_family == rhs.font_family && same_fallbacks(lhs, rhs);
}

bool TextInputComponent::same_presentation(const PresentationSignature &lhs,
                                           const PresentationSignature &rhs) {
  return same_color(lhs.field_fill, rhs.field_fill) &&
         same_color(lhs.border, rhs.border) &&
         same_color(lhs.label, rhs.label) && same_color(lhs.text, rhs.text) &&
         same_color(lhs.placeholder, rhs.placeholder) &&
         same_color(lhs.selection, rhs.selection) &&
         same_color(lhs.caret, rhs.caret) &&
         same_color(lhs.composition_underline, rhs.composition_underline) &&
         lhs.border_width == rhs.border_width &&
         lhs.corner_radius == rhs.corner_radius &&
         lhs.selection_corner_radius == rhs.selection_corner_radius &&
         lhs.selection_vertical_inset == rhs.selection_vertical_inset &&
         lhs.caret_width == rhs.caret_width &&
         lhs.caret_vertical_inset == rhs.caret_vertical_inset &&
         lhs.composition_underline_width == rhs.composition_underline_width &&
         lhs.composition_underline_inset == rhs.composition_underline_inset &&
         same_layout(lhs, rhs);
}

template <class Context>
void TextInputComponent::invalidate_style_transition(
    const PresentationSignature &before, const PresentationSignature &after,
    Context &context, bool require_paint) {
  if (!same_layout(before, after)) {
    context.invalidate_layout();
    return;
  }
  if (require_paint || !same_presentation(before, after))
    context.invalidate();
}

VisualState
TextInputComponent::current_visual_state(bool focused) const noexcept {
  return VisualState{
      .enabled = effective_enabled(),
      .read_only = effective_read_only() || (policy_ && policy_->read_only),
      .hovered = hovered_,
      .pressed = pressed_,
      .focused = focused,
  };
}

ResolvedTextInputStyle TextInputComponent::resolved_style(bool focused) const {
  return resolve_text_input_style(default_text_input_style(current_theme()),
                                  style_, current_visual_state(focused));
}

Rect TextInputComponent::field_rect(
    Rect bounds, const ResolvedTextInputStyle &style) noexcept {
  return Rect{bounds.x, bounds.y + style.field_top, bounds.w,
              style.field_height};
}

Rect TextInputComponent::content_rect(
    Rect field, const ResolvedTextInputStyle &style) noexcept {
  return Rect{field.x + style.horizontal_padding,
              field.y + style.content_vertical_inset,
              std::max(1.0f, field.w - style.horizontal_padding * 2.0f),
              std::max(1.0f, field.h - style.content_vertical_inset * 2.0f)};
}

std::string_view
TextInputComponent::prefix(std::size_t byte_index) const noexcept {
  return std::string_view{model_.text()}.substr(
      0, std::min(byte_index, model_.text().size()));
}

void TextInputComponent::copy_selection(InputContext &ctx) const {
  if (!model_.has_selection())
    return;
  ctx.set_clipboard_text(model_.selected_text());
}

void TextInputComponent::insert_text(std::string_view raw, InputContext &ctx) {
  const auto incoming = text::single_line(raw);
  if (incoming.empty())
    return;
  if (model_.insert(incoming))
    commit(ctx);
}

TextMotion TextInputComponent::motion_for(const InputEvent &event) noexcept {
  if (event.gui)
    return TextMotion::Document;
  if (event.ctrl || event.alt)
    return TextMotion::Word;
  return TextMotion::Codepoint;
}

void TextInputComponent::move_left(const InputEvent &event, InputContext &ctx) {
  model_.move_left(motion_for(event), event.shift);
  ensure_cursor_visible(ctx);
  ctx.invalidate();
}

void TextInputComponent::move_right(const InputEvent &event,
                                    InputContext &ctx) {
  model_.move_right(motion_for(event), event.shift);
  ensure_cursor_visible(ctx);
  ctx.invalidate();
}

void TextInputComponent::backspace(const InputEvent &event, InputContext &ctx) {
  if (model_.backspace(motion_for(event)))
    commit(ctx);
}

void TextInputComponent::delete_forward(const InputEvent &event,
                                        InputContext &ctx) {
  if (model_.delete_forward(motion_for(event)))
    commit(ctx);
}

template <class Context>
std::size_t TextInputComponent::index_from_x(Context &ctx, float x) const {
  const auto resolved = resolved_style(focused_);
  const auto field = field_rect(ctx.bounds(), resolved);
  const auto content = content_rect(field, resolved);
  const float wanted = std::max(0.0f, x - content.x + scroll_x_);

  std::size_t previous = 0;
  float previous_width = 0.0f;
  for (std::size_t i = text::next_codepoint(model_.text(), 0);
       i <= model_.text().size() && i > previous;
       i = (i == model_.text().size()
                ? model_.text().size() + 1
                : text::next_codepoint(model_.text(), i))) {
    const auto bounded = std::min(i, model_.text().size());
    const float width = ctx.text_width(prefix(bounded), resolved.text_size);
    if (wanted < (previous_width + width) * 0.5f)
      return previous;
    previous = bounded;
    previous_width = width;
    if (bounded == model_.text().size())
      break;
  }
  return model_.text().size();
}

template <class Context>
void TextInputComponent::ensure_cursor_visible(Context &ctx) {
  const auto resolved = resolved_style(focused_);
  const auto field = field_rect(ctx.bounds(), resolved);
  const auto content = content_rect(field, resolved);

  float caret = ctx.text_width(prefix(model_.cursor()), resolved.text_size);
  float total = ctx.text_width(model_.text(), resolved.text_size);
  if (model_.composition_active()) {
    const auto committed = std::string_view{model_.text()};
    const auto before = committed.substr(0, model_.selection_begin());
    const auto after = committed.substr(model_.selection_end());
    const auto preedit = std::string_view{model_.composition_text()};
    const auto preedit_cursor = preedit.substr(
        0, std::min(model_.composition_cursor_byte(), preedit.size()));

    caret = ctx.text_width(before, resolved.text_size) +
            ctx.text_width(preedit_cursor, resolved.text_size);
    total = ctx.text_width(before, resolved.text_size) +
            ctx.text_width(preedit, resolved.text_size) +
            ctx.text_width(after, resolved.text_size);
  }
  constexpr float margin = 4.0f;

  if (caret - scroll_x_ < margin)
    scroll_x_ = std::max(0.0f, caret - margin);
  if (caret - scroll_x_ > content.w - margin) {
    scroll_x_ = caret - content.w + margin;
  }
  scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, total - content.w));

  const float cursor_offset = resolved.horizontal_padding +
                              std::clamp(caret - scroll_x_, 0.0f, content.w);
  ctx.set_text_input(true, field, cursor_offset);
}

void TextInputComponent::commit(InputContext &ctx) {
  const auto session = session_;
  const auto source = state_;
  const std::string expected = source.get();
  const auto generation = ++session->storage_->edit_generation;
  const auto buffer = ++session->storage_->buffer_generation;
  auto snapshot = detail::TextInputAccess::snapshot(*this);
  snapshot.mutation_guard = detail::InputMutationAccess::guard(ctx);
  auto value = snapshot.text;
  auto callback =
      policy_ ? policy_->committed_edit : decltype(policy_->committed_edit){};
  if (!session->mounted() || session->storage_->edit_generation != generation ||
      session->storage_->buffer_generation != buffer)
    return;
  ensure_cursor_visible(ctx);
  ctx.invalidate();
  if (!session->mounted() || session->storage_->edit_generation != generation ||
      session->storage_->buffer_generation != buffer || !source.valid() ||
      source.get() != expected || !detail::InputMutationAccess::allowed(ctx))
    return;
  // Binding publication may throw after committing. The durable generation is
  // deliberately retained; clients reconcile caches without replaying setters.
  auto writer = source;
  writer.set(std::move(value));
  if (session->mounted() && session->storage_->edit_generation == generation &&
      session->storage_->buffer_generation == buffer && source.valid() &&
      source.get() == snapshot.text && callback)
    callback(std::move(snapshot));
}

TextInput::TextInput(std::string label, Binding<std::string> state)
    : label_(std::move(label)), state_(std::move(state)) {}

TextInput::TextInput(std::string label, State<std::string> &state)
    : TextInput(std::move(label), state.binding()) {}

TextInput &&TextInput::placeholder(std::string value) && {
  placeholder_ = std::move(value);
  return std::move(*this);
}

TextInput &&TextInput::max_length(std::size_t value) && {
  max_length_ = value;
  return std::move(*this);
}

TextInput &&TextInput::on_submit(SubmitCallback callback) && {
  on_submit_ = std::move(callback);
  return std::move(*this);
}

TextInput &&TextInput::on_key_down(KeyDownCallback callback) && {
  on_key_down_ = std::move(callback);
  return std::move(*this);
}

TextInput &&TextInput::style(TextInputStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec TextInput::spec() && {
  auto label = std::move(label_);
  auto state = std::move(state_);
  auto placeholder = std::move(placeholder_);
  auto callback = std::move(on_submit_);
  auto key_down = std::move(on_key_down_);
  auto style = std::move(style_);
  const auto max_length = max_length_;
  return Spec{[label = std::move(label), state = std::move(state),
               placeholder = std::move(placeholder), max_length,
               callback = std::move(callback), key_down = std::move(key_down),
               style = std::move(style)]() mutable {
                return std::make_unique<TextInputComponent>(
                    std::move(label), std::move(state), std::move(placeholder),
                    max_length, std::move(callback), std::move(key_down),
                    std::move(style));
              },
              {}};
}

} // namespace ui
