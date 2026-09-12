class TextInputComponent final : public Component, public detail::ThemeBinding {
public:
    using SubmitCallback = std::function<void(const std::string&)>;

    TextInputComponent(
        std::string label,
        State<std::string>& state,
        std::string placeholder,
        std::size_t max_length,
        SubmitCallback on_submit,
        TextInputStyle style)
        : label_(std::move(label)),
          state_(state),
          model_(state.get(), max_length),
          placeholder_(std::move(placeholder)),
          on_submit_(std::move(on_submit)),
          style_(std::move(style)) {}

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
                scroll_x_ = 0.0f;
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
        focused_ = false;
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
            if (!duplicate_commit) insert_text(event.text, ctx);
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
            if (drag_select_) ctx.capture_pointer();
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
                if (!read_only) backspace(event, ctx);
                return EventResult::Handled;
            case Key::Delete:
                if (!read_only) delete_forward(event, ctx);
                return EventResult::Handled;
            case Key::Enter:
                focus_snapshot_ = model_.text();
                if (on_submit_) on_submit_(model_.text());
                model_.move_to(model_.cursor());
                ctx.invalidate();
                return EventResult::Handled;
            case Key::Escape:
                if (read_only) return EventResult::Handled;
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

    void paint(PaintContext& p) const override {
        const auto b = p.bounds();
        auto& dl = p.painter();
        const auto resolved = resolved_style(p.focused());
        const bool show_composition = model_.composition_active() && !effective_read_only();

        TextStyle label_style{};
        label_style.size = resolved.label_size;
        label_style.color = resolved.label;
        label_style.align = TextAlign::Left;
        label_style.weight = resolved.text_weight;
        label_style.slant = resolved.text_slant;
        label_style.family = resolved.font_family;
        label_style.fallback_families = resolved.fallback_families;
        dl.text(Point{b.x, b.y + resolved.label_offset_y}, label_, label_style);

        const auto field = field_rect(b, resolved);
        dl.fill_rounded_rect(field, resolved.corner_radius, resolved.field_fill);
        dl.stroke_rounded_rect(
            field,
            resolved.corner_radius,
            resolved.border_width,
            resolved.border);

        const auto content = content_rect(field, resolved);
        dl.push_clip(content);

        const float base_x = content.x - scroll_x_;
        const float center_y = field.y + field.h * 0.5f;

        if (model_.has_selection() && !show_composition) {
            const auto begin = model_.selection_begin();
            const auto end = model_.selection_end();
            const float x1 = base_x + p.text_width(prefix(begin), resolved.text_size);
            const float x2 = base_x + p.text_width(prefix(end), resolved.text_size);
            dl.fill_rounded_rect(
                Rect{
                    x1,
                    field.y + resolved.selection_vertical_inset,
                    std::max(1.0f, x2 - x1),
                    std::max(1.0f, field.h - resolved.selection_vertical_inset * 2.0f)},
                resolved.selection_corner_radius,
                resolved.selection);
        }

        if (show_composition) {
            const auto committed = std::string_view{model_.text()};
            const auto replace_begin = model_.selection_begin();
            const auto replace_end = model_.selection_end();
            const auto before = committed.substr(0, replace_begin);
            const auto after = committed.substr(replace_end);
            const auto& preedit = model_.composition_text();

            float x = base_x;
            if (!before.empty()) {
                dl.text(Point{x, center_y}, before, resolved.text_size, resolved.text, TextAlign::Left);
                x += p.text_width(before, resolved.text_size);
            }

            if (!preedit.empty()) {
                dl.text(Point{x, center_y}, preedit, resolved.text_size, resolved.text, TextAlign::Left);
                const float preedit_width = p.text_width(preedit, resolved.text_size);
                const float underline_y = field.y + field.h - resolved.composition_underline_inset;
                dl.line(
                    Point{x, underline_y},
                    Point{x + std::max(1.0f, preedit_width), underline_y},
                    resolved.composition_underline_width,
                    resolved.composition_underline);
                x += preedit_width;
            }

            if (!after.empty()) {
                dl.text(Point{x, center_y}, after, resolved.text_size, resolved.text, TextAlign::Left);
            }
        } else if (model_.text().empty() && !placeholder_.empty()) {
            dl.text(
                Point{base_x, center_y},
                placeholder_,
                resolved.text_size,
                resolved.placeholder,
                TextAlign::Left);
        } else {
            dl.text(
                Point{base_x, center_y},
                model_.text(),
                resolved.text_size,
                resolved.text,
                TextAlign::Left);
        }

        if (p.focused() && caret_visible_) {
            float caret_x = base_x + p.text_width(prefix(model_.cursor()), resolved.text_size);
            if (show_composition) {
                const auto before = std::string_view{model_.text()}.substr(0, model_.selection_begin());
                const auto preedit_cursor = std::string_view{model_.composition_text()}.substr(
                    0,
                    std::min(model_.composition_cursor_byte(), model_.composition_text().size()));
                caret_x = base_x + p.text_width(before, resolved.text_size) +
                          p.text_width(preedit_cursor, resolved.text_size);
            }
            dl.line(
                Point{caret_x, field.y + resolved.caret_vertical_inset},
                Point{caret_x, field.y + field.h - resolved.caret_vertical_inset},
                resolved.caret_width,
                resolved.caret);
        }

        dl.pop_clip();
    }

private:
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
        float field_height{};
        float horizontal_padding{};
        float content_vertical_inset{};
        float label_offset_y{};
        float label_size{};
        float text_size{};
        float selection_corner_radius{};
        float selection_vertical_inset{};
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
                            const TextInputStylePatch& patch) noexcept {
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
        if (patch.field_height) target.field_height = *patch.field_height;
        if (patch.horizontal_padding) target.horizontal_padding = *patch.horizontal_padding;
        if (patch.content_vertical_inset) {
            target.content_vertical_inset = *patch.content_vertical_inset;
        }
        if (patch.label_offset_y) target.label_offset_y = *patch.label_offset_y;
        if (patch.label_size) target.label_size = *patch.label_size;
        if (patch.text_size) target.text_size = *patch.text_size;
        if (patch.selection_corner_radius) {
            target.selection_corner_radius = *patch.selection_corner_radius;
        }
        if (patch.selection_vertical_inset) {
            target.selection_vertical_inset = *patch.selection_vertical_inset;
        }
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
        if (state.read_only) apply_patch(result, style_.read_only);
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
               lhs.field_height == rhs.field_height &&
               lhs.horizontal_padding == rhs.horizontal_padding &&
               lhs.content_vertical_inset == rhs.content_vertical_inset &&
               lhs.label_offset_y == rhs.label_offset_y &&
               lhs.label_size == rhs.label_size &&
               lhs.text_size == rhs.text_size &&
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

    [[nodiscard]] ResolvedTextInputStyle resolved_style(bool focused) const {
        return resolve_text_input_style(
            default_text_input_style(current_theme()), style_, current_visual_state(focused));
    }

    [[nodiscard]] static Rect field_rect(
        Rect bounds,
        const ResolvedTextInputStyle& style) noexcept {
        return Rect{bounds.x, bounds.y + style.field_top, bounds.w, style.field_height};
    }

    [[nodiscard]] static Rect content_rect(
        Rect field,
        const ResolvedTextInputStyle& style) noexcept {
        return Rect{
            field.x + style.horizontal_padding,
            field.y + style.content_vertical_inset,
            std::max(1.0f, field.w - style.horizontal_padding * 2.0f),
            std::max(1.0f, field.h - style.content_vertical_inset * 2.0f)};
    }

    [[nodiscard]] std::string_view prefix(std::size_t byte_index) const noexcept {
        return std::string_view{model_.text()}.substr(0, std::min(byte_index, model_.text().size()));
    }

    void copy_selection(InputContext& ctx) const {
        if (!model_.has_selection()) return;
        ctx.set_clipboard_text(model_.selected_text());
    }

    void insert_text(std::string_view raw, InputContext& ctx) {
        const auto incoming = text::single_line(raw);
        if (incoming.empty()) return;
        if (model_.insert(incoming)) commit(ctx);
    }

    [[nodiscard]] static TextMotion motion_for(const InputEvent& event) noexcept {
        if (event.gui) return TextMotion::Document;
        if (event.ctrl || event.alt) return TextMotion::Word;
        return TextMotion::Codepoint;
    }

    void move_left(const InputEvent& event, InputContext& ctx) {
        model_.move_left(motion_for(event), event.shift);
        ensure_cursor_visible(ctx);
        ctx.invalidate();
    }

    void move_right(const InputEvent& event, InputContext& ctx) {
        model_.move_right(motion_for(event), event.shift);
        ensure_cursor_visible(ctx);
        ctx.invalidate();
    }

    void backspace(const InputEvent& event, InputContext& ctx) {
        if (model_.backspace(motion_for(event))) commit(ctx);
    }

    void delete_forward(const InputEvent& event, InputContext& ctx) {
        if (model_.delete_forward(motion_for(event))) commit(ctx);
    }

    template <class Context>
    [[nodiscard]] std::size_t index_from_x(Context& ctx, float x) const {
        const auto resolved = resolved_style(focused_);
        const auto field = field_rect(ctx.bounds(), resolved);
        const auto content = content_rect(field, resolved);
        const float wanted = std::max(0.0f, x - content.x + scroll_x_);

        std::size_t previous = 0;
        float previous_width = 0.0f;
        for (std::size_t i = text::next_codepoint(model_.text(), 0);
             i <= model_.text().size() && i > previous;
             i = (i == model_.text().size() ? model_.text().size() + 1 : text::next_codepoint(model_.text(), i))) {
            const auto bounded = std::min(i, model_.text().size());
            const float width = ctx.text_width(prefix(bounded), resolved.text_size);
            if (wanted < (previous_width + width) * 0.5f) return previous;
            previous = bounded;
            previous_width = width;
            if (bounded == model_.text().size()) break;
        }
        return model_.text().size();
    }

    template <class Context>
    void ensure_cursor_visible(Context& ctx) {
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
                0,
                std::min(model_.composition_cursor_byte(), preedit.size()));

            caret = ctx.text_width(before, resolved.text_size) +
                    ctx.text_width(preedit_cursor, resolved.text_size);
            total = ctx.text_width(before, resolved.text_size) +
                    ctx.text_width(preedit, resolved.text_size) +
                    ctx.text_width(after, resolved.text_size);
        }
        constexpr float margin = 4.0f;

        if (caret - scroll_x_ < margin) scroll_x_ = std::max(0.0f, caret - margin);
        if (caret - scroll_x_ > content.w - margin) {
            scroll_x_ = caret - content.w + margin;
        }
        scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, total - content.w));

        const float cursor_offset =
            resolved.horizontal_padding + std::clamp(caret - scroll_x_, 0.0f, content.w);
        ctx.set_text_input(true, field, cursor_offset);
    }

    void commit(InputContext& ctx) {
        state_.set(model_.text());
        ensure_cursor_visible(ctx);
        ctx.invalidate();
    }

    std::string label_;
    State<std::string>& state_;
    TextEditModel model_;
    std::string placeholder_;
    SubmitCallback on_submit_;
    TextInputStyle style_;

    bool drag_select_{};
    bool focused_{};
    bool hovered_{};
    bool pressed_{};
    bool caret_visible_{true};
    float scroll_x_{};
    std::string focus_snapshot_;
    std::string pending_ime_commit_;
    State<std::string>::Subscription subscription_;
};