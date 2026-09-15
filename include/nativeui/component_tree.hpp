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

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace ui {

namespace detail {
inline std::unique_ptr<Node> compile_node(Spec spec, NodeId& next_id, Node* parent);
} // namespace detail

class Dialog;
class UI;

class Tree {
public:
#include <nativeui/detail/tree_public.inc>
#if defined(NATIVEUI_ENABLE_INSPECTOR)
#include <nativeui/detail/tree_inspector_public.inc>
#endif
#include <nativeui/detail/tree_theme_public.inc>
private:
    // T131 Dialog transaction recovery needs to distinguish an active retained
    // dispatch from an outer safe checkpoint without exposing dispatch
    // bookkeeping through Tree's public API. Dialog remains UI-owned policy;
    // this friendship is only an internal coordination seam.
    friend class Dialog;
    friend class UI;
#include <nativeui/detail/tree_theme_private.inc>
#include <nativeui/detail/tree_overlay.inc>
#include <nativeui/detail/tree_transient.inc>
#define availability_invalidator unsafe_availability_invalidator
#define paint_invalidator unsafe_paint_invalidator
#define layout_invalidator unsafe_layout_invalidator
#define focus_invalidator unsafe_focus_invalidator
#define mount_node unsafe_mount_node
#define unmount_node unsafe_unmount_node
#include <nativeui/detail/tree_layout.inc>
#undef unmount_node
#undef mount_node
#undef focus_invalidator
#undef layout_invalidator
#undef paint_invalidator
#undef availability_invalidator
#include <nativeui/detail/tree_focus.inc>
#include <nativeui/detail/tree_input.inc>
#include <nativeui/detail/tree_focus_group.inc>
#define register_dynamic_node unsafe_register_dynamic_node
#include <nativeui/detail/tree_dynamic.inc>
#undef register_dynamic_node
#include <nativeui/detail/tree_retained_invalidation.inc>
};

#include <nativeui/detail/tree_compile.inc>

} // namespace ui
