#pragma once

#include "../view_geometry.hpp"

#include <nativeui/geometry.hpp>
#include <optional>

namespace ui::detail {

/// The physical IME geometry and activation state belong to one native view.
/// Updating the model is independent of platform timer/IME side effects.
class ViewTextInputState final {
public:
    struct Transition final {
        bool active_changed{};
        bool active{};
        Rect physical_area{};
        float physical_cursor_offset{};
    };

    [[nodiscard]] std::optional<Transition> update(
        bool active, Rect logical_area, float logical_cursor_offset,
        float scale) noexcept {
        if (!text_input_boundary_needs_update(
                active_, physical_area_, physical_cursor_offset_, active,
                logical_area, logical_cursor_offset, scale)) {
            return std::nullopt;
        }

        const bool changed = active_ != active;
        const auto scaled = scale_text_input_geometry(
            active ? logical_area : Rect{},
            active ? logical_cursor_offset : 0.0f, scale);
        active_ = active;
        physical_area_ = scaled.first;
        physical_cursor_offset_ = scaled.second;
        return Transition{changed, active, physical_area_, physical_cursor_offset_};
    }

private:
    bool active_{};
    Rect physical_area_{};
    float physical_cursor_offset_{};
};

} // namespace ui::detail
