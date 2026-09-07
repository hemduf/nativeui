#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {

namespace text {

[[nodiscard]] inline bool continuation(unsigned char c) noexcept {
    return (c & 0xC0u) == 0x80u;
}

[[nodiscard]] inline std::size_t clamp_boundary(std::string_view value, std::size_t index) noexcept {
    if (index >= value.size()) return value.size();
    while (index > 0 && continuation(static_cast<unsigned char>(value[index]))) --index;
    return index;
}

[[nodiscard]] inline std::size_t next_codepoint(std::string_view value, std::size_t index) noexcept {
    if (index >= value.size()) return value.size();
    ++index;
    while (index < value.size() && continuation(static_cast<unsigned char>(value[index]))) ++index;
    return index;
}

[[nodiscard]] inline std::size_t previous_codepoint(std::string_view value, std::size_t index) noexcept {
    if (index == 0) return 0;
    index = std::min(index, value.size());
    --index;
    while (index > 0 && continuation(static_cast<unsigned char>(value[index]))) --index;
    return index;
}

[[nodiscard]] inline std::size_t codepoint_count(std::string_view value) noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < value.size(); i = next_codepoint(value, i)) ++count;
    return count;
}

[[nodiscard]] inline std::string truncate_codepoints(std::string_view value, std::size_t max_count) {
    std::size_t i = 0;
    std::size_t count = 0;
    while (i < value.size() && count < max_count) {
        i = next_codepoint(value, i);
        ++count;
    }
    return std::string{value.substr(0, i)};
}

[[nodiscard]] inline std::string single_line(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    bool pending_space = false;
    for (std::size_t i = 0; i < input.size();) {
        const auto next = next_codepoint(input, i);
        const auto cp = input.substr(i, next - i);
        if (cp.size() == 1) {
            const unsigned char c = static_cast<unsigned char>(cp.front());
            if (c == '\r' || c == '\n' || c == '\t') {
                pending_space = true;
                i = next;
                continue;
            }
            if (c < 0x20u || c == 0x7Fu) {
                i = next;
                continue;
            }
        }
        if (pending_space && !out.empty() && out.back() != ' ') out.push_back(' ');
        pending_space = false;
        out.append(cp);
        i = next;
    }
    return out;
}

enum class CharClass { Space, Word, Punctuation };

[[nodiscard]] inline CharClass char_class_at(std::string_view value, std::size_t index) noexcept {
    index = clamp_boundary(value, index);
    if (index >= value.size()) return CharClass::Space;
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if (c >= 0x80u) return CharClass::Word;
    if (std::isspace(c)) return CharClass::Space;
    if (std::isalnum(c) || c == '_') return CharClass::Word;
    return CharClass::Punctuation;
}

[[nodiscard]] inline std::size_t previous_word(std::string_view value, std::size_t cursor) noexcept {
    std::size_t i = clamp_boundary(value, std::min(cursor, value.size()));
    while (i > 0) {
        const auto p = previous_codepoint(value, i);
        if (char_class_at(value, p) != CharClass::Space) break;
        i = p;
    }
    if (i == 0) return 0;
    auto p = previous_codepoint(value, i);
    const auto cls = char_class_at(value, p);
    i = p;
    while (i > 0) {
        p = previous_codepoint(value, i);
        if (char_class_at(value, p) != cls) break;
        i = p;
    }
    return i;
}

[[nodiscard]] inline std::size_t next_word(std::string_view value, std::size_t cursor) noexcept {
    std::size_t i = clamp_boundary(value, std::min(cursor, value.size()));
    if (i >= value.size()) return value.size();
    const auto cls = char_class_at(value, i);
    while (i < value.size() && char_class_at(value, i) == cls) i = next_codepoint(value, i);
    while (i < value.size() && char_class_at(value, i) == CharClass::Space) i = next_codepoint(value, i);
    return i;
}

[[nodiscard]] inline std::pair<std::size_t, std::size_t>
word_bounds(std::string_view value, std::size_t index) noexcept {
    if (value.empty()) return {0, 0};
    index = clamp_boundary(value, std::min(index, value.size()));
    if (index >= value.size()) index = previous_codepoint(value, value.size());
    const auto cls = char_class_at(value, index);
    std::size_t begin = index;
    while (begin > 0) {
        const auto p = previous_codepoint(value, begin);
        if (char_class_at(value, p) != cls) break;
        begin = p;
    }
    std::size_t end = index;
    while (end < value.size() && char_class_at(value, end) == cls) end = next_codepoint(value, end);
    return {begin, end};
}

[[nodiscard]] inline std::size_t line_start(std::string_view value, std::size_t index) noexcept {
    std::size_t i = clamp_boundary(value, std::min(index, value.size()));
    while (i > 0) {
        const auto previous = previous_codepoint(value, i);
        if (value[previous] == '\n') break;
        i = previous;
    }
    return i;
}

[[nodiscard]] inline std::size_t line_end(std::string_view value, std::size_t index) noexcept {
    std::size_t i = clamp_boundary(value, std::min(index, value.size()));
    while (i < value.size() && value[i] != '\n') i = next_codepoint(value, i);
    return i;
}

[[nodiscard]] inline std::size_t line_column(std::string_view value, std::size_t index) noexcept {
    const auto bounded = clamp_boundary(value, std::min(index, value.size()));
    const auto begin = line_start(value, bounded);
    return codepoint_count(value.substr(begin, bounded - begin));
}

[[nodiscard]] inline std::size_t index_at_line_column(
    std::string_view value,
    std::size_t begin,
    std::size_t end,
    std::size_t column) noexcept {
    begin = clamp_boundary(value, std::min(begin, value.size()));
    end = clamp_boundary(value, std::min(end, value.size()));
    std::size_t i = std::min(begin, end);
    std::size_t current = 0;
    while (i < end && current < column) {
        i = next_codepoint(value, i);
        ++current;
    }
    return std::min(i, end);
}

} // namespace text

enum class TextMotion {
    Codepoint,
    Word,
    Document,
};

class TextEditModel {
public:
    struct Snapshot {
        std::string text;
        std::size_t cursor{};
        std::size_t anchor{};

        [[nodiscard]] bool operator==(const Snapshot&) const = default;
    };

    explicit TextEditModel(std::string value = {}, std::size_t max_length = 0)
        : text_(std::move(value)),
          max_length_(max_length == 0 ? std::numeric_limits<std::size_t>::max() : max_length),
          cursor_(text_.size()),
          anchor_(text_.size()) {}

    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] std::size_t anchor() const noexcept { return anchor_; }
    [[nodiscard]] std::size_t max_length() const noexcept { return max_length_; }

    [[nodiscard]] bool has_selection() const noexcept { return cursor_ != anchor_; }
    [[nodiscard]] std::size_t selection_begin() const noexcept { return std::min(cursor_, anchor_); }
    [[nodiscard]] std::size_t selection_end() const noexcept { return std::max(cursor_, anchor_); }
    [[nodiscard]] std::string_view selected_text() const noexcept {
        return std::string_view{text_}.substr(selection_begin(), selection_end() - selection_begin());
    }

    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }

    [[nodiscard]] bool composition_active() const noexcept { return composition_active_; }
    [[nodiscard]] const std::string& composition_text() const noexcept { return composition_text_; }
    [[nodiscard]] std::size_t composition_cursor_byte() const noexcept { return composition_cursor_byte_; }
    [[nodiscard]] std::size_t composition_selection_bytes() const noexcept { return composition_selection_bytes_; }

    void begin_composition() {
        composition_start_ = snapshot();
        composition_text_.clear();
        composition_cursor_byte_ = 0;
        composition_selection_bytes_ = 0;
        composition_active_ = true;
    }

    void update_composition(std::string preedit, std::size_t cursor_byte, std::size_t selection_bytes) {
        if (!composition_active_) begin_composition();
        composition_text_ = std::move(preedit);
        composition_cursor_byte_ =
            text::clamp_boundary(composition_text_, std::min(cursor_byte, composition_text_.size()));
        composition_selection_bytes_ =
            text::clamp_boundary(composition_text_, std::min(selection_bytes, composition_text_.size()));
    }

    [[nodiscard]] bool commit_composition(std::string_view committed) {
        if (!composition_active_) return false;

        const auto start = composition_start_;
        clear_composition_state();

        // Editing while a composition is active must cancel it first. Refuse a
        // stale replacement range rather than applying it to changed contents.
        if (text_ != start.text) return false;

        cursor_ = text::clamp_boundary(text_, std::min(start.cursor, text_.size()));
        anchor_ = text::clamp_boundary(text_, std::min(start.anchor, text_.size()));
        vertical_column_.reset();

        const auto selected = text::codepoint_count(selected_text());
        const auto current = text::codepoint_count(text_);
        const auto remaining_base = current >= selected ? current - selected : 0;
        if (remaining_base >= max_length_) return false;

        const auto available = max_length_ - remaining_base;
        auto accepted = text::truncate_codepoints(committed, available);
        if (accepted.empty()) return false;

        checkpoint();
        erase_selection_untracked();
        text_.insert(cursor_, accepted);
        cursor_ += accepted.size();
        anchor_ = cursor_;
        vertical_column_.reset();
        return true;
    }

    void cancel_composition() noexcept {
        if (!composition_active_) return;
        const auto start = composition_start_;
        clear_composition_state();

        if (text_ == start.text) {
            cursor_ = text::clamp_boundary(text_, std::min(start.cursor, text_.size()));
            anchor_ = text::clamp_boundary(text_, std::min(start.anchor, text_.size()));
            vertical_column_.reset();
        }
    }

    void set_max_length(std::size_t max_length) noexcept {
        max_length_ = max_length == 0 ? std::numeric_limits<std::size_t>::max() : max_length;
    }

    void set_text(std::string value, bool preserve_selection = true, bool clear_history = true) {
        text_ = std::move(value);
        if (preserve_selection) {
            cursor_ = text::clamp_boundary(text_, std::min(cursor_, text_.size()));
            anchor_ = text::clamp_boundary(text_, std::min(anchor_, text_.size()));
        } else {
            cursor_ = text_.size();
            anchor_ = cursor_;
        }
        vertical_column_.reset();
        if (clear_history) this->clear_history();
    }

    void clear_history() noexcept {
        undo_.clear();
        redo_.clear();
    }

    void move_to(std::size_t target, bool extend = false) noexcept {
        set_cursor(target, extend, true);
    }

    void select_range(std::size_t anchor, std::size_t cursor) noexcept {
        anchor_ = text::clamp_boundary(text_, std::min(anchor, text_.size()));
        cursor_ = text::clamp_boundary(text_, std::min(cursor, text_.size()));
        vertical_column_.reset();
    }

    void select_all() noexcept {
        anchor_ = 0;
        cursor_ = text_.size();
        vertical_column_.reset();
    }

    void select_word_at(std::size_t byte_index) noexcept {
        const auto [begin, end] = text::word_bounds(text_, byte_index);
        anchor_ = begin;
        cursor_ = end;
        vertical_column_.reset();
    }

    void collapse_to_begin() noexcept { move_to(selection_begin()); }
    void collapse_to_end() noexcept { move_to(selection_end()); }

    void move_left(TextMotion motion = TextMotion::Codepoint, bool extend = false) noexcept {
        if (!extend && motion == TextMotion::Codepoint && has_selection()) {
            collapse_to_begin();
            return;
        }

        std::size_t target = cursor_;
        switch (motion) {
            case TextMotion::Codepoint: target = text::previous_codepoint(text_, cursor_); break;
            case TextMotion::Word: target = text::previous_word(text_, cursor_); break;
            case TextMotion::Document: target = 0; break;
        }
        move_to(target, extend);
    }

    void move_right(TextMotion motion = TextMotion::Codepoint, bool extend = false) noexcept {
        if (!extend && motion == TextMotion::Codepoint && has_selection()) {
            collapse_to_end();
            return;
        }

        std::size_t target = cursor_;
        switch (motion) {
            case TextMotion::Codepoint: target = text::next_codepoint(text_, cursor_); break;
            case TextMotion::Word: target = text::next_word(text_, cursor_); break;
            case TextMotion::Document: target = text_.size(); break;
        }
        move_to(target, extend);
    }

    void move_line_start(bool extend = false) noexcept {
        set_cursor(text::line_start(text_, cursor_), extend, true);
    }

    void move_line_end(bool extend = false) noexcept {
        set_cursor(text::line_end(text_, cursor_), extend, true);
    }

    void move_up(bool extend = false) noexcept {
        move_vertical(false, extend);
    }

    void move_down(bool extend = false) noexcept {
        move_vertical(true, extend);
    }

    [[nodiscard]] bool insert(std::string_view incoming) {
        if (incoming.empty()) return false;
        if (composition_active_) cancel_composition();

        const auto selected = text::codepoint_count(selected_text());
        const auto current = text::codepoint_count(text_);
        const auto remaining_base = current >= selected ? current - selected : 0;
        if (remaining_base >= max_length_) return false;

        const auto available = max_length_ - remaining_base;
        auto accepted = text::truncate_codepoints(incoming, available);
        if (accepted.empty()) return false;

        checkpoint();
        erase_selection_untracked();
        text_.insert(cursor_, accepted);
        cursor_ += accepted.size();
        anchor_ = cursor_;
        vertical_column_.reset();
        return true;
    }

    [[nodiscard]] bool erase_selection() {
        if (!has_selection()) return false;
        checkpoint();
        erase_selection_untracked();
        return true;
    }

    [[nodiscard]] bool backspace(TextMotion motion = TextMotion::Codepoint) {
        if (!has_selection() && cursor_ == 0) return false;
        checkpoint();
        if (has_selection()) {
            erase_selection_untracked();
            return true;
        }

        std::size_t begin = cursor_;
        switch (motion) {
            case TextMotion::Codepoint: begin = text::previous_codepoint(text_, cursor_); break;
            case TextMotion::Word: begin = text::previous_word(text_, cursor_); break;
            case TextMotion::Document: begin = 0; break;
        }
        text_.erase(begin, cursor_ - begin);
        cursor_ = begin;
        anchor_ = cursor_;
        vertical_column_.reset();
        return true;
    }

    [[nodiscard]] bool delete_forward(TextMotion motion = TextMotion::Codepoint) {
        if (!has_selection() && cursor_ >= text_.size()) return false;
        checkpoint();
        if (has_selection()) {
            erase_selection_untracked();
            return true;
        }

        std::size_t end = cursor_;
        switch (motion) {
            case TextMotion::Codepoint: end = text::next_codepoint(text_, cursor_); break;
            case TextMotion::Word: end = text::next_word(text_, cursor_); break;
            case TextMotion::Document: end = text_.size(); break;
        }
        text_.erase(cursor_, end - cursor_);
        anchor_ = cursor_;
        vertical_column_.reset();
        return true;
    }

    [[nodiscard]] bool undo() {
        if (undo_.empty()) return false;
        redo_.push_back(snapshot());
        restore(undo_.back());
        undo_.pop_back();
        trim_history(redo_);
        return true;
    }

    [[nodiscard]] bool redo() {
        if (redo_.empty()) return false;
        undo_.push_back(snapshot());
        restore(redo_.back());
        redo_.pop_back();
        trim_history(undo_);
        return true;
    }

    [[nodiscard]] Snapshot snapshot() const { return Snapshot{text_, cursor_, anchor_}; }

private:
    static constexpr std::size_t kHistoryLimit = 64;

    void clear_composition_state() noexcept {
        composition_active_ = false;
        composition_text_.clear();
        composition_cursor_byte_ = 0;
        composition_selection_bytes_ = 0;
    }

    static void trim_history(std::vector<Snapshot>& history) {
        if (history.size() > kHistoryLimit) {
            history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(history.size() - kHistoryLimit));
        }
    }

    void restore(const Snapshot& state) {
        text_ = state.text;
        cursor_ = text::clamp_boundary(text_, std::min(state.cursor, text_.size()));
        anchor_ = text::clamp_boundary(text_, std::min(state.anchor, text_.size()));
        vertical_column_.reset();
    }

    void checkpoint() {
        const auto current = snapshot();
        if (undo_.empty() || !(undo_.back() == current)) {
            undo_.push_back(current);
            trim_history(undo_);
        }
        redo_.clear();
    }

    void set_cursor(std::size_t target, bool extend, bool reset_vertical) noexcept {
        cursor_ = text::clamp_boundary(text_, std::min(target, text_.size()));
        if (!extend) anchor_ = cursor_;
        if (reset_vertical) vertical_column_.reset();
    }

    void move_vertical(bool down, bool extend) noexcept {
        const auto preferred = vertical_column_.value_or(text::line_column(text_, cursor_));
        vertical_column_ = preferred;

        const auto current_start = text::line_start(text_, cursor_);
        const auto current_end = text::line_end(text_, cursor_);
        std::size_t target = cursor_;

        if (down) {
            if (current_end >= text_.size()) {
                target = text_.size();
            } else {
                const auto next_start = text::next_codepoint(text_, current_end);
                const auto next_end = text::line_end(text_, next_start);
                target = text::index_at_line_column(text_, next_start, next_end, preferred);
            }
        } else if (current_start == 0) {
            target = 0;
        } else {
            const auto previous_end = text::previous_codepoint(text_, current_start);
            const auto previous_start = text::line_start(text_, previous_end);
            target = text::index_at_line_column(text_, previous_start, previous_end, preferred);
        }

        set_cursor(target, extend, false);
    }

    void erase_selection_untracked() {
        if (!has_selection()) return;
        const auto begin = selection_begin();
        const auto end = selection_end();
        text_.erase(begin, end - begin);
        cursor_ = begin;
        anchor_ = begin;
        vertical_column_.reset();
    }

    std::string text_;
    std::size_t max_length_{};
    std::size_t cursor_{};
    std::size_t anchor_{};
    std::optional<std::size_t> vertical_column_;
    bool composition_active_{};
    Snapshot composition_start_{};
    std::string composition_text_;
    std::size_t composition_cursor_byte_{};
    std::size_t composition_selection_bytes_{};
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
};

} // namespace ui
