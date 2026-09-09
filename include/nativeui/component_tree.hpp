#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/focus_group.hpp>

#include <optional>

namespace ui {

class Tree {
public:
#include <nativeui/detail/tree_public.inc>
private:
#include <nativeui/detail/tree_layout.inc>
#include <nativeui/detail/tree_focus.inc>
#include <nativeui/detail/tree_input.inc>
#include <nativeui/detail/tree_focus_group.inc>
};

#include <nativeui/detail/tree_compile.inc>

} // namespace ui
