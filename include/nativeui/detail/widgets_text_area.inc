class TextAreaComponent final : public Component {
public:
    TextAreaComponent(
        std::string label,
        State<std::string>& state,
        std::string placeholder,
        std::size_t max_length)
        : label_(std::move(label)),
          state_(state),
          model_(state.get(), max_length),
          placeholder_(std::move(placeholder)) {
        rebuild_lines();
    }

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return Size{420.0f, 180.0f};
    }

    void mount(MountContext& ctx) override {
        auto invalidate = ctx.invalidator();
        subscription_ = state_.observe(
            [this, invalidate = std::move(invalidate)](const std::string& value) {
                if (value == model_.text()) return;
                model_.set_text(value, true, true);
                rebuild_lines();
                scroll_x_ = 0.0f;
                scroll_y_ = 0.0f;
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
            model_.cancel_composition();
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

        if (event.type == InputType::Composition) {
            switch (event.composition.type) {
                case CompositionType::Start:
                    model_.begin_composition();
                    ensure_cursor_visible(ctx);
                    ctx.invalidate();
                    return EventResult::Handled;
                case CompositionType::Update:
                    model_.update_composition(
                        event.composition.text,
                        event.composition.cursor_byte,
                        event.composition.selection_bytes);
                    ensure_cursor_visible(ctx);
                    ctx.invalidate();
                    return EventResult::Handled;
                case CompositionType::Commit:
                    if (model_.commit_composition(event.composition.text)) {
                        commit(ctx);
                    } else {
                        ensure_cursor_visible(ctx);
                        ctx.invalidate();
                    }
                    return EventResult::Handled;
                case CompositionType::Cancel:
                    model_.cancel_composition();
                    ensure_cursor_visible(ctx);
                    ctx.invalidate();
                    return EventResult::Handled;
            }
        }

        if (event.type == InputType::TextInput) {
            const auto incoming = normalize_multiline(event.text);
            if (!incoming.empty() && model_.insert(incoming)) commit(ctx);
            return event.text.empty() ? EventResult::Ignored : EventResult::Handled;
        }

        if (event.type == InputType::PointerDown) {
            model_.cancel_composition();
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
            ctx.invalidate();
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
                if (model_.backspace(motion_for(event))) commit(ctx);
                return EventResult::Handled;
            case Key::Delete:
                if (model_.delete_forward(motion_for(event))) commit(ctx);
                return EventResult::Handled;
            case Key::Enter:
                if (model_.insert("\n")) commit(ctx);
                return EventResult::Handled;
            case Key::Escape:
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

        painter.text(
            Point{bounds.x, bounds.y + 10.0f},
            label_,
            12.0f,
            colors::textMuted,
            TextAlign::Left);

        const auto field = field_rect(bounds);
        painter.fill_rounded_rect(field, 9.0f, colors::input);
        painter.stroke_rounded_rect(
            field,
            9.0f,
            p.focused() ? 2.0f : 1.0f,
            p.focused() ? colors::borderFocus : colors::border);

        const auto content = content_rect(field);
        painter.push_clip(content);

        if (model_.text().empty() && !placeholder_.empty()) {
            painter.text(
                Point{content.x, content.y + kLineHeight * 0.5f},
                placeholder_,
                kTextSize,
                colors::textMuted,
                TextAlign::Left);
        } else {
            paint_visible_lines(p, content);
        }

        if (p.focused() && caret_visible_) paint_caret(p, content);
        painter.pop_clip();
    }

private:
    struct LineRange {
        std::size_t begin{};
        std::size_t end{};
    };

    static constexpr float kTextSize = 15.0f;
    static constexpr float kLineHeight = 22.0f;
    static constexpr float kHorizontalPadding = 12.0f;
    static constexpr float kVerticalPadding = 8.0f;

    [[nodiscard]] static Rect field_rect(Rect bounds) noexcept {
        return Rect{bounds.x, bounds.y + 24.0f, bounds.w, std::max(46.0f, bounds.h - 24.0f)};
    }

    [[nodiscard]] static Rect content_rect(Rect field) noexcept {
        return Rect{
            field.x + kHorizontalPadding,
            field.y + kVerticalPadding,
            std::max(1.0f, field.w - kHorizontalPadding * 2.0f),
            std::max(1.0f, field.h - kVerticalPadding * 2.0f)};
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

    [[nodiscard]] std::string_view line_prefix(std::size_t line_index, std::size_t byte_index) const noexcept {
        const auto& line = lines_[std::min(line_index, lines_.size() - 1)];
        const auto bounded = std::clamp(byte_index, line.begin, line.end);
        return std::string_view{model_.text()}.substr(line.begin, bounded - line.begin);
    }

    template <class Context>
    [[nodiscard]] std::size_t index_from_point(Context& ctx, Point point) const {
        const auto content = content_rect(field_rect(ctx.bounds()));
        const float local_y = std::max(0.0f, point.y - content.y + scroll_y_);
        const auto line_index = std::min(
            static_cast<std::size_t>(local_y / kLineHeight),
            lines_.empty() ? std::size_t{0} : lines_.size() - 1);
        const auto& line = lines_[line_index];
        const float wanted = std::max(0.0f, point.x - content.x + scroll_x_);

        std::size_t previous = line.begin;
        float previous_width = 0.0f;
        for (std::size_t i = text::next_codepoint(model_.text(), line.begin);
             i <= line.end && i > previous;
             i = (i == line.end ? line.end + 1 : text::next_codepoint(model_.text(), i))) {
            const auto bounded = std::min(i, line.end);
            const float width = ctx.text_width(line_prefix(line_index, bounded), kTextSize);
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
        const auto content = content_rect(field_rect(ctx.bounds()));
        const auto cursor = model_.composition_active() ? model_.selection_begin() : model_.cursor();
        const auto line_index = line_index_for_cursor(cursor);
        float caret_x = ctx.text_width(line_prefix(line_index, cursor), kTextSize);
        float line_width = ctx.text_width(line_text(line_index), kTextSize);

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

            const float before_width = ctx.text_width(before, kTextSize);
            caret_x = before_width + ctx.text_width(preedit_cursor, kTextSize);
            line_width = before_width + ctx.text_width(preedit, kTextSize) +
                         ctx.text_width(after, kTextSize);
        }

        const float caret_y = static_cast<float>(line_index) * kLineHeight;
        constexpr float margin = 4.0f;

        if (caret_x - scroll_x_ < margin) scroll_x_ = std::max(0.0f, caret_x - margin);
        if (caret_x - scroll_x_ > content.w - margin) {
            scroll_x_ = caret_x - content.w + margin;
        }
        scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, line_width - content.w));

        if (caret_y - scroll_y_ < 0.0f) scroll_y_ = caret_y;
        if (caret_y + kLineHeight - scroll_y_ > content.h) {
            scroll_y_ = caret_y + kLineHeight - content.h;
        }
        const float total_height = static_cast<float>(lines_.size()) * kLineHeight;
        scroll_y_ = std::clamp(scroll_y_, 0.0f, std::max(0.0f, total_height - content.h));

        const float cursor_offset =
            kHorizontalPadding + std::clamp(caret_x - scroll_x_, 0.0f, content.w);
        ctx.set_text_input(true, field_rect(ctx.bounds()), cursor_offset);
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

    void paint_visible_lines(PaintContext& p, Rect content) const {
        if (lines_.empty()) return;
        const auto first = std::min(
            static_cast<std::size_t>(std::max(0.0f, scroll_y_) / kLineHeight),
            lines_.size() - 1);
        const auto visible_count = static_cast<std::size_t>(content.h / kLineHeight) + 2;
        const auto last = std::min(lines_.size(), first + visible_count);
        const float base_x = content.x - scroll_x_;

        for (std::size_t i = first; i < last; ++i) {
            const auto& line = lines_[i];
            const float top = content.y + static_cast<float>(i) * kLineHeight - scroll_y_;
            const float center_y = top + kLineHeight * 0.5f;

            if (model_.has_selection()) {
                const auto selection_begin = model_.selection_begin();
                const auto selection_end = model_.selection_end();
                const auto begin = std::max(selection_begin, line.begin);
                const auto end = std::min(selection_end, line.end);
                if (begin < end || (selection_begin <= line.end && selection_end > line.end)) {
                    const float x1 = base_x + p.text_width(line_prefix(i, begin), kTextSize);
                    float x2 = base_x + p.text_width(line_prefix(i, end), kTextSize);
                    if (selection_end > line.end && line.end < model_.text().size()) {
                        x2 = std::max(x2 + 6.0f, x1 + 1.0f);
                    }
                    p.painter().fill_rounded_rect(
                        Rect{x1, top + 2.0f, std::max(1.0f, x2 - x1), kLineHeight - 4.0f},
                        2.0f,
                        colors::selection);
                }
            }

            p.painter().text(
                Point{base_x, center_y},
                line_text(i),
                kTextSize,
                colors::text,
                TextAlign::Left);
        }
    }

    void paint_caret(PaintContext& p, Rect content) const {
        if (lines_.empty()) return;
        const auto line_index = line_index_for_cursor(model_.cursor());
        const float x = content.x - scroll_x_ +
                        p.text_width(line_prefix(line_index, model_.cursor()), kTextSize);
        const float top = content.y + static_cast<float>(line_index) * kLineHeight - scroll_y_;
        p.painter().line(
            Point{x, top + 2.0f},
            Point{x, top + kLineHeight - 2.0f},
            1.5f,
            colors::caret);
    }

    std::string label_;
    State<std::string>& state_;
    TextEditModel model_;
    std::string placeholder_;
    std::vector<LineRange> lines_;
    bool drag_select_{};
    bool focused_{};
    bool caret_visible_{true};
    float scroll_x_{};
    float scroll_y_{};
    std::string focus_snapshot_;
    State<std::string>::Subscription subscription_;
};