class TextInputComponent final : public Component {
public:
    using SubmitCallback = std::function<void(const std::string&)>;

    TextInputComponent(
        std::string label,
        State<std::string>& state,
        std::string placeholder,
        std::size_t max_length,
        SubmitCallback on_submit)
        : label_(std::move(label)),
          state_(state),
          model_(state.get(), max_length),
          placeholder_(std::move(placeholder)),
          on_submit_(std::move(on_submit)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return Size{420.0f, 82.0f};
    }

    void mount(MountContext& ctx) override {
        auto invalidate = ctx.invalidator();
        subscription_ = state_.observe(
            [this, invalidate = std::move(invalidate)](const std::string& value) {
                if (value == model_.text()) return;
                model_.set_text(value, true, true);
                scroll_x_ = 0.0f;
                caret_visible_ = true;
                invalidate();
            });
    }

    void focus_changed(bool focused, FocusContext& ctx) override {
        const bool was_focused = focused_;
        focused_ = focused;
        caret_visible_ = true;
        if (focused_) {
            if (!was_focused) focus_snapshot_ = model_.text();
            ensure_cursor_visible(ctx);
        } else {
            drag_select_ = false;
            ctx.set_text_input(false);
        }
        ctx.invalidate();
    }

    EventResult input(const InputEvent& event, InputContext& ctx) override {
        if (event.type == InputType::Tick) {
            if (!focused_) return EventResult::Ignored;
            caret_visible_ = !caret_visible_;
            ctx.invalidate();
            return EventResult::Handled;
        }

        caret_visible_ = true;

        if (event.type == InputType::TextInput) {
            if (event.text.empty()) return EventResult::Ignored;
            insert_text(event.text, ctx);
            return EventResult::Handled;
        }

        if (event.type == InputType::PointerDown) {
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
            ctx.invalidate();
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
                    if (model_.has_selection()) {
                        copy_selection(ctx);
                        if (model_.erase_selection()) commit(ctx);
                    }
                    return EventResult::Handled;
                case Command::Paste:
                    ctx.request_clipboard_text();
                    return EventResult::Handled;
                case Command::Undo:
                    if (model_.undo()) commit(ctx);
                    return EventResult::Handled;
                case Command::Redo:
                    if (model_.redo()) commit(ctx);
                    return EventResult::Handled;
                case Command::None:
                    return EventResult::Ignored;
            }
        }

        if (event.type != InputType::KeyDown) return EventResult::Ignored;

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
                backspace(event, ctx);
                return EventResult::Handled;
            case Key::Delete:
                delete_forward(event, ctx);
                return EventResult::Handled;
            case Key::Enter:
                focus_snapshot_ = model_.text();
                if (on_submit_) on_submit_(model_.text());
                model_.move_to(model_.cursor());
                ctx.invalidate();
                return EventResult::Handled;
            case Key::Escape:
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
        constexpr float text_size = kTextSize;

        dl.text(
            Point{b.x, b.y + 10.0f},
            label_,
            12.0f,
            colors::textMuted,
            TextAlign::Left);

        const auto field = field_rect(b);
        dl.fill_rounded_rect(field, 9.0f, colors::input);
        dl.stroke_rounded_rect(
            field,
            9.0f,
            p.focused() ? 2.0f : 1.0f,
            p.focused() ? colors::borderFocus : colors::border);

        const auto content = content_rect(field);
        dl.push_clip(content);

        const float base_x = content.x - scroll_x_;
        const float center_y = field.y + field.h * 0.5f;

        if (model_.has_selection()) {
            const auto begin = model_.selection_begin();
            const auto end = model_.selection_end();
            const float x1 = base_x + p.text_width(prefix(begin), text_size);
            const float x2 = base_x + p.text_width(prefix(end), text_size);
            dl.fill_rounded_rect(
                Rect{x1, field.y + 8.0f, std::max(1.0f, x2 - x1), field.h - 16.0f},
                3.0f,
                colors::selection);
        }

        if (model_.text().empty() && !placeholder_.empty()) {
            dl.text(
                Point{base_x, center_y},
                placeholder_,
                text_size,
                colors::textMuted,
                TextAlign::Left);
        } else {
            dl.text(
                Point{base_x, center_y},
                model_.text(),
                text_size,
                colors::text,
                TextAlign::Left);
        }

        if (p.focused() && caret_visible_) {
            const float caret_x = base_x + p.text_width(prefix(model_.cursor()), text_size);
            dl.line(
                Point{caret_x, field.y + 9.0f},
                Point{caret_x, field.y + field.h - 9.0f},
                1.5f,
                colors::caret);
        }

        dl.pop_clip();
    }

private:
    static constexpr float kTextSize = 15.0f;
    static constexpr float kHorizontalPadding = 12.0f;

    [[nodiscard]] static Rect field_rect(Rect bounds) noexcept {
        return Rect{bounds.x, bounds.y + 24.0f, bounds.w, 46.0f};
    }

    [[nodiscard]] static Rect content_rect(Rect field) noexcept {
        return Rect{
            field.x + kHorizontalPadding,
            field.y + 4.0f,
            std::max(1.0f, field.w - kHorizontalPadding * 2.0f),
            std::max(1.0f, field.h - 8.0f)};
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
        const auto field = field_rect(ctx.bounds());
        const auto content = content_rect(field);
        const float wanted = std::max(0.0f, x - content.x + scroll_x_);

        std::size_t previous = 0;
        float previous_width = 0.0f;
        for (std::size_t i = text::next_codepoint(model_.text(), 0);
             i <= model_.text().size() && i > previous;
             i = (i == model_.text().size() ? model_.text().size() + 1 : text::next_codepoint(model_.text(), i))) {
            const auto bounded = std::min(i, model_.text().size());
            const float width = ctx.text_width(prefix(bounded), kTextSize);
            if (wanted < (previous_width + width) * 0.5f) return previous;
            previous = bounded;
            previous_width = width;
            if (bounded == model_.text().size()) break;
        }
        return model_.text().size();
    }

    template <class Context>
    void ensure_cursor_visible(Context& ctx) {
        const auto field = field_rect(ctx.bounds());
        const auto content = content_rect(field);
        const float caret = ctx.text_width(prefix(model_.cursor()), kTextSize);
        const float total = ctx.text_width(model_.text(), kTextSize);
        constexpr float margin = 4.0f;

        if (caret - scroll_x_ < margin) scroll_x_ = std::max(0.0f, caret - margin);
        if (caret - scroll_x_ > content.w - margin) {
            scroll_x_ = caret - content.w + margin;
        }
        scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, total - content.w));

        const float cursor_offset =
            kHorizontalPadding + std::clamp(caret - scroll_x_, 0.0f, content.w);
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

    bool drag_select_{};
    bool focused_{};
    bool caret_visible_{true};
    float scroll_x_{};
    std::string focus_snapshot_;
    State<std::string>::Subscription subscription_;
};
