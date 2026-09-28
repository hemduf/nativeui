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

/// Test the UTF-8 continuation-byte bit pattern. This classifies one byte only;
/// it does not validate a complete UTF-8 sequence.
[[nodiscard]] inline bool continuation(unsigned char c) noexcept {
    return (c & 0xC0u) == 0x80u;
}

/// Clamp a byte offset to the start of its containing UTF-8 code-point-like
/// sequence; out-of-range offsets clamp to value.size().
[[nodiscard]] inline std::size_t clamp_boundary(std::string_view value, std::size_t index) noexcept {
    if (index >= value.size()) return value.size();
    while (index > 0 && continuation(static_cast<unsigned char>(value[index]))) --index;
    return index;
}

/// Advance one UTF-8 boundary without performing full Unicode validation.
[[nodiscard]] inline std::size_t next_codepoint(std::string_view value, std::size_t index) noexcept {
    if (index >= value.size()) return value.size();
    ++index;
    while (index < value.size() && continuation(static_cast<unsigned char>(value[index]))) ++index;
    return index;
}

/// Move to the previous UTF-8 boundary.
[[nodiscard]] inline std::size_t previous_codepoint(std::string_view value, std::size_t index) noexcept {
    if (index == 0) return 0;
    index = std::min(index, value.size());
    --index;
    while (index > 0 && continuation(static_cast<unsigned char>(value[index]))) --index;
    return index;
}

/// Count code-point-like UTF-8 units using boundary traversal.
[[nodiscard]] inline std::size_t codepoint_count(std::string_view value) noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < value.size(); i = next_codepoint(value, i)) ++count;
    return count;
}

/// Copy at most `max_count` code points without splitting a boundary.
[[nodiscard]] inline std::string truncate_codepoints(std::string_view value, std::size_t max_count) {
    std::size_t i = 0;
    std::size_t count = 0;
    while (i < value.size() && count < max_count) {
        i = next_codepoint(value, i);
        ++count;
    }
    return std::string{value.substr(0, i)};
}

/// Produce single-line text: CR/LF/TAB become at most one separating space,
/// other ASCII control bytes are removed and remaining bytes are preserved.
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

/// Coarse navigation class. Non-ASCII leading bytes count as Word; ASCII
/// alphanumeric/underscore are Word and other non-space ASCII is Punctuation.
enum class CharClass { Space, Word, Punctuation };

/// Classify the code point beginning at/containing one byte offset.
[[nodiscard]] inline CharClass char_class_at(std::string_view value, std::size_t index) noexcept {
    index = clamp_boundary(value, index);
    if (index >= value.size()) return CharClass::Space;
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if (c >= 0x80u) return CharClass::Word;
    if (std::isspace(c)) return CharClass::Space;
    if (std::isalnum(c) || c == '_') return CharClass::Word;
    return CharClass::Punctuation;
}

/// Return the byte boundary for one backward word-navigation step.
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

/// Return the byte boundary for one forward word-navigation step.
[[nodiscard]] inline std::size_t next_word(std::string_view value, std::size_t cursor) noexcept {
    std::size_t i = clamp_boundary(value, std::min(cursor, value.size()));
    if (i >= value.size()) return value.size();
    const auto cls = char_class_at(value, i);
    while (i < value.size() && char_class_at(value, i) == cls) i = next_codepoint(value, i);
    while (i < value.size() && char_class_at(value, i) == CharClass::Space) i = next_codepoint(value, i);
    return i;
}

/// Return [begin,end) byte boundaries for the contiguous CharClass at index.
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

/// Return the byte offset of the current newline-delimited line start.
[[nodiscard]] inline std::size_t line_start(std::string_view value, std::size_t index) noexcept {
    std::size_t i = clamp_boundary(value, std::min(index, value.size()));
    while (i > 0) {
        const auto previous = previous_codepoint(value, i);
        if (value[previous] == '\n') break;
        i = previous;
    }
    return i;
}

/// Return the byte offset of the current line end (before newline when present).
[[nodiscard]] inline std::size_t line_end(std::string_view value, std::size_t index) noexcept {
    std::size_t i = clamp_boundary(value, std::min(index, value.size()));
    while (i < value.size() && value[i] != '\n') i = next_codepoint(value, i);
    return i;
}

/// Return the code-point column within the current line.
[[nodiscard]] inline std::size_t line_column(std::string_view value, std::size_t index) noexcept {
    const auto bounded = clamp_boundary(value, std::min(index, value.size()));
    const auto begin = line_start(value, bounded);
    return codepoint_count(value.substr(begin, bounded - begin));
}

/// Resolve a code-point column to a byte boundary inside [begin,end].
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

/// Granularity used by horizontal movement and deletion.
enum class TextMotion {
    Codepoint,
    Word,
    Document,
};

/// Platform-independent mutable UTF-8 editing model.
///
/// Cursor/anchor/composition positions are byte offsets clamped to UTF-8
/// boundaries. The model owns text, selection, pre-edit state and bounded
/// undo/redo history, but no platform editor, drawing or synchronization.
class TextEditModel {
public:
    /// Owned text/cursor/anchor checkpoint. Offsets are bytes in `text`.
    struct Snapshot {
        std::string text;
        std::size_t cursor{};
        std::size_t anchor{};

        [[nodiscard]] bool operator==(const Snapshot&) const = default;
    };

    /// Construct with initial text. max_length counts future inserted code
    /// points; zero means unlimited.
    explicit TextEditModel(std::string value = {}, std::size_t max_length = 0)
        : text_(std::move(value)),
          max_length_(max_length == 0 ? std::numeric_limits<std::size_t>::max() : max_length),
          cursor_(text_.size()),
          anchor_(text_.size()) {}

    /// Borrow current UTF-8 bytes; mutation may invalidate the reference.
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    /// Current insertion endpoint as a byte offset.
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    /// Opposite selection endpoint as a byte offset.
    [[nodiscard]] std::size_t anchor() const noexcept { return anchor_; }
    /// Code-point limit applied to future insertion/composition commits.
    [[nodiscard]] std::size_t max_length() const noexcept { return max_length_; }

    /// Whether cursor and anchor delimit a non-empty selection.
    [[nodiscard]] bool has_selection() const noexcept { return cursor_ != anchor_; }
    /// Lower byte endpoint of the selection.
    [[nodiscard]] std::size_t selection_begin() const noexcept { return std::min(cursor_, anchor_); }
    /// Upper byte endpoint of the selection.
    [[nodiscard]] std::size_t selection_end() const noexcept { return std::max(cursor_, anchor_); }
    /// Borrow the selected UTF-8 byte slice.
    [[nodiscard]] std::string_view selected_text() const noexcept {
        return std::string_view{text_}.substr(selection_begin(), selection_end() - selection_begin());
    }

    /// Whether an undo checkpoint exists.
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    /// Whether a redo checkpoint exists.
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }

    /// Whether an IME/pre-edit transaction is active.
    [[nodiscard]] bool composition_active() const noexcept { return composition_active_; }
    /// Borrow current pre-edit UTF-8 text.
    [[nodiscard]] const std::string& composition_text() const noexcept { return composition_text_; }
    /// Pre-edit cursor byte offset.
    [[nodiscard]] std::size_t composition_cursor_byte() const noexcept { return composition_cursor_byte_; }
    /// Pre-edit selection byte count from composition_cursor_byte().
    [[nodiscard]] std::size_t composition_selection_bytes() const noexcept { return composition_selection_bytes_; }

    /// Start/restart composition and snapshot base text/selection.
    void begin_composition() {
        composition_start_ = snapshot();
        composition_text_.clear();
        composition_cursor_byte_ = 0;
        composition_selection_bytes_ = 0;
        composition_active_ = true;
    }

    /// Replace pre-edit text and clamp its cursor/selection byte range.
    /// Starts composition automatically when inactive.
    void update_composition(std::string preedit, std::size_t cursor_byte, std::size_t selection_bytes) {
        if (!composition_active_) begin_composition();
        composition_text_ = std::move(preedit);
        composition_cursor_byte_ =
            text::clamp_boundary(composition_text_, std::min(cursor_byte, composition_text_.size()));

        const auto remaining = composition_text_.size() - composition_cursor_byte_;
        const auto requested = std::min(selection_bytes, remaining);
        const auto selection_end = text::clamp_boundary(
            composition_text_, composition_cursor_byte_ + requested);
        composition_selection_bytes_ = selection_end - composition_cursor_byte_;
    }

    /// Commit text over the composition-start selection.
    ///
    /// Returns false for inactive/stale composition, exhausted max length or
    /// when no committed text can be accepted.
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

    /// Cancel pre-edit state; when base text is unchanged, restore the
    /// composition-start cursor/anchor.
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

    /// Set the future insertion limit in code points; zero means unlimited.
    /// Existing text is not truncated.
    void set_max_length(std::size_t max_length) noexcept {
        max_length_ = max_length == 0 ? std::numeric_limits<std::size_t>::max() : max_length;
    }

    /// Replace all text. This explicit replacement does not enforce max_length.
    /// Selection is clamped/preserved by default and history is cleared by default.
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

    /// Drop every undo and redo checkpoint.
    void clear_history() noexcept {
        undo_.clear();
        redo_.clear();
    }

    /// Move to a clamped byte boundary; collapse selection unless extending.
    void move_to(std::size_t target, bool extend = false) noexcept {
        set_cursor(target, extend, true);
    }

    /// Set explicit clamped anchor/cursor byte endpoints.
    void select_range(std::size_t anchor, std::size_t cursor) noexcept {
        anchor_ = text::clamp_boundary(text_, std::min(anchor, text_.size()));
        cursor_ = text::clamp_boundary(text_, std::min(cursor, text_.size()));
        vertical_column_.reset();
    }

    /// Select the complete text.
    void select_all() noexcept {
        anchor_ = 0;
        cursor_ = text_.size();
        vertical_column_.reset();
    }

    /// Select the contiguous CharClass at one byte position.
    void select_word_at(std::size_t byte_index) noexcept {
        const auto [begin, end] = text::word_bounds(text_, byte_index);
        anchor_ = begin;
        cursor_ = end;
        vertical_column_.reset();
    }

    /// Collapse selection to its lower byte endpoint.
    void collapse_to_begin() noexcept { move_to(selection_begin()); }
    /// Collapse selection to its upper byte endpoint.
    void collapse_to_end() noexcept { move_to(selection_end()); }

    /// Move left by code point, word class or document boundary.
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

    /// Move right by code point, word class or document boundary.
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

    /// Move to current line start.
    void move_line_start(bool extend = false) noexcept {
        set_cursor(text::line_start(text_, cursor_), extend, true);
    }

    /// Move to current line end.
    void move_line_end(bool extend = false) noexcept {
        set_cursor(text::line_end(text_, cursor_), extend, true);
    }

    /// Move to previous line preserving a preferred code-point column.
    void move_up(bool extend = false) noexcept {
        move_vertical(false, extend);
    }

    /// Move to next line preserving a preferred code-point column.
    void move_down(bool extend = false) noexcept {
        move_vertical(true, extend);
    }

    /// Replace selection/insert at cursor, truncating incoming text to the
    /// remaining max-length code-point budget. Creates an undo checkpoint.
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

    /// Erase a non-empty selection and create an undo checkpoint.
    [[nodiscard]] bool erase_selection() {
        if (!has_selection()) return false;
        if (composition_active_) cancel_composition();
        checkpoint();
        erase_selection_untracked();
        return true;
    }

    /// Delete selection or content before cursor at the requested granularity.
    [[nodiscard]] bool backspace(TextMotion motion = TextMotion::Codepoint) {
        if (composition_active_) cancel_composition();
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

    /// Delete selection or content after cursor at the requested granularity.
    [[nodiscard]] bool delete_forward(TextMotion motion = TextMotion::Codepoint) {
        if (composition_active_) cancel_composition();
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

    /// Restore the previous owned text/cursor/anchor checkpoint.
    [[nodiscard]] bool undo() {
        if (composition_active_) cancel_composition();
        if (undo_.empty()) return false;
        redo_.push_back(snapshot());
        restore(undo_.back());
        undo_.pop_back();
        trim_history(redo_);
        return true;
    }

    /// Reapply one previously undone checkpoint.
    [[nodiscard]] bool redo() {
        if (composition_active_) cancel_composition();
        if (redo_.empty()) return false;
        undo_.push_back(snapshot());
        restore(redo_.back());
        redo_.pop_back();
        trim_history(undo_);
        return true;
    }

    /// Copy current text/cursor/anchor; transient composition state is omitted.
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
