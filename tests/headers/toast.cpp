#include <nativeui/toast.hpp>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<ui::Toast>);
static_assert(!std::is_move_constructible_v<ui::Toast>);
namespace {
[[maybe_unused]] void probe(ui::Toast &messages) {
    auto result = messages.show({.message = "Saved", .duration = std::chrono::milliseconds{500}});
    (void)messages.dismiss(result.handle);
    (void)result.handle.valid();
}
} // namespace
