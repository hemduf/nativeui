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
#include <optional>
#include <string>

namespace ui {

namespace detail {
inline std::unique_ptr<Node> compile_node(Spec spec, NodeId& next_id, Node* parent);
} // namespace detail

class UI;

class Tree {
public:
#include <nativeui/detail/tree_public.inc>
#if defined(NATIVEUI_ENABLE_INSPECTOR)
#include <nativeui/detail/tree_inspector_public.inc>
#endif
#include <nativeui/detail/tree_theme_public.inc>
private:
    friend class UI;
#include <nativeui/detail/tree_theme_private.inc>
#include <nativeui/detail/tree_overlay.inc>
#include <nativeui/detail/tree_transient.inc>
#include <nativeui/detail/tree_layout.inc>
#include <nativeui/detail/tree_focus.inc>
#include <nativeui/detail/tree_input.inc>
#include <nativeui/detail/tree_focus_group.inc>
#include <nativeui/detail/tree_dynamic.inc>
};

#include <nativeui/detail/tree_compile.inc>

} // namespace ui
