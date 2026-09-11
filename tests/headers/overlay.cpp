#include <nativeui/overlay.hpp>

#include <type_traits>

static_assert(std::is_same_v<decltype(ui::Node::focus_restore), ui::NodeId>,
              "T061 modal focus restoration must store NodeId, never raw Node*");

void nativeui_header_compile_overlay() {}
