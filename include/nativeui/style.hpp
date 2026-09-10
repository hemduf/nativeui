#pragma once

namespace ui {

/// Interaction branch shared by all T038 widget style resolvers. Focus,
/// selection/check state and read-only remain orthogonal flags on VisualState
/// and therefore do not erase the base interaction branch.
enum class InteractionVisualState {
    Normal,
    Hovered,
    Pressed,
    Disabled,
};

/// Backend-neutral logical/interaction snapshot consumed by typed widget style
/// resolvers. Widgets provide the flags they support; unsupported flags remain
/// false. No instance ownership or mutable global state is stored here.
struct VisualState {
    bool enabled{true};
    bool read_only{};
    bool hovered{};
    bool pressed{};
    bool focused{};
    bool selected{};
    bool checked{};

    [[nodiscard]] constexpr bool operator==(const VisualState&) const noexcept = default;
};

/// Fixed v1 interaction precedence: disabled > pressed > hovered > normal.
/// Orthogonal VisualState flags remain available to the caller for subsequent
/// typed style overlays.
[[nodiscard]] constexpr InteractionVisualState resolve_interaction_state(
    const VisualState& state) noexcept {
    if (!state.enabled) return InteractionVisualState::Disabled;
    if (state.pressed) return InteractionVisualState::Pressed;
    if (state.hovered) return InteractionVisualState::Hovered;
    return InteractionVisualState::Normal;
}

} // namespace ui
