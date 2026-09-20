#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/detail/interaction_observer.hpp>
#include <nativeui/detail/overlay_service.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/transient_presentation.hpp>
#include <nativeui/theme.hpp>
#if defined(NATIVEUI_ENABLE_INSPECTOR)
#include <nativeui/inspector.hpp>
#endif

#include <array>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ui {

namespace detail {
inline std::unique_ptr<Node> compile_node(Spec spec, NodeId& next_id, Node* parent);
struct DynamicReconcileFaultAccess;
struct T125DynamicFaultAccess;
} // namespace detail

class Dialog;
class UI;
struct TreeTestAccess;

class Tree {
public:
#include <nativeui/detail/tree_public.inc>
#if defined(NATIVEUI_ENABLE_INSPECTOR)
#include <nativeui/detail/tree_inspector_public.inc>
#endif
#include <nativeui/detail/tree_theme_public.inc>
private:
    friend class Dialog;
    friend class UI;
    friend struct TreeTestAccess;
    friend struct detail::T125DynamicFaultAccess;
    friend struct detail::DynamicReconcileFaultAccess;
#include <nativeui/detail/tree_theme_private.inc>
#include <nativeui/detail/tree_overlay.inc>
#include <nativeui/detail/tree_transient.inc>
#define availability_invalidator unsafe_availability_invalidator
#define paint_invalidator unsafe_paint_invalidator
#define layout_invalidator unsafe_layout_invalidator
#define focus_invalidator unsafe_focus_invalidator
#define ensure_layout ensure_layout_legacy
#define layout_node layout_node_legacy
#define mount_node mount_node_untracked
#define activate_node activate_node_untracked
#define deactivate_node deactivate_node_untracked
#define unmount_node unmount_node_untracked
#define sync_availability_inactive sync_availability_inactive_t130_impl
#define sync_availability_structure sync_availability_structure_t130_impl
#include <nativeui/detail/tree_layout.inc>
#undef sync_availability_structure
#undef sync_availability_inactive
#undef unmount_node
#undef deactivate_node
#undef activate_node
#undef mount_node
#undef layout_node
#undef ensure_layout
#undef focus_invalidator
#undef layout_invalidator
#undef paint_invalidator
#undef availability_invalidator
#include <nativeui/detail/tree_focus.inc>
#include <nativeui/detail/tree_input.inc>
#include <nativeui/detail/tree_t125_availability.inc>
#include <nativeui/detail/tree_t125_semantic.inc>
#include <nativeui/detail/tree_focus_group.inc>
#define register_dynamic_node unsafe_register_dynamic_node
#define queue_dynamic_mutation queue_dynamic_mutation_t130_impl
#define flush_dynamic_mutations flush_dynamic_mutations_t130_impl
#include <nativeui/detail/tree_dynamic.inc>
#undef flush_dynamic_mutations
#undef queue_dynamic_mutation
#undef register_dynamic_node
#include <nativeui/detail/tree_t125_dynamic.inc>
#define mount_node retained_mount_node_legacy
#define unmount_node retained_unmount_node_legacy
#include <nativeui/detail/tree_retained_invalidation.inc>
#undef unmount_node
#undef mount_node
#include <nativeui/detail/tree_lifecycle_transaction.inc>
#include <nativeui/detail/tree_layout_transaction.inc>

    // Keep the currently executing callback alive across re-entrant replacement
    // or clearing. Installing a non-empty handler may allocate, but dispatch only
    // copies shared ownership and therefore performs no heap allocation.
    class KeyDownHandlerSlot final {
    public:
        using Handler = std::function<EventResult(const InputEvent&)>;

        KeyDownHandlerSlot& operator=(Handler handler) {
            if (!handler) {
                handler_.reset();
                return *this;
            }

            auto replacement = std::make_shared<const Handler>(std::move(handler));
            handler_ = std::move(replacement);
            return *this;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(handler_);
        }

        EventResult operator()(const InputEvent& event) const {
            const auto handler = handler_;
            return handler ? (*handler)(event) : EventResult::Ignored;
        }

    private:
        std::shared_ptr<const Handler> handler_;
    };

    KeyDownHandlerSlot global_key_down_handler_;
};

#include <nativeui/detail/tree_compile.inc>

} // namespace ui
