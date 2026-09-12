class TextAreaComponent final : public Component, public detail::ThemeBinding {
public:
    TextAreaComponent(
        std::string label,
        State<std::string>& state,
        std::string placeholder,
        std::size_t max_length,
        TextAreaStyle style)
        : label_(std::move(label)),
          state_(state),
          model_(state.get(), max_length),
          placeholder_(std::move(placeholder)),
          style_(std::move(style)) {
        rebuild_lines();
    }

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto resolved = resolved_style(focused_);
        return Size{resolved.control_width, resolved.control_height};
    }

    void mount(MountContext& ctx) override {
        auto invalidate = ctx.invalidator();
        subscription_ = state_.observe(
            [this, invalidate = std::move(invalidate)](const std::string& value) {
                if (value == model_.text()) return;
                pending_ime_commit_.clear();
                model_.set_text(value, true, true);
                rebuild_lines();
                scroll_x_ = 0.0f;
                scroll_y_ = 0.0f;
                caret_visible_ = true;
                invalidate();
            });
    }

    void focus_changed(bool focused, FocusContext& ctx) override {
        const auto before = presentation_signature(focused_);
        const bool was_focused = focused_;
        focused_ = focused;
        caret_visible_ = true;
        if (focused_) {
            if (!was_focused) focus_snapshot_ = model_.text();
            ensure_cursor_visible(ctx);
        } else {
            pending_ime_commit_.clear();
            model_.cancel_composition();
            drag_select_ = false;
            pressed_ = false;
            ctx.set_text_input(false);
        }
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, ctx, true);
    }

    void deactivate(LifecycleContext& ctx) override {
        const auto before = presentation_signature(focused_);
        hovered_ = false;
        pressed_ = false;
        const auto after = presentation_signature(focused_);
        invalidate_style_transition(before, after, ctx);
    }

    EventResult input(const InputEvent& event, InputContext& ctx) override {
        const bool read_only = effective_read_only();

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
            if (!focused_) return EventResult::Ignored;
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
                    model_.update_composition(
                        event.composition.text,
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
            if (event.text.empty()) return EventResult::Ignored;
            if (read_only) {
                pending_ime_commit_.clear();
                return EventResult::Handled;
            }
            const bool duplicate_commit =
                !pending_ime_commit_.empty() && event.text == pending_ime_commit_;
            pending_ime_commit_.clear();
            if (!duplicate_commit) {
                const auto incoming = normalize_multiline(event.text);
                if (!incoming.empty() && model_.insert(incoming)) commit(ctx);
            }
            return EventResult::Handled;
        }

        if (read_only && model_.composition_active()) {
            pending_ime_commit_.clear();
            model_.cancel_composition();
            ensure_cursor_visible(ctx);
            ctx.invalidate();
        }

        // The platform IME owns navigation/editing keys while marked text is
        // active. Native callbacks will subsequently update, commit or cancel
        // composition; the retained editor must not run a second command path.
        if (event.type == InputType::KeyDown && model_.composition_active()) {
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerDown) {
            const auto before = presentation_signature(focused_);
            pending_ime_commit_.clear();
            model_.cancel_composition();
            hovered_ = true;
            pressed_ = true;
            const auto clicked = index_from_point(ctx, event.position);
            if (event.clicks >= 3) {
                model_.select_all();
            } else if (event.clicks == 2) {
                model_.select_word_at(clicked);
            } else {
                model_.move_to(clicked, event.shift);
            }
            drag_select_ = event.clicks == 1;
            if (drag_select_) ctx.capture_pointer();
            ensure_cursor_visible(ctx);
            const auto after = presentation_signature(focused_);
            invalidate_style_transition(before, after, ctx, true);
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerMove && drag_select_) {
            model_.move_to(index_from_point(ctx, event.position), true);
            ensure_cursor_visible(ctx);
            ctx.invalidate();
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerUp && drag_select_) {
            model_.move_to(index_from_point(ctx, event.position), true);
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
                    if (read_only) return EventResult::Handled;
                    if (model_.has_selection()) {
                        copy_selection(ctx);
                        if (model_.erase_selection()) commit(ctx);
                    }
                    return EventResult::Handled;
                case Command::Paste:
                    if (!read_only) ctx.request_clipboard_text();
                    return EventResult::Handled;
                case Command::Undo:
                    if (!read_only && model_.undo()) commit(ctx);
                    return EventResult::Handled;
                case Command::Redo:
                    if (!read_only && model_.redo()) commit(ctx);
                    return EventResult::Handled;
                case Command::None:
                    return EventResult::Ignored;
            }
        }

        if (event.type != InputType::KeyDown) return EventResult::Ignored;
        pending_ime_commit_.clear();

        switch (event.key) {
            case Key::Left:
                model_.move_left(motion_for(event), event.shift);
                cursor_moved(ctx);
                return EventResult::Handled;
            case Key::Right:
                model_.move_right(motion_for(event), event.shift);
                cursor_moved(ctx);
                return EventResult::Handled;
            case Key::Up:
                model_.move_up(event.shift);
                cursor_moved(ctx);
                return EventResult::Handled;
            case Key::Down:
                model_.move_down(event.shift);
                cursor_moved(ctx);
                return EventResult::Handled;
            case Key::Home:
                if (event.primary || event.ctrl || event.gui) {
                    model_.move_to(0, event.shift);
                } else {
                    model_.move_line_start(event.shift);
                }
                cursor_moved(ctx);
                return EventResult::Handled;
            case Key::End:
                if (event.primary || event.ctrl || event.gui) {
                    model_.move_to(model_.text().size(), event.shift);
                } else {
                    model_.move_line_end(event.shift);
                }
                cursor_moved(ctx);
                return EventResult::Handled;
            case Key::Backspace:
                if (!read_only && model_.backspace(motion_for(event))) commit(ctx);
                return EventResult::Handled;
            case Key::Delete:
                if (!read_only && model_.delete_forward(motion_for(event))) commit(ctx);
                return EventResult::Handled;
            case Key::Enter:
                if (!read_only && model_.insert("\n")) commit(ctx);
                return EventResult::Handled;
            case Key::Escape:
                if (read_only) return EventResult::Handled;
                if (model_.text() != focus_snapshot_) {
                    model_.set_text(focus_snapshot_, false, false);
                    commit(ctx);
                } else {
                    model_.move_to(model_.cursor());
                    cursor_moved(ctx);
                }
                return EventResult::Handled;
            default:
                return EventResult::Ignored;
        }
    }

    void paint(PaintContext& p) const override {
        const auto bounds = p.bounds();
        auto& painter = p.painter();
        const auto resolved = resolved_style(p.focused());
        const bool show_composition = model_.composition_active() && !effective_read_only();

        painter.text(
            Point{bounds.x, bounds.y + resolved.label_offset_y},
            label_,
            make_text_style(resolved, resolved.label_size, resolved.label));

        const auto field = field_rect(bounds, resolved);
        painter.fill_rounded_rect(field, resolved.corner_radius, resolved.field_fill);
        painter.stroke_rounded_rect(
            field,
            resolved.corner_radius,
            resolved.border_width,
            resolved.border);

        const auto content = content_rect(field, resolved);
        painter.push_clip(content);

        if (model_.text().empty() && !show_composition && !placeholder_.empty()) {
            painter.text(
                Point{content.x, content.y + resolved.line_height * 0.5f},
                placeholder_,
                make_text_style(resolved, resolved.text_size, resolved.placeholder));
        } else {
            paint_visible_lines(p, content, show_composition, resolved);
        }

        if (p.focused() && caret_visible_) {
            paint_caret(p, content, show_composition, resolved);
        }
        painter.pop_clip();
    }

private:
    struct LineRange {
        std::size_t begin{};
        std::size_t end{};
    };

    struct PresentationSignature {
        Color field_fill{};
        Color border{};
        Color label{};
        Color text{};
        Color placeholder{};
        Color selection{};
        Color caret{};
        Color composition_underline{};
        float border_width{};
        float corner_radius{};
        float control_width{};
        float control_height{};
        float field_top{};
        float minimum_field_height{};
        float horizontal_padding{};
        float vertical_padding{};
        float label_offset_y{};
        float label_size{};
        float text_size{};
        float line_height{};
        float selection_corner_radius{};
        float selection_vertical_inset{};
        float newline_selection_width{};
        float caret_width{};
        float caret_vertical_inset{};
        float composition_underline_width{};
        float composition_underline_inset{};
        FontWeight text_weight{FontWeight::Regular};
        FontSlant text_slant{FontSlant::Upright};
        std::string_view font_family;
        const std::vector<std::string>* fallback_families{};
    };

    static void apply_patch(PresentationSignature& target,
                            const TextAreaStylePatch& patch) noexcept {
        if (patch.field_fill) target.field_fill = *patch.field_fill;
        if (patch.border) target.border = *patch.border;
        if (patch.label) target.label = *patch.label;
        if (patch.text) target.text = *patch.text;
        if (patch.placeholder) target.placeholder = *patch.placeholder;
        if (patch.selection) target.selection = *patch.selection;
        if (patch.caret) target.caret = *patch.caret;
        if (patch.composition_underline) {
            target.composition_underline = *patch.composition_underline;
        }
        if (patch.border_width) target.border_width = *patch.border_width;
        if (patch.corner_radius) target.corner_radius = *patch.corner_radius;
        if (patch.control_width) target.control_width = *patch.control_width;
        if (patch.control_height) target.control_height = *patch.control_height;
        if (patch.field_top) target.field_top = *patch.field_top;
        if (patch.minimum_field_height) target.minimum_field_height = *patch.minimum_field_height;
        if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
        if (patch.vertical_padding) target.vertical_padding = *patch.vertical_padding;
        if (patch.label_offset_y) target.label_offset_y = *patch.label_offset_y;
        if (patch.label_size) target.label_size = *patch.label_size;
        if (patch.text_size) target.text_size = *patch.text_size;
        if (patch.line_height) target.line_height = *patch.line_height;
        if (patch.selection_corner_radius) {
            target.selection_corner_radius = *patch.selection_corner_radius;
        }
        if (patch.selection_vertical_inset) {
            target.selection_vertical_inset = *patch.selection_vertical_inset;
        }
        if (patch.newline_selection_width) target.newline_selection_width = *patch.newline_selection_width;
        if (patch.caret_width) target.caret_width = *patch.caret_width;
        if (patch.caret_vertical_inset) target.caret_vertical_inset = *patch.caret_vertical_inset;
        if (patch.composition_underline_width) {
            target.composition_underline_width = *patch.composition_underline_width;
        }
        if (patch.composition_underline_inset) {
            target.composition_underline_inset = *patch.composition_underline_inset;
        }
        if (patch.text_weight) target.text_weight = *patch.text_weight;
        if (patch.text_slant) target.text_slant = *patch.text_slant;
        if (patch.font_family) target.font_family = *patch.font_family;
        if (patch.fallback_families) target.fallback_families = &*patch.fallback_families;
    }

    [[nodiscard]] PresentationSignature presentation_signature(bool focused) const noexcept {
        const auto& theme = current_theme();
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
            .corner_radius = 9.0f,
            .control_width = 420.0f,
            .control_height = 180.0f,
            .field_top = 24.0f,
            .minimum_field_height = 46.0f,
            .horizontal_padding = 12.0f,
            .vertical_padding = 8.0f,
            .label_offset_y = 10.0f,
            .label_size = 12.0f,
            .text_size = 15.0f,
            .line_height = 22.0f,
            .selection_corner_radius = 2.0f,
            .selection_vertical_inset = 2.0f,
            .newline_selection_width = 6.0f,
            .caret_width = 1.5f,
            .caret_vertical_inset = 2.0f,
            .composition_underline_width = 1.5f,
            .composition_underline_inset = 3.0f,
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
                result.label = theme.palette.disabled;
                result.text = theme.palette.disabled;
                result.placeholder = theme.palette.disabled;
                result.caret = theme.palette.disabled;
                apply_patch(result, style_.disabled);
                break;
        }
        if (state.read_only) {
            result.text = theme.palette.muted_text;
            result.caret = theme.palette.muted_text;
            apply_patch(result, style_.read_only);
        }
        if (state.focused) {
            result.border = theme.palette.focus;
            result.border_width = theme.controls.focus_ring_width;
            apply_patch(result, style_.focused);
        }
        return result;
    }

    [[nodiscard]] static bool same_color(Color lhs, Color rhs) noexcept {
        return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
    }

    [[nodiscard]] static bool same_fallbacks(const PresentationSignature& lhs,
                                             const PresentationSignature& rhs) {
        return lhs.fallback_families == rhs.fallback_families ||
            (lhs.fallback_families != nullptr && rhs.fallback_families != nullptr &&
             *lhs.fallback_families == *rhs.fallback_families);
    }

    [[nodiscard]] static bool same_layout(const PresentationSignature& lhs,
                                          const PresentationSignature& rhs) {
        return lhs.control_width == rhs.control_width &&
               lhs.control_height == rhs.control_height &&
               lhs.field_top == rhs.field_top &&
               lhs.minimum_field_height == rhs.minimum_field_height &&
               lhs.horizontal_padding == rhs.horizontal_padding &&
               lhs.vertical_padding == rhs.vertical_padding &&
               lhs.label_offset_y == rhs.label_offset_y &&
               lhs.label_size == rhs.label_size &&
               lhs.text_size == rhs.text_size &&
               lhs.line_height == rhs.line_height &&
               lhs.text_weight == rhs.text_weight &&
               lhs.text_slant == rhs.text_slant &&
               lhs.font_family == rhs.font_family && same_fallbacks(lhs, rhs);
    }

    [[nodiscard]] static bool same_presentation(const PresentationSignature& lhs,
                                                const PresentationSignature& rhs) {
        return same_color(lhs.field_fill, rhs.field_fill) &&
               same_color(lhs.border, rhs.border) &&
               same_color(lhs.label, rhs.label) &&
               same_color(lhs.text, rhs.text) &&
               same_color(lhs.placeholder, rhs.placeholder) &&
               same_color(lhs.selection, rhs.selection) &&
               same_color(lhs.caret, rhs.caret) &&
               same_color(lhs.composition_underline, rhs.composition_underline) &&
               lhs.border_width == rhs.border_width &&
               lhs.corner_radius == rhs.corner_radius &&
               lhs.selection_corner_radius == rhs.selection_corner_radius &&
               lhs.selection_vertical_inset == rhs.selection_vertical_inset &&
               lhs.newline_selection_width == rhs.newline_selection_width &&
               lhs.caret_width == rhs.caret_width &&
               lhs.caret_vertical_inset == rhs.caret_vertical_inset &&
               lhs.composition_underline_width == rhs.composition_underline_width &&
               lhs.composition_underline_inset == rhs.composition_underline_inset &&
               same_layout(lhs, rhs);
    }

    template <class Context>
    static void invalidate_style_transition(const PresentationSignature& before,
                                            const PresentationSignature& after,
                                            Context& context,
                                            bool require_paint = false) {
        if (!same_layout(before, after)) {
            context.invalidate_layout();
            return;
        }
        if (require_paint || !same_presentation(before, after)) context.invalidate();
    }

    [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept {
        return VisualState{
            .enabled = effective_enabled(),
            .read_only = effective_read_only(),
            .hovered = hovered_,
            .pressed = pressed_,
            .focused = focused,
        };
    }

    [[nodiscard]] ResolvedTextAreaStyle resolved_style(bool focused) const {
        return resolve_text_area_style(
            default_text_area_style(current_theme()), style_, current_visual_state(focused));
    }

    [[nodiscard]] static TextStyle make_text_style(
        const ResolvedTextAreaStyle& style,
        float size,
        Color color) {
        TextStyle text_style{};
        text_style.size = size;
        text_style.color = color;
        text_style.align = TextAlign::Left;
        text_style.weight = style.text_weight;
        text_style.slant = style.text_slant;
        text_style.family = style.font_family;
        text_style.fallback_families = style.fallback_families;
        return text_style;
    }

    [[nodiscard]] static Rect field_rect(
        Rect bounds,
        const ResolvedTextAreaStyle& style) noexcept {
        return Rect{
            bounds.x,
            bounds.y + style.field_top,
            bounds.w,
            std::max(style.minimum_field_height, bounds.h - style.field_top)};
    }

    [[nodiscard]] static Rect content_rect(
        Rect field,
        const ResolvedTextAreaStyle& style) noexcept {
        return Rect{
            field.x + style.horizontal_padding,
            field.y + style.vertical_padding,
            std::max(1.0f, field.w - style.horizontal_padding * 2.0f),
            std::max(1.0f, field.h - style.vertical_padding * 2.0f)};
    }

    [[nodiscard]] static std::string normalize_multiline(std::string_view input) {
        std::string output;
        output.reserve(input.size());
        for (std::size_t i = 0; i < input.size();) {
            const auto next = text::next_codepoint(input, i);
            const auto cp = input.substr(i, next - i);
            if (cp.size() == 1) {
                const auto c = static_cast<unsigned char>(cp.front());
                if (c == '\r') {
                    output.push_back('\n');
                    i = next;
                    if (i < input.size() && input[i] == '\n') ++i;
                    continue;
                }
                if (c == '\n' || c == '\t') {
                    output.push_back(static_cast<char>(c));
                    i = next;
                    continue;
                }
                if (c < 0x20u || c == 0x7fu) {
                    i = next;
                    continue;
                }
            }
            output.append(cp);
            i = next;
        }
        return output;
    }

    [[nodiscard]] static TextMotion motion_for(const InputEvent& event) noexcept {
        if (event.gui) return TextMotion::Document;
        if (event.ctrl || event.alt) return TextMotion::Word;
        return TextMotion::Codepoint;
    }

    void rebuild_lines() {
        lines_.clear();
        std::size_t begin = 0;
        for (std::size_t i = 0; i < model_.text().size(); ++i) {
            if (model_.text()[i] != '\n') continue;
            lines_.push_back(LineRange{begin, i});
            begin = i + 1;
        }
        lines_.push_back(LineRange{begin, model_.text().size()});
    }

    [[nodiscard]] std::size_t line_index_for_cursor(std::size_t cursor) const noexcept {
        if (lines_.empty()) return 0;
        const auto bounded = std::min(cursor, model_.text().size());
        auto it = std::upper_bound(
            lines_.begin(), lines_.end(), bounded,
            [](std::size_t value, const LineRange& line) { return value < line.begin; });
        if (it == lines_.begin()) return 0;
        return static_cast<std::size_t>(std::distance(lines_.begin(), std::prev(it)));
    }

    [[nodiscard]] std::string_view line_text(std::size_t line_index) const noexcept {
        const auto& line = lines_[std::min(line_index, lines_.size() - 1)];
        return std::string_view{model_.text()}.substr(line.begin, line.end - line.begin);
    }

    [[nodiscard]] std::string_view line_prefix(
        std::size_t line_index,
        std::size_t byte_index) const noexcept {
        const auto& line = lines_[std::min(line_index, lines_.size() - 1)];
        const auto bounded = std::clamp(byte_index, line.begin, line.end);
        return std::string_view{model_.text()}.substr(line.begin, bounded - line.begin);
    }

    template <class Context>
    [[nodiscard]] float text_width(
        Context& ctx,
        std::string_view text,
        const ResolvedTextAreaStyle& style) const {
        return ctx.text_metrics(
                      text,
                      make_text_style(style, style.text_size, style.text))
            .width;
    }

    template <class Context>
    [[nodiscard]] std::size_t index_from_point(Context& ctx, Point point) const {
        const auto resolved = resolved_style(focused_);
        const auto content = content_rect(field_rect(ctx.bounds(), resolved), resolved);
        const float line_height = std::max(1.0f, resolved.line_height);
        const float local_y = std::max(0.0f, point.y - content.y + scroll_y_);
        const auto line_index = std::min(
            static_cast<std::size_t>(local_y / line_height),
            lines_.empty() ? std::size_t{0} : lines_.size() - 1);
        const auto& line = lines_[line_index];
        const float wanted = std::max(0.0f, point.x - content.x + scroll_x_);

        std::size_t previous = line.begin;
        float previous_width = 0.0f;
        for (std::size_t i = text::next_codepoint(model_.text(), line.begin);
             i <= line.end && i > previous;
             i = (i == line.end ? line.end + 1 : text::next_codepoint(model_.text(), i))) {
            const auto bounded = std::min(i, line.end);
            const float width = text_width(ctx, line_prefix(line_index, bounded), resolved);
            if (wanted < (previous_width + width) * 0.5f) return previous;
            previous = bounded;
            previous_width = width;
            if (bounded == line.end) break;
        }
        return line.end;
    }

    template <class Context>
    void ensure_cursor_visible(Context& ctx) {
        if (lines_.empty()) rebuild_lines();
        const auto resolved = resolved_style(focused_);
        const auto field = field_rect(ctx.bounds(), resolved);
        const auto content = content_rect(field, resolved);
        const float line_height = std::max(1.0f, resolved.line_height);
        const auto cursor = model_.composition_active() ? model_.selection_begin() : model_.cursor();
        const auto line_index = line_index_for_cursor(cursor);
        float caret_x = text_width(ctx, line_prefix(line_index, cursor), resolved);
        float line_width = text_width(ctx, line_text(line_index), resolved);

        if (model_.composition_active()) {
            const auto& line = lines_[line_index];
            const auto replace_begin = std::clamp(model_.selection_begin(), line.begin, line.end);
            const auto replace_end = std::clamp(model_.selection_end(), line.begin, line.end);
            const auto committed = std::string_view{model_.text()};
            const auto before = committed.substr(line.begin, replace_begin - line.begin);
            const auto after = committed.substr(replace_end, line.end - replace_end);
            const auto preedit = std::string_view{model_.composition_text()};
            const auto preedit_cursor = preedit.substr(
                0,
                std::min(model_.composition_cursor_byte(), preedit.size()));

            const float before_width = text_width(ctx, before, resolved);
            caret_x = before_width + text_width(ctx, preedit_cursor, resolved);
            line_width = before_width + text_width(ctx, preedit, resolved) +
                         text_width(ctx, after, resolved);
        }

        const float caret_y = static_cast<float>(line_index) * line_height;
        constexpr float margin = 4.0f;

        if (caret_x - scroll_x_ < margin) scroll_x_ = std::max(0.0f, caret_x - margin);
        if (caret_x - scroll_x_ > content.w - margin) {
            scroll_x_ = caret_x - content.w + margin;
        }
        scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, line_width - content.w));

        if (caret_y - scroll_y_ < 0.0f) scroll_y_ = caret_y;
        if (caret_y + line_height - scroll_y_ > content.h) {
            scroll_y_ = caret_y + line_height - content.h;
        }
        const float total_height = static_cast<float>(lines_.size()) * line_height;
        scroll_y_ = std::clamp(scroll_y_, 0.0f, std::max(0.0f, total_height - content.h));

        const float cursor_offset =
            resolved.horizontal_padding + std::clamp(caret_x - scroll_x_, 0.0f, content.w);
        const Rect candidate_area{
            field.x,
            content.y + caret_y - scroll_y_,
            field.w,
            line_height};
        ctx.set_text_input(true, candidate_area, cursor_offset);
    }

    void cursor_moved(InputContext& ctx) {
        ensure_cursor_visible(ctx);
        ctx.invalidate();
    }

    void copy_selection(InputContext& ctx) const {
        if (model_.has_selection()) ctx.set_clipboard_text(model_.selected_text());
    }

    void commit(InputContext& ctx) {
        rebuild_lines();
        state_.set(model_.text());
        ensure_cursor_visible(ctx);
        ctx.invalidate();
    }

    void paint_composition_line(
        PaintContext& p,
        std::size_t line_index,
        float base_x,
        float top,
        float center_y,
        const ResolvedTextAreaStyle& style) const {
        const auto& line = lines_[line_index];
        const auto replace_begin = std::clamp(model_.selection_begin(), line.begin, line.end);
        const auto replace_end = std::clamp(model_.selection_end(), line.begin, line.end);
        const auto committed = std::string_view{model_.text()};
        const auto before = committed.substr(line.begin, replace_begin - line.begin);
        const auto after = committed.substr(replace_end, line.end - replace_end);
        const auto preedit = std::string_view{model_.composition_text()};
        const auto body_style = make_text_style(style, style.text_size, style.text);

        float x = base_x;
        if (!before.empty()) {
            p.painter().text(Point{x, center_y}, before, body_style);
            x += text_width(p, before, style);
        }

        if (!preedit.empty()) {
            p.painter().text(Point{x, center_y}, preedit, body_style);
            const float preedit_width = text_width(p, preedit, style);
            const float underline_y = top + style.line_height - style.composition_underline_inset;
            p.painter().line(
                Point{x, underline_y},
                Point{x + std::max(1.0f, preedit_width), underline_y},
                style.composition_underline_width,
                style.composition_underline);
            x += preedit_width;
        }

        if (!after.empty()) {
            p.painter().text(Point{x, center_y}, after, body_style);
        }
    }

    void paint_visible_lines(
        PaintContext& p,
        Rect content,
        bool show_composition,
        const ResolvedTextAreaStyle& style) const {
        if (lines_.empty()) return;
        const float line_height = std::max(1.0f, style.line_height);
        const auto first = std::min(
            static_cast<std::size_t>(std::max(0.0f, scroll_y_) / line_height),
            lines_.size() - 1);
        const auto visible_count = static_cast<std::size_t>(content.h / line_height) + 2;
        const auto last = std::min(lines_.size(), first + visible_count);
        const float base_x = content.x - scroll_x_;
        const auto composition_line = show_composition
                                          ? line_index_for_cursor(model_.selection_begin())
                                          : lines_.size();
        const auto body_style = make_text_style(style, style.text_size, style.text);

        for (std::size_t i = first; i < last; ++i) {
            const auto& line = lines_[i];
            const float top = content.y + static_cast<float>(i) * line_height - scroll_y_;
            const float center_y = top + line_height * 0.5f;

            if (model_.has_selection() && !show_composition) {
                const auto selection_begin = model_.selection_begin();
                const auto selection_end = model_.selection_end();
                const auto begin = std::max(selection_begin, line.begin);
                const auto end = std::min(selection_end, line.end);
                if (begin < end || (selection_begin <= line.end && selection_end > line.end)) {
                    const float x1 = base_x + text_width(p, line_prefix(i, begin), style);
                    float x2 = base_x + text_width(p, line_prefix(i, end), style);
                    if (selection_end > line.end && line.end < model_.text().size()) {
                        x2 = std::max(x2 + style.newline_selection_width, x1 + 1.0f);
                    }
                    p.painter().fill_rounded_rect(
                        Rect{
                            x1,
                            top + style.selection_vertical_inset,
                            std::max(1.0f, x2 - x1),
                            std::max(1.0f, line_height - style.selection_vertical_inset * 2.0f)},
                        style.selection_corner_radius,
                        style.selection);
                }
            }

            if (i == composition_line) {
                paint_composition_line(p, i, base_x, top, center_y, style);
            } else {
                p.painter().text(Point{base_x, center_y}, line_text(i), body_style);
            }
        }
    }

    void paint_caret(
        PaintContext& p,
        Rect content,
        bool show_composition,
        const ResolvedTextAreaStyle& style) const {
        if (lines_.empty()) return;

        std::size_t cursor = model_.cursor();
        if (show_composition) cursor = model_.selection_begin();
        const auto line_index = line_index_for_cursor(cursor);
        float x = content.x - scroll_x_ +
                  text_width(p, line_prefix(line_index, cursor), style);

        if (show_composition) {
            const auto preedit = std::string_view{model_.composition_text()};
            const auto preedit_cursor = preedit.substr(
                0,
                std::min(model_.composition_cursor_byte(), preedit.size()));
            x += text_width(p, preedit_cursor, style);
        }

        const float line_height = std::max(1.0f, style.line_height);
        const float top = content.y + static_cast<float>(line_index) * line_height - scroll_y_;
        p.painter().line(
            Point{x, top + style.caret_vertical_inset},
            Point{x, top + line_height - style.caret_vertical_inset},
            style.caret_width,
            style.caret);
    }

    std::string label_;
    State<std::string>& state_;
    TextEditModel model_;
    std::string placeholder_;
    TextAreaStyle style_;
    std::vector<LineRange> lines_;
    bool drag_select_{};
    bool focused_{};
    bool hovered_{};
    bool pressed_{};
    bool caret_visible_{true};
    float scroll_x_{};
    float scroll_y_{};
    std::string pending_ime_commit_;
    std::string focus_snapshot_;
    State<std::string>::Subscription subscription_;
};