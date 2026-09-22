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
struct DynamicFaultAccess;
} // namespace detail

class Dialog;
class UI;
struct TreeTestAccess;

class Tree {
public:
#include <nativeui/detail/tree_public.inc>
#include <nativeui/detail/tree_paint_culling_public.inc>
#if defined(NATIVEUI_ENABLE_INSPECTOR)
#include <nativeui/detail/tree_inspector_public.inc>
#endif
#include <nativeui/detail/tree_theme_public.inc>
private:
    friend class Dialog;
    friend class UI;
    friend struct TreeTestAccess;
    friend struct detail::DynamicFaultAccess;
    friend struct detail::DynamicReconcileFaultAccess;
#include <nativeui/detail/tree_theme_private.inc>
#include <nativeui/detail/tree_overlay.inc>
#include <nativeui/detail/tree_transient.inc>
#include <nativeui/detail/tree_layout.inc>
#include <nativeui/detail/tree_focus.inc>
#include <nativeui/detail/tree_input.inc>
#include <nativeui/detail/tree_availability_recovery.inc>
#include <nativeui/detail/tree_semantic_recovery.inc>
#include <nativeui/detail/tree_focus_group.inc>
#include <nativeui/detail/tree_dynamic.inc>
#include <nativeui/detail/tree_retained_invalidation.inc>
#include <nativeui/detail/tree_paint_culling_private.inc>
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
