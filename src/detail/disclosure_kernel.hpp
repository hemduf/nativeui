#pragma once

#include <nativeui/animation.hpp>
#include <nativeui/collapsible.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace ui::detail {

// One disclosure state per compiled UI; Accordion owns several and publishes
// their target flags together before invoking any retained invalidator.
struct DisclosureState {
    std::string title;
    CollapsibleStyle style;
    DisclosureContentPolicy policy{DisclosureContentPolicy::Retain};
    bool open{};
    bool enabled{true};
    float phase{};
    float cached_height{};
    bool focus_within{};
    bool mounted{};
    bool active{};
    bool allowed{true};
    bool bounds_nonempty{};
    std::uint64_t generation{};
    std::uint64_t transition_serial{};
    unsigned pending_effects{};
    Dispatcher dispatcher;
    std::unique_ptr<AnimationContext> animations;
    AnimationHandle animation;
    AnimationInvalidationTarget animation_target;
    std::function<void()> invalidate_layout;
    std::function<void()> invalidate_availability;
    std::function<void()> invalidate_structure;
    std::function<void()> request_focus;
    std::function<bool()> can_write;
    std::function<void(bool)> set_open;
    std::function<void()> retained_checkpoint;
    std::function<void()> mounted_callback;
    std::function<void()> unmounted_callback;
    const void* group_identity{};
    std::function<bool()> group_selected;
    std::function<void()> group_select;
    std::function<void(Key)> group_edge;
};

void validate_disclosure(std::string_view title, const CollapsibleStyle& style,
                         DisclosureContentPolicy policy);
void prepare_disclosure_change(const std::shared_ptr<DisclosureState>& state, bool next) noexcept;
void flush_disclosure_change(const std::shared_ptr<DisclosureState>& state);
Spec disclosure_spec(std::shared_ptr<DisclosureState> state,
                     std::shared_ptr<const Spec> content, std::shared_ptr<void> owner);

} // namespace ui::detail
