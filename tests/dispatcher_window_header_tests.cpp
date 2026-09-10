#include <nativeui/window.hpp>

#include <concepts>
#include <utility>

static_assert(std::same_as<decltype(std::declval<ui::StandaloneWindow&>().dispatcher()), ui::Dispatcher>);
static_assert(std::same_as<decltype(std::declval<const ui::StandaloneWindow&>().dispatcher()), ui::Dispatcher>);
static_assert(std::same_as<decltype(std::declval<ui::EmbeddedView&>().dispatcher()), ui::Dispatcher>);
static_assert(std::same_as<decltype(std::declval<const ui::EmbeddedView&>().dispatcher()), ui::Dispatcher>);

int main() {
    return 0;
}
