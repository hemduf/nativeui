#include <nativeui/edit.hpp>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<ui::EditSession<double>>);
