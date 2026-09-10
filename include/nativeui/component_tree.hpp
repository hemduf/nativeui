#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/focus_group.hpp>

#include <limits>
#include <optional>

namespace ui {

namespace detail {
inline std::unique_ptr<Node> compile_node(Spec spec, NodeId& next_id, Node* parent);
} // namespace detail

class Tree {
public:
#include <nativeui/detail/tree_public.inc>
private:
#include <nativeui/detail/tree_layout.inc>
#include <nativeui/detail/tree_focus.inc>
#include <nativeui/detail/tree_input.inc>
#include <nativeui/detail/tree_focus_group.inc>
#include <nativeui/detail/tree_dynamic.inc>
};

#include <nativeui/detail/tree_compile.inc>

} // namespace ui
