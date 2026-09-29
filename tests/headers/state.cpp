#include <nativeui/state.hpp>

#include <concepts>
#include <utility>

static_assert(std::copy_constructible<ui::Binding<int>>);
static_assert(std::movable<ui::Binding<int>>);
static_assert(!std::default_initializable<ui::Binding<int>>);

void nativeui_header_compile_state() {
    ui::State<int> state{1};
    auto binding = state.binding();
    auto copy = binding;
    auto moved = std::move(copy);
    auto subscription = moved.observe([](const int&) {});
    moved.set(2);

    (void)binding.valid();
    (void)binding.get();
    (void)subscription.active();
}
